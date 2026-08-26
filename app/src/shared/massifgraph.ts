/**
 * The `.massifgraph` project file: JSON, human-diffable, stable key order.
 *
 * Node types are not enumerated here. A file records whatever type name the
 * registry offered, so a graph saved with a plugin's node still loads — the
 * placeholder handling for a missing plugin lives with the registry, not here.
 */

import type {
	GraphDocument,
	GraphEdgeDocument,
	GraphNodeDocument,
	GraphSettings,
	ParameterValue
} from './graph-document';

export const massifGraphFormatVersion = 1;

export interface GraphProject {
	graph: GraphDocument;
	previewNodeId: string | null;
}

function orderedParameters(parameters: Record<string, ParameterValue>): Record<string, ParameterValue> {
	const ordered: Record<string, ParameterValue> = {};
	for (const name of Object.keys(parameters).sort()) {
		ordered[name] = parameters[name];
	}
	return ordered;
}

function orderedNode(node: GraphNodeDocument): GraphNodeDocument {
	return {
		id: node.id,
		typeName: node.typeName,
		positionX: node.positionX,
		positionY: node.positionY,
		parameters: orderedParameters(node.parameters)
	};
}

function orderedEdge(edge: GraphEdgeDocument): GraphEdgeDocument {
	return {
		fromNodeId: edge.fromNodeId,
		fromPort: edge.fromPort,
		toNodeId: edge.toNodeId,
		toPort: edge.toPort
	};
}

function orderedSettings(settings: GraphSettings): GraphSettings {
	return {
		resolution: settings.resolution,
		worldSizeMeters: settings.worldSizeMeters,
		seed: settings.seed,
		georeference: {
			coordinateSystem: settings.georeference.coordinateSystem,
			eastingMinMeters: settings.georeference.eastingMinMeters,
			northingMaxMeters: settings.georeference.northingMaxMeters
		}
	};
}

function compareEdges(left: GraphEdgeDocument, right: GraphEdgeDocument): number {
	if (left.toNodeId !== right.toNodeId) {
		return left.toNodeId.localeCompare(right.toNodeId);
	}
	if (left.toPort !== right.toPort) {
		return left.toPort - right.toPort;
	}
	if (left.fromNodeId !== right.fromNodeId) {
		return left.fromNodeId.localeCompare(right.fromNodeId);
	}
	return left.fromPort - right.fromPort;
}

/// Sorted so that moving a node in the editor does not reorder the file.
export function serializeGraph(project: GraphProject): string {
	const nodes = [...project.graph.nodes]
		.sort((left, right) => left.id.localeCompare(right.id))
		.map(orderedNode);
	const edges = [...project.graph.edges].sort(compareEdges).map(orderedEdge);

	return `${JSON.stringify(
		{
			formatVersion: massifGraphFormatVersion,
			settings: orderedSettings(project.graph.settings),
			previewNodeId: project.previewNodeId,
			nodes,
			edges
		},
		null,
		'\t'
	)}\n`;
}

function requireArray(value: unknown, what: string): unknown[] {
	if (!Array.isArray(value)) {
		throw new Error(`A .massifgraph needs an array of ${what}`);
	}
	return value;
}

export function parseGraph(text: string): GraphProject {
	const parsed = JSON.parse(text) as Record<string, unknown>;
	const formatVersion = parsed.formatVersion;
	if (formatVersion !== massifGraphFormatVersion) {
		throw new Error(`Unsupported .massifgraph format version ${String(formatVersion)}`);
	}

	const nodes = requireArray(parsed.nodes, 'nodes').map((entry) =>
		orderedNode(entry as GraphNodeDocument)
	);
	const edges = requireArray(parsed.edges, 'edges').map((entry) =>
		orderedEdge(entry as GraphEdgeDocument)
	);

	let previewNodeId: string | null = null;
	if (typeof parsed.previewNodeId === 'string') {
		previewNodeId = parsed.previewNodeId;
	}

	return {
		graph: {
			settings: orderedSettings(parsed.settings as GraphSettings),
			nodes,
			edges
		},
		previewNodeId
	};
}
