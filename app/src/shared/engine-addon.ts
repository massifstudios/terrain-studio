/**
 * The native addon's surface, as both the engine host and the `massif` command
 * see it. Declared once, so a change in the bridge cannot leave one of them
 * describing a function that is no longer there.
 */

import type {
	ColorResult,
	ElevationModelDescription,
	EvaluateResult,
	FieldResult,
	PluginReport,
	PointsResult
} from './engine-protocol';
import type { GraphDocument } from './graph-document';

export interface AddonProgress {
	nodeId: string;
	typeName: string;
	fraction: number;
}

export interface AddonEvaluateOptions {
	threadCount?: number;
	isFinalBuild?: boolean;
	thumbnailResolution?: number;
}

/// The addon leaves out whichever kinds of result the evaluated port did not
/// produce, where the protocol carries an explicit absence instead.
export interface AddonEvaluateResult {
	field?: FieldResult;
	color?: ColorResult;
	points?: PointsResult;
	thumbnails: EvaluateResult['thumbnails'];
	signatures: EvaluateResult['signatures'];
	evaluatedNodeIds: string[];
	timings: EvaluateResult['timings'];
	errors: EvaluateResult['errors'];
	cancelled: boolean;
}

export interface EngineAddon {
	engineVersion(): string;
	nodeRegistryJson(): string;
	loadPlugins(directories: string[]): PluginReport[];
	evaluate(
		graph: GraphDocument,
		nodeId: string,
		outputPort: number,
		options: AddonEvaluateOptions,
		onProgress: (update: AddonProgress) => void
	): Promise<AddonEvaluateResult>;
	cancelEvaluation(): void;
	describeElevationModel(path: string): ElevationModelDescription;
}
