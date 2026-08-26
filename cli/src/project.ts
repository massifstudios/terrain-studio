/**
 * Reading and writing the `.massifgraph` file, the same serializer the editor
 * uses. A graph the editor has open and a graph this command wrote are the same
 * file, sorted the same way, so the two never fight over formatting.
 */

import { existsSync, readFileSync, writeFileSync } from 'node:fs';
import { emptyGraphSettings } from '$shared/graph-document';
import { parseGraph, serializeGraph, type GraphProject } from '$shared/massifgraph';

export function emptyProject(): GraphProject {
	return {
		graph: { settings: emptyGraphSettings(), nodes: [], edges: [] },
		previewNodeId: null
	};
}

/// A file that is not there yet reads as an empty project, so applying a first
/// graph makes one rather than demanding the file be created first.
export function readProject(path: string): GraphProject {
	if (!existsSync(path)) {
		return emptyProject();
	}
	return parseGraph(readFileSync(path, 'utf8'));
}

export function requireProject(path: string): GraphProject {
	if (!existsSync(path)) {
		throw new Error(`There is no graph at ${path}`);
	}
	return parseGraph(readFileSync(path, 'utf8'));
}

export function writeProject(path: string, project: GraphProject): void {
	writeFileSync(path, serializeGraph(project), 'utf8');
}

export function readStandardInput(): string {
	return readFileSync(0, 'utf8');
}
