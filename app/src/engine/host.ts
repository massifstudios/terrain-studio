/**
 * Engine utilityProcess entry point. Owns the native addon and answers
 * requests arriving on the MessagePort shared with the renderer. A native
 * crash takes this process down and leaves the UI standing.
 */

import { createRequire } from 'node:module';
import { delimiter } from 'node:path';
import type { MessagePortMain } from 'electron';
import type {
	EngineRequest,
	EngineResponse,
	EvaluateRequest,
	ElevationModelDescription,
	EvaluateResult,
	ColorResult,
	FieldResult,
	PluginReport,
	PointsResult
} from '../shared/engine-protocol';
import type { GraphDocument } from '../shared/graph-document';

interface AddonProgress {
	nodeId: string;
	typeName: string;
	fraction: number;
}

interface AddonEvaluateResult {
	field?: FieldResult;
	color?: ColorResult;
	points?: PointsResult;
	thumbnails: EvaluateResult['thumbnails'];
	signatures: EvaluateResult['signatures'];
	evaluatedNodeIds: string[];
	timings: EvaluateResult['timings'];
	errors: string[];
	cancelled: boolean;
}

interface EngineAddon {
	engineVersion(): string;
	nodeRegistryJson(): string;
	loadPlugins(directories: string[]): PluginReport[];
	evaluate(
		graph: GraphDocument,
		nodeId: string,
		outputPort: number,
		options: { threadCount?: number; isFinalBuild?: boolean; thumbnailResolution?: number },
		onProgress: (update: AddonProgress) => void
	): Promise<AddonEvaluateResult>;
	cancelEvaluation(): void;
	describeElevationModel(path: string): ElevationModelDescription;
}

function loadAddon(): EngineAddon {
	const addonPath = process.env.TERRAIN_ENGINE_ADDON;
	if (!addonPath) {
		throw new Error('TERRAIN_ENGINE_ADDON is not set; the main process resolves the addon path');
	}
	return createRequire(__filename)(addonPath) as EngineAddon;
}

const addon = loadAddon();

/// Which directories hold plugins is the main process's decision, not this
/// one's: only it knows where the user's data lives and where a packaged app
/// keeps its resources.
function pluginDirectories(): string[] {
	const configured = process.env.TERRAIN_PLUGIN_PATH;
	if (!configured) {
		return [];
	}
	return configured.split(delimiter).filter((entry) => entry.length > 0);
}

let pluginReports: PluginReport[] = [];

/// Also written to the log, not only to the UI: a refused plugin is exactly
/// the kind of thing someone will go looking for in a terminal.
function reportPlugins(reports: PluginReport[]): void {
	for (const plugin of reports) {
		if (plugin.message) {
			console.warn(`Plugin ${plugin.path} ${plugin.message}`);
			continue;
		}
		console.log(`Plugin ${plugin.name} loaded ${plugin.nodeCount} nodes`);
	}
}

function rescanPlugins(): PluginReport[] {
	pluginReports = addon.loadPlugins(pluginDirectories());
	reportPlugins(pluginReports);
	return pluginReports;
}

/// The addon leaves out whichever kinds of result the evaluated port did not
/// produce, and the renderer would rather see an explicit absence.
function orNull<Value>(value: Value | undefined): Value | null {
	if (value === undefined) {
		return null;
	}
	return value;
}

async function runEvaluate(port: MessagePortMain, payload: EvaluateRequest): Promise<EvaluateResult> {
	const reportProgress = (update: AddonProgress): void => {
		port.postMessage({ event: 'progress', ...update });
	};

	const result = await addon.evaluate(
		payload.graph,
		payload.nodeId,
		payload.outputPort,
		{
			threadCount: payload.threadCount,
			isFinalBuild: payload.isFinalBuild,
			thumbnailResolution: payload.thumbnailResolution
		},
		reportProgress
	);

	return {
		field: orNull(result.field),
		color: orNull(result.color),
		points: orNull(result.points),
		thumbnails: result.thumbnails,
		signatures: result.signatures,
		evaluatedNodeIds: result.evaluatedNodeIds,
		timings: result.timings,
		errors: result.errors,
		cancelled: result.cancelled
	};
}

async function runCommand(port: MessagePortMain, request: EngineRequest): Promise<unknown> {
	if (request.command === 'engineVersion') {
		return addon.engineVersion();
	}
	if (request.command === 'nodeRegistry') {
		return JSON.parse(addon.nodeRegistryJson());
	}
	if (request.command === 'pluginReports') {
		return pluginReports;
	}
	if (request.command === 'rescanPlugins') {
		return rescanPlugins();
	}
	if (request.command === 'cancelEvaluation') {
		addon.cancelEvaluation();
		return null;
	}
	if (request.command === 'describeElevationModel') {
		return addon.describeElevationModel((request.payload as { path: string }).path);
	}
	if (request.command === 'evaluate') {
		return runEvaluate(port, request.payload as EvaluateRequest);
	}
	throw new Error(`Unknown engine command: ${request.command}`);
}

function describeError(error: unknown): string {
	if (error instanceof Error) {
		return error.message;
	}
	return String(error);
}

async function handleRequest(port: MessagePortMain, request: EngineRequest): Promise<void> {
	let response: EngineResponse;
	try {
		response = { requestId: request.requestId, status: 'ok', result: await runCommand(port, request) };
	} catch (error) {
		response = { requestId: request.requestId, status: 'error', message: describeError(error) };
	}
	// MessagePortMain transfers ports only; a Float32Array crosses as a
	// structured clone, which is one binary copy and never JSON.
	port.postMessage(response);
}

function attachPort(port: MessagePortMain): void {
	port.on('message', (event) => {
		handleRequest(port, event.data as EngineRequest);
	});
	port.start();
}

// One port per renderer connection; a reload asks for a fresh one.
process.parentPort.on('message', (event) => {
	event.ports.forEach(attachPort);
});

// Plugins register before anything asks for the registry, so a plugin's node
// is in the palette from the first frame.
rescanPlugins();

process.parentPort.postMessage({ status: 'ready' });
