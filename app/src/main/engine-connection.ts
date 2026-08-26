/**
 * Supervises the engine utilityProcess and hands each renderer a MessagePort
 * straight to it, so result buffers never round-trip through the main process.
 */

import { delimiter, join } from 'node:path';
import { app, MessageChannelMain, utilityProcess, type UtilityProcess, type WebContents } from 'electron';
import type { EngineStatus } from '../shared/engine-protocol';

let engineProcess: UtilityProcess | null = null;
let engineStatus: EngineStatus = 'starting';
let shuttingDown = false;
const statusListeners = new Set<WebContents>();

function resolveAddonPath(): string {
	const override = process.env.TERRAIN_ENGINE_ADDON;
	if (override) {
		return override;
	}
	if (app.isPackaged) {
		return join(process.resourcesPath, 'engine.node');
	}
	return join(app.getAppPath(), '..', 'bridge', 'build', 'Release', 'engine.node');
}

/// Two plugin directories: one belonging to the user, one shipped beside the
/// app. The engine host is told where they are rather than working it out —
/// only the main process knows either path.
function resolvePluginPath(): string {
	const userDirectory = join(app.getPath('userData'), 'plugins');
	if (app.isPackaged) {
		return [userDirectory, join(process.resourcesPath, 'plugins')].join(delimiter);
	}
	return [userDirectory, join(app.getAppPath(), '..', 'plugins')].join(delimiter);
}

function setStatus(status: EngineStatus): void {
	engineStatus = status;
	for (const contents of statusListeners) {
		if (!contents.isDestroyed()) {
			contents.send('engine:status', status);
		}
	}
}

export function startEngine(): void {
	const hostPath = join(__dirname, 'engine-host.mjs');
	setStatus('starting');

	engineProcess = utilityProcess.fork(hostPath, [], {
		serviceName: 'terrain-engine',
		stdio: 'inherit',
		env: {
			...process.env,
			TERRAIN_ENGINE_ADDON: resolveAddonPath(),
			TERRAIN_PLUGIN_PATH: resolvePluginPath()
		}
	});

	engineProcess.on('message', (message: { status?: string }) => {
		if (message.status === 'ready') {
			setStatus('ready');
		}
	});

	// A deliberate shutdown also fires 'exit'; only an unasked-for one is a crash.
	engineProcess.on('exit', () => {
		engineProcess = null;
		if (!shuttingDown) {
			setStatus('crashed');
		}
	});
}

export function stopEngine(): void {
	shuttingDown = true;
	if (!engineProcess) {
		return;
	}
	engineProcess.kill();
	engineProcess = null;
}

export function trackStatus(contents: WebContents): void {
	statusListeners.add(contents);
	contents.once('destroyed', () => statusListeners.delete(contents));
	contents.send('engine:status', engineStatus);
}

export function connectRenderer(contents: WebContents): void {
	if (!engineProcess) {
		return;
	}
	const channel = new MessageChannelMain();
	engineProcess.postMessage(null, [channel.port1]);
	contents.postMessage('engine:port', null, [channel.port2]);
}
