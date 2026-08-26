/**
 * Positions for nodes that arrived without any: a text edit adds nodes the
 * editor has never placed. A node the editor already knows keeps exactly where
 * it was put, so rewriting a line never rearranges the graph around it.
 */

import type { GraphNodeDocument, RegistrySchema } from './graph-document';
import { inDependencyOrder, type GraphContents } from './graph-operations';
import { findNodeSchema } from './registry-schema';
import { nodeHeight, nodeWidth } from './graph-geometry';

const columnGap = 72;
const rowGap = 28;

/// How far downstream a node sits, which is the column it belongs in.
function columnIndices(contents: GraphContents): Map<string, number> {
	const columns = new Map<string, number>();
	for (const node of inDependencyOrder(contents)) {
		const upstream = contents.edges
			.filter((edge) => edge.toNodeId === node.id)
			.map((edge) => columns.get(edge.fromNodeId));
		columns.set(node.id, 1 + Math.max(-1, ...upstream.filter((column) => column !== undefined)));
	}
	return columns;
}

function nextRow(rows: Map<number, number>, column: number): number {
	const existing = rows.get(column);
	if (existing === undefined) {
		return 0;
	}
	return existing;
}

export function laidOut(
	contents: GraphContents,
	registry: RegistrySchema | null,
	placed: GraphNodeDocument[]
): GraphNodeDocument[] {
	const columns = columnIndices(contents);
	const rows = new Map<number, number>();
	return contents.nodes.map((node) => {
		const column = columns.get(node.id) as number;
		const height = nodeHeight(findNodeSchema(registry, node.typeName)) + rowGap;
		const known = placed.find((entry) => entry.id === node.id);
		if (known) {
			rows.set(column, Math.max(nextRow(rows, column), known.positionY + height));
			return { ...node, positionX: known.positionX, positionY: known.positionY };
		}
		const top = nextRow(rows, column);
		rows.set(column, top + height);
		return { ...node, positionX: column * (nodeWidth + columnGap), positionY: top };
	});
}
