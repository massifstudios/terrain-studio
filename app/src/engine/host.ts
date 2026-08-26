/**
 * Engine utilityProcess entry point. Owns the native addon and answers
 * requests arriving on the MessagePort shared with the renderer. A native
 * crash takes this process down and leaves the UI standing.
 */

import { createRequire } from 'node:module';
import type { MessagePortMain } from 'electron';
import type { EngineRequest, EngineResponse } from '../shared/engine-protocol';

interface EngineAddon {
	engineVersion(): string;
}

function loadAddon(): EngineAddon {
	const addonPath = process.env.TERRAIN_ENGINE_ADDON;
	if (!addonPath) {
		throw new Error('TERRAIN_ENGINE_ADDON is not set; the main process resolves the addon path');
	}
	return createRequire(__filename)(addonPath) as EngineAddon;
}

const addon = loadAddon();

function runCommand(request: EngineRequest): unknown {
	if (request.command === 'engineVersion') {
		return addon.engineVersion();
	}
	throw new Error(`Unknown engine command: ${request.command}`);
}

function describeError(error: unknown): string {
	if (error instanceof Error) {
		return error.message;
	}
	return String(error);
}

function handleRequest(port: MessagePortMain, request: EngineRequest): void {
	let response: EngineResponse;
	try {
		response = { requestId: request.requestId, status: 'ok', result: runCommand(request) };
	} catch (error) {
		response = { requestId: request.requestId, status: 'error', message: describeError(error) };
	}
	port.postMessage(response);
}

function attachPort(port: MessagePortMain): void {
	port.on('message', (event) => handleRequest(port, event.data as EngineRequest));
	port.start();
}

// One port per renderer connection; a reload asks for a fresh one.
process.parentPort.on('message', (event) => {
	event.ports.forEach(attachPort);
});

process.parentPort.postMessage({ status: 'ready' });
