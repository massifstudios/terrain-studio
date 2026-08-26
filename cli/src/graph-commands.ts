/**
 * Reading and rewriting the graph as text. The DSL is the whole editing
 * surface: a caller writes statements, this parses them against the registry
 * and lays out whatever is new, and the result is a `.massifgraph` the editor
 * opens without knowing where it came from.
 */

import { readFileSync } from 'node:fs';
import type { GraphProject } from '$shared/massifgraph';
import type { RegistrySchema } from '$shared/graph-document';
import { parseDsl, printDsl } from '$shared/graph-dsl';
import { laidOut } from '$shared/graph-layout';
import type { GraphContents } from '$shared/graph-operations';
import { findNodeSchema } from '$shared/registry-schema';
import { countOf } from './plural';
import { readProject, readStandardInput, requireProject, writeProject } from './project';

export function printGraph(registry: RegistrySchema, path: string): void {
	const project = requireProject(path);
	process.stdout.write(printDsl(project.graph, registry));
}

function readSource(fromPath: string | null): string {
	if (fromPath === null) {
		return readStandardInput();
	}
	return readFileSync(fromPath, 'utf8');
}

/// The preview node is a name in the same namespace, so a rewrite that drops it
/// drops the preview with it rather than leaving a dangling id behind.
function survivingPreviewNodeId(project: GraphProject, contents: GraphContents): string | null {
	const surviving = new Set(contents.nodes.map((node) => node.id));
	if (project.previewNodeId === null || !surviving.has(project.previewNodeId)) {
		return null;
	}
	return project.previewNodeId;
}

/**
 * Replaces the whole graph with what the text says, keeping the settings and
 * the position of every node the file already had. New nodes are laid out by
 * dependency depth, the same way the editor's code view places them.
 */
export function applyDsl(registry: RegistrySchema, path: string, fromPath: string | null): void {
	const project = readProject(path);
	const parsed = parseDsl(readSource(fromPath), registry);
	const nodes = laidOut(parsed, registry, project.graph.nodes);
	writeProject(path, {
		graph: { settings: project.graph.settings, nodes, edges: parsed.edges },
		previewNodeId: survivingPreviewNodeId(project, parsed)
	});
	console.error(
		`Wrote ${countOf(nodes.length, 'node')} and ${countOf(parsed.edges.length, 'edge')} to ${path}`
	);
}

function unknownTypeComplaints(project: GraphProject, registry: RegistrySchema): string[] {
	const complaints: string[] = [];
	for (const node of project.graph.nodes) {
		if (findNodeSchema(registry, node.typeName) === null) {
			complaints.push(`${node.id}: no node type '${node.typeName}' — is a plugin missing?`);
		}
	}
	return complaints;
}

function edgeComplaints(project: GraphProject, registry: RegistrySchema): string[] {
	const complaints: string[] = [];
	for (const edge of project.graph.edges) {
		const complaint = describeEdgeFault(edge, project, registry);
		if (complaint !== null) {
			complaints.push(complaint);
		}
	}
	return complaints;
}

function describeEdgeFault(
	edge: GraphProject['graph']['edges'][number],
	project: GraphProject,
	registry: RegistrySchema
): string | null {
	const from = project.graph.nodes.find((node) => node.id === edge.fromNodeId);
	const to = project.graph.nodes.find((node) => node.id === edge.toNodeId);
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

/// Throws rather than printing a verdict, so a shell reading the exit code and
/// a person reading the output learn the same thing.
export function validateGraph(registry: RegistrySchema, path: string): void {
	const project = requireProject(path);
	const complaints = [
		...unknownTypeComplaints(project, registry),
		...edgeComplaints(project, registry)
	];
	if (complaints.length > 0) {
		throw new Error(complaints.join('\n'));
	}
	console.error(`${path} is a valid graph of ${countOf(project.graph.nodes.length, 'node')}`);
}

export interface SettingsChange {
	resolution: number | null;
	seed: number | null;
	worldSizeMeters: number | null;
	previewNodeId: string | null;
}

function changed(existing: number, wanted: number | null): number {
	if (wanted === null) {
		return existing;
	}
	return wanted;
}

export function setSettings(path: string, change: SettingsChange): void {
	const project = requireProject(path);
	const settings = project.graph.settings;
	writeProject(path, {
		graph: {
			...project.graph,
			settings: {
				...settings,
				resolution: changed(settings.resolution, change.resolution),
				seed: changed(settings.seed, change.seed),
				worldSizeMeters: changed(settings.worldSizeMeters, change.worldSizeMeters)
			}
		},
		previewNodeId: previewAfter(project, change)
	});
	console.error(`Updated the settings in ${path}`);
}

function previewAfter(project: GraphProject, change: SettingsChange): string | null {
	if (change.previewNodeId === null) {
		return project.previewNodeId;
	}
	return change.previewNodeId;
}
