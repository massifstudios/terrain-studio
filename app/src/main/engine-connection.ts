/**
 * Supervises the engine utilityProcess and hands each renderer a MessagePort
 * straight to it, so result buffers never round-trip through the main process.
 */

import { delimiter, join } from 'node:path';
import { app, MessageChannelMain, utilityProcess, type UtilityProcess, type WebContents } from 'electron';
import type { EngineStatus } from '../shared/engine-protocol';

/// How long to wait before relaunching a crashed engine, and how many crashes
/// in a row are allowed before the app stops trying. A plugin that faults on
/// load would otherwise crash forever at full speed.
const restartDelayMilliseconds = 750;
const restartAttemptLimit = 4;

let engineProcess: UtilityProcess | null = null;
let engineStatus: EngineStatus = 'starting';
let shuttingDown = false;
let consecutiveCrashes = 0;
let restartTimer: ReturnType<typeof setTimeout> | null = null;
const statusListeners = new Set<WebContents>();
const connectedRenderers = new Set<WebContents>();

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

/// Asset directories the same way: the user's own strips first, then the
/// ones shipped with the app. In development the engine's source tree holds
/// the shipped set.
function resolveAssetPath(): string {
	const userDirectory = join(app.getPath('userData'), 'assets');
	if (app.isPackaged) {
		return [userDirectory, join(process.resourcesPath, 'assets')].join(delimiter);
	}
	return [userDirectory, join(app.getAppPath(), '..', 'engine', 'assets')].join(delimiter);
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
			TERRAIN_PLUGIN_PATH: resolvePluginPath(),
			TERRAIN_ASSET_PATH: resolveAssetPath()
		}
	});

	engineProcess.on('message', (message: { status?: string }) => {
		if (message.status !== 'ready') {
			return;
		}
		consecutiveCrashes = 0;
		setStatus('ready');
		reconnectRenderers();
	});

	// A deliberate shutdown also fires 'exit'; only an unasked-for one is a crash.
	engineProcess.on('exit', () => {
		engineProcess = null;
		if (!shuttingDown) {
			handleCrash();
		}
	});
}

function handleCrash(): void {
	consecutiveCrashes += 1;
	if (consecutiveCrashes > restartAttemptLimit) {
		setStatus('unavailable');
		return;
	}
	setStatus('crashed');
	restartTimer = setTimeout(restartEngine, restartDelayMilliseconds);
}

function restartEngine(): void {
	restartTimer = null;
	if (shuttingDown) {
		return;
	}
	startEngine();
}

/// A renderer's port died with the process it led to, so every window that had
/// one is handed a fresh one. The renderer adopts it and asks the engine to
/// describe itself again, which is how the registry and the plugin list come
/// back. A window that never asked for a port is left alone.
function reconnectRenderers(): void {
	for (const contents of connectedRenderers) {
		connectRenderer(contents);
	}
}

export function stopEngine(): void {
	shuttingDown = true;
	if (restartTimer !== null) {
		clearTimeout(restartTimer);
		restartTimer = null;
	}
	if (!engineProcess) {
		return;
	}
	engineProcess.kill();
	engineProcess = null;
}

export function trackStatus(contents: WebContents): void {
	statusListeners.add(contents);
	contents.once('destroyed', () => forget(contents));
	contents.send('engine:status', engineStatus);
}

function forget(contents: WebContents): void {
	statusListeners.delete(contents);
	connectedRenderers.delete(contents);
}

/// The renderer asks for a port once its listeners are up, which is the first
/// moment a status push is certain to land — one sent at window creation is
/// dropped, because the preload script has not run yet. A renderer that asks
/// while the engine is down is remembered anyway, so the next start reaches it.
export function connectRenderer(contents: WebContents): void {
	if (contents.isDestroyed()) {
		return;
	}
	connectedRenderers.add(contents);
	contents.send('engine:status', engineStatus);
	if (!engineProcess) {
		return;
	}
	const channel = new MessageChannelMain();
	engineProcess.postMessage(null, [channel.port1]);
	contents.postMessage('engine:port', null, [channel.port2]);
}
