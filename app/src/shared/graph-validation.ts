/**
 * Checking a graph against the registry: that every node type is one the engine
 * offers, and that every edge joins ports that exist and carry the same type.
 *
 * A `.massifgraph` parses without a registry — the file format knows nothing
 * about node types — so this is the second half of reading one, and the half
 * that needs the engine to have reported its nodes.
 */

import type { GraphDocument, GraphEdgeDocument, RegistrySchema } from './graph-document';
import { findNodeSchema } from './registry-schema';

function unknownTypeComplaints(graph: GraphDocument, registry: RegistrySchema): string[] {
	const complaints: string[] = [];
	for (const node of graph.nodes) {
		if (findNodeSchema(registry, node.typeName) === null) {
			complaints.push(`${node.id}: no node type '${node.typeName}' — is a plugin missing?`);
		}
	}
	return complaints;
}

function describeEdgeFault(
	edge: GraphEdgeDocument,
	graph: GraphDocument,
	registry: RegistrySchema
): string | null {
	const from = graph.nodes.find((node) => node.id === edge.fromNodeId);
	const to = graph.nodes.find((node) => node.id === edge.toNodeId);
	if (!from || !to) {
		return `${edge.fromNodeId} -> ${edge.toNodeId}: an end of this edge is not in the graph`;
	}
	const output = findNodeSchema(registry, from.typeName)?.outputs[edge.fromPort];
	const input = findNodeSchema(registry, to.typeName)?.inputs[edge.toPort];
	if (!output || !input) {
		return `${edge.fromNodeId} -> ${edge.toNodeId}: this edge uses a port that does not exist`;
	}
	if (output.type !== input.type) {
		return `${edge.fromNodeId} -> ${edge.toNodeId}: ${output.type} cannot feed ${input.type}`;
	}
	return null;
}

function edgeComplaints(graph: GraphDocument, registry: RegistrySchema): string[] {
	const complaints: string[] = [];
	for (const edge of graph.edges) {
		const complaint = describeEdgeFault(edge, graph, registry);
		if (complaint !== null) {
			complaints.push(complaint);
		}
	}
	return complaints;
}

/// Everything wrong with the graph, in reading order, so one pass tells the
/// whole story rather than one fault at a time.
export function graphComplaints(graph: GraphDocument, registry: RegistrySchema): string[] {
	return [...unknownTypeComplaints(graph, registry), ...edgeComplaints(graph, registry)];
}
