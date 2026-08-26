/**
 * Reading and rewriting the graph. The `.massifgraph` JSON is the whole editing
 * surface: a caller writes the document, this checks it against the registry,
 * and the result is a file the editor opens without knowing where it came from.
 */

import { readFileSync } from 'node:fs';
import type { GraphProject } from '$shared/massifgraph';
import { parseGraph, serializeGraph } from '$shared/massifgraph';
import type { RegistrySchema } from '$shared/graph-document';
import { graphComplaints } from '$shared/graph-validation';
import { countOf } from './plural';
import { readStandardInput, requireProject, writeProject } from './project';

/// The file as the serializer writes it, so a graph printed and applied back is
/// the same bytes rather than merely the same meaning.
export function printGraph(path: string): void {
	process.stdout.write(serializeGraph(requireProject(path)));
}

function readSource(fromPath: string | null): string {
	if (fromPath === null) {
		return readStandardInput();
	}
	return readFileSync(fromPath, 'utf8');
}

function requireValidGraph(project: GraphProject, registry: RegistrySchema): void {
	const complaints = graphComplaints(project.graph, registry);
	if (complaints.length > 0) {
		throw new Error(complaints.join('\n'));
	}
}

/**
 * Replaces the whole file with the document the text holds — settings, node
 * positions and preview included. Nothing is carried over from what was there
 * before, so what the caller wrote is exactly what the editor opens.
 */
export function applyGraph(registry: RegistrySchema, path: string, fromPath: string | null): void {
	const project = parseGraph(readSource(fromPath));
	requireValidGraph(project, registry);
	writeProject(path, project);
	console.error(
		`Wrote ${countOf(project.graph.nodes.length, 'node')} and ` +
			`${countOf(project.graph.edges.length, 'edge')} to ${path}`
	);
}

/// Throws rather than printing a verdict, so a shell reading the exit code and
/// a person reading the output learn the same thing.
export function validateGraph(registry: RegistrySchema, path: string): void {
	const project = requireProject(path);
	requireValidGraph(project, registry);
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
