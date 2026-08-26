/**
 * Message shapes carried over the MessagePort between the renderer and the
 * engine utilityProcess. Heightfield payloads travel as transferable
 * ArrayBuffers alongside these envelopes, never inside them.
 */

export type EngineCommandName = 'engineVersion';

export interface EngineRequest {
	requestId: number;
	command: EngineCommandName;
	payload?: unknown;
}

export interface EngineSuccess {
	requestId: number;
	status: 'ok';
	result: unknown;
}

export interface EngineFailure {
	requestId: number;
	status: 'error';
	message: string;
}

export type EngineResponse = EngineSuccess | EngineFailure;

export type EngineStatus = 'starting' | 'ready' | 'crashed';

/** Channel name of the `window.postMessage` hop that hands the port to the page. */
export const enginePortChannel = 'engine:port';
