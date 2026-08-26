/**
 * Graph edits as pure functions over plain arrays. The reactive store is a
 * thin wrapper around these, which keeps the rules — legal connections, cycle
 * refusal, id allocation — testable without a component tree.
 */

import type {
	GraphEdgeDocument,
	GraphNodeDocument,
	ParameterValue,
	RegistrySchema
} from './graph-document';
import { findNodeSchema } from './registry-schema';

export function findNode(
	nodes: GraphNodeDocument[],
	nodeId: string
): GraphNodeDocument | null {
	const found = nodes.find((node) => node.id === nodeId);
	if (!found) {
		return null;
	}
	return found;
}

/// Ids are stable and human-readable, because they are what a saved project
/// and the DSL both refer to.
export function nextNodeId(nodes: GraphNodeDocument[], typeName: string): string {
	const stem = typeName.charAt(0).toLowerCase() + typeName.slice(1);
	let index = 1;
	while (findNode(nodes, `${stem}${index}`)) {
		index += 1;
	}
	return `${stem}${index}`;
}

/// Whether `targetId` can be reached by following edges downstream from
/// `startId`. Used to refuse a connection that would close a loop.
export function reachesDownstream(
	edges: GraphEdgeDocument[],
	startId: string,
	targetId: string
): boolean {
	const pending = [startId];
	const seen = new Set<string>();
	while (pending.length > 0) {
		const current = pending.pop() as string;
		if (current === targetId) {
			return true;
		}
		if (seen.has(current)) {
			continue;
		}
		seen.add(current);
		for (const edge of edges) {
			if (edge.fromNodeId === current) {
				pending.push(edge.toNodeId);
			}
		}
	}
	return false;
}

export interface ConnectionRequest {
	fromNodeId: string;
	fromPort: number;
	toNodeId: string;
	toPort: number;
}

export function canConnect(
	registry: RegistrySchema | null,
	nodes: GraphNodeDocument[],
	edges: GraphEdgeDocument[],
	request: ConnectionRequest
): boolean {
	if (request.fromNodeId === request.toNodeId) {
		return false;
	}
	const source = findNode(nodes, request.fromNodeId);
	const target = findNode(nodes, request.toNodeId);
	if (!source || !target) {
		return false;
	}
	const output = findNodeSchema(registry, source.typeName)?.outputs[request.fromPort];
	const input = findNodeSchema(registry, target.typeName)?.inputs[request.toPort];
	if (!output || !input || output.type !== input.type) {
		return false;
	}
	return !reachesDownstream(edges, request.toNodeId, request.fromNodeId);
}

/// An input port holds at most one edge, so connecting replaces.
export function withConnection(
	edges: GraphEdgeDocument[],
	request: ConnectionRequest
): GraphEdgeDocument[] {
	return [...withoutInput(edges, request.toNodeId, request.toPort), { ...request }];
}

export function withoutInput(
	edges: GraphEdgeDocument[],
	toNodeId: string,
	toPort: number
): GraphEdgeDocument[] {
	return edges.filter((edge) => edge.toNodeId !== toNodeId || edge.toPort !== toPort);
}

export interface GraphContents {
	nodes: GraphNodeDocument[];
	edges: GraphEdgeDocument[];
}

export function withoutNodes(contents: GraphContents, nodeIds: string[]): GraphContents {
	const removing = new Set(nodeIds);
	return {
		nodes: contents.nodes.filter((node) => !removing.has(node.id)),
		edges: contents.edges.filter(
			(edge) => !removing.has(edge.fromNodeId) && !removing.has(edge.toNodeId)
		)
	};
}

export function withMovedNodes(
	nodes: GraphNodeDocument[],
	nodeIds: string[],
	deltaX: number,
	deltaY: number
): GraphNodeDocument[] {
	const moving = new Set(nodeIds);
	return nodes.map((node) => {
		if (!moving.has(node.id)) {
			return node;
		}
		return { ...node, positionX: node.positionX + deltaX, positionY: node.positionY + deltaY };
	});
}

function dependenciesMet(
	edges: GraphEdgeDocument[],
	node: GraphNodeDocument,
	settled: Set<string>
): boolean {
	return edges
		.filter((edge) => edge.toNodeId === node.id)
		.every((edge) => settled.has(edge.fromNodeId));
}

/// Nodes ordered so that every node follows the ones feeding it. A graph with
/// a cycle in it still comes back whole, in an arbitrary order — reporting the
/// cycle is the engine's job, and a caller here must not hang on one.
export function inDependencyOrder(contents: GraphContents): GraphNodeDocument[] {
	const remaining = [...contents.nodes];
	const ordered: GraphNodeDocument[] = [];
	const settled = new Set<string>();
	while (remaining.length > 0) {
		const found = remaining.findIndex((node) => dependenciesMet(contents.edges, node, settled));
		const next = remaining.splice(Math.max(found, 0), 1)[0];
		ordered.push(next);
		settled.add(next.id);
	}
	return ordered;
}

export function withParameter(
	nodes: GraphNodeDocument[],
	nodeId: string,
	name: string,
	value: ParameterValue
): GraphNodeDocument[] {
	return nodes.map((node) => {
		if (node.id !== nodeId) {
			return node;
		}
		return { ...node, parameters: { ...node.parameters, [name]: value } };
	});
}
