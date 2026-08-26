/**
 * Message shapes carried over the MessagePort between the renderer and the
 * engine utilityProcess. Field samples ride along as a Float32Array, which
 * the structured clone copies as binary — never as JSON.
 */

import type { GraphDocument, GraphGeoReference } from './graph-document';

export type EngineCommandName =
	| 'engineVersion'
	| 'nodeRegistry'
	| 'evaluate'
	| 'cancelEvaluation'
	| 'describeElevationModel';

export interface EvaluateRequest {
	graph: GraphDocument;
	nodeId: string;
	outputPort: number;
	isFinalBuild?: boolean;
	threadCount?: number;
}

/** Header-only description of an elevation model on disk. */
export interface ElevationModelDescription {
	width: number;
	height: number;
	cellSizeMeters: number;
	minimumAltitudeMeters: number;
	maximumAltitudeMeters: number;
	label: string;
	georeference: GraphGeoReference;
}

export interface NodeTiming {
	nodeId: string;
	milliseconds: number;
	servedFromCache: boolean;
}

/** One evaluated output port, ready for the viewport. */
export interface FieldResult {
	samples: Float32Array;
	width: number;
	height: number;
	cellSizeMeters: number;
	minimum: number;
	maximum: number;
	semantics: 'heightfield' | 'mask';
}

export interface EvaluateResult {
	field: FieldResult | null;
	evaluatedNodeIds: string[];
	timings: NodeTiming[];
	errors: string[];
	cancelled: boolean;
}

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

/** Sent while an evaluation runs; not a reply to any one request. */
export interface EngineProgressEvent {
	event: 'progress';
	nodeId: string;
	typeName: string;
	fraction: number;
}

export type EngineOutbound = EngineResponse | EngineProgressEvent;

export function isProgressEvent(message: EngineOutbound): message is EngineProgressEvent {
	return (message as EngineProgressEvent).event === 'progress';
}

export type EngineStatus = 'starting' | 'ready' | 'crashed';

/** Channel name of the `window.postMessage` hop that hands the port to the page. */
export const enginePortChannel = 'engine:port';
