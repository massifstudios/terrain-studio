import type { ElevationModelDescription } from '$shared/engine-protocol';
import type { GraphDocument } from '$shared/graph-document';

/**
 * The placeholder graph the preview shows until the node editor lands: one
 * import, straight to the viewport. It is built from the registry's own node
 * type name and nothing else.
 */
export function demoRefinementGraph(
	demPath: string,
	description: ElevationModelDescription,
	resolution: number
): GraphDocument {
	return {
		settings: {
			resolution,
			worldSizeMeters: description.width * description.cellSizeMeters,
			seed: 1,
			georeference: description.georeference
		},
		nodes: [
			{
				id: 'source',
				typeName: 'ImportDEM',
				positionX: 0,
				positionY: 0,
				parameters: { path: demPath }
			}
		],
		edges: []
	};
}

export const demoPreviewNodeId = 'source';
