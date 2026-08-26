/**
 * Engine utilityProcess entry point. Owns the native addon and answers
 * requests arriving on the MessagePort shared with the renderer. A native
 * crash takes this process down and leaves the UI standing.
 */

import { createRequire } from 'node:module';
import type { MessagePortMain } from 'electron';
import type {
	EngineRequest,
	EngineResponse,
	EvaluateRequest,
	ElevationModelDescription,
	EvaluateResult,
	FieldResult
} from '../shared/engine-protocol';
import type { GraphDocument } from '../shared/graph-document';

interface AddonProgress {
	nodeId: string;
	typeName: string;
	fraction: number;
}

interface AddonEvaluateResult {
	samples?: Float32Array;
	width?: number;
	height?: number;
	cellSizeMeters?: number;
	minimum?: number;
	maximum?: number;
	semantics?: 'heightfield' | 'mask';
	evaluatedNodeIds: string[];
	timings: EvaluateResult['timings'];
	errors: string[];
	cancelled: boolean;
}

interface EngineAddon {
	engineVersion(): string;
	nodeRegistryJson(): string;
	evaluate(
		graph: GraphDocument,
		nodeId: string,
		outputPort: number,
		options: { threadCount?: number; isFinalBuild?: boolean },
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

function fieldFrom(result: AddonEvaluateResult): FieldResult | null {
	if (!result.samples) {
		return null;
	}
	return {
		samples: result.samples,
		width: result.width!,
		height: result.height!,
		cellSizeMeters: result.cellSizeMeters!,
		minimum: result.minimum!,
		maximum: result.maximum!,
		semantics: result.semantics!
	};
}

async function runEvaluate(port: MessagePortMain, payload: EvaluateRequest): Promise<EvaluateResult> {
	const reportProgress = (update: AddonProgress): void => {
		port.postMessage({ event: 'progress', ...update });
	};

	const result = await addon.evaluate(
		payload.graph,
		payload.nodeId,
		payload.outputPort,
		{ threadCount: payload.threadCount, isFinalBuild: payload.isFinalBuild },
		reportProgress
	);

	return {
		field: fieldFrom(result),
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

process.parentPort.postMessage({ status: 'ready' });
