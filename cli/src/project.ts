/**
 * Reading and writing the `.massifgraph` file, the same serializer the editor
 * uses. A graph the editor has open and a graph this command wrote are the same
 * file, sorted the same way, so the two never fight over formatting.
 */

import { existsSync, readFileSync, writeFileSync } from 'node:fs';
import { parseGraph, serializeGraph, type GraphProject } from '$shared/massifgraph';

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
