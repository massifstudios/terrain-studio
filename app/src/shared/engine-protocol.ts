/**
 * Message shapes carried over the MessagePort between the renderer and the
 * engine utilityProcess. Field samples ride along as a Float32Array, which
 * the structured clone copies as binary — never as JSON.
 */

import type { GraphDocument, GraphGeoReference } from './graph-document';

export type EngineCommandName =
	| 'engineVersion'
	| 'nodeRegistry'
	| 'pluginReports'
	| 'rescanPlugins'
	| 'evaluate'
	| 'cancelEvaluation'
	| 'describeElevationModel';

/**
 * What became of one plugin library the engine looked at. `message` is empty
 * only when it loaded cleanly and brought at least one node; a refusal always
 * carries a reason, because a plugin must never fail silently.
 */
export interface PluginReport {
	path: string;
	name: string;
	loaded: boolean;
	abiVersion: number;
	nodeCount: number;
	message: string;
}

export interface EvaluateRequest {
	graph: GraphDocument;
	nodeId: string;
	outputPort: number;
	isFinalBuild?: boolean;
	threadCount?: number;
	/// Square size of the per-node thumbnails the graph editor draws.
	thumbnailResolution?: number;
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

/** An evaluated colour port: three floats per cell, red green blue. */
export interface ColorResult {
	samples: Float32Array;
	width: number;
	height: number;
}

/**
 * An evaluated point port. Positions ride in one flat array of five values per
 * instance rather than an array of objects, because a forest is millions of
 * instances and a million small objects is a million allocations.
 */
export interface PointsResult {
	count: number;
	/// easting, northing, altitude, rotation, scale — in that order, per point.
	values: Float32Array;
	ids: BigUint64Array;
}

export const valuesPerPoint = 5;

/** One node's thumbnail, shaded by the engine and ready to blit. */
export interface NodeThumbnail {
	nodeId: string;
	size: number;
	rgba: Uint8Array;
}

export interface EvaluateResult {
	field: FieldResult | null;
	color: ColorResult | null;
	points: PointsResult | null;
	thumbnails: NodeThumbnail[];
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
