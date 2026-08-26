/**
 * A full-resolution run of every output node, which is the only evaluation
 * allowed to write files. Which it is, the engine decides from `isFinalBuild`;
 * nothing here can talk a node into exporting.
 *
 * Outputs are evaluated one at a time for the same reason the app does it:
 * they share almost all of their upstream work, the cache serves the second of
 * a pair in milliseconds, and two full-resolution branches at once would double
 * the peak memory for nothing.
 */

import type { AddonEvaluateOptions, AddonProgress, EngineAddon } from '$shared/engine-addon';
import type { EngineError } from '$shared/engine-protocol';
import type { GraphDocument, RegistrySchema } from '$shared/graph-document';
import { outputNodeIds } from '$shared/graph-operations';
import { countOf } from './plural';
import { requireProject } from './project';

export interface BuildRequest {
	path: string;
	resolution: number | null;
	nodeId: string | null;
	threadCount: number | null;
}

function describeError(error: EngineError): string {
	if (!error.nodeId) {
		return error.message;
	}
	return `${error.nodeId}: ${error.message}`;
}

/// The build resolution replaces the graph's own for this run only; the file
/// keeps the resolution it was authored at.
function buildDocument(graph: GraphDocument, resolution: number | null): GraphDocument {
	if (resolution === null) {
		return graph;
	}
	return { ...graph, settings: { ...graph.settings, resolution } };
}

function targetNodeIds(graph: GraphDocument, registry: RegistrySchema, nodeId: string | null): string[] {
	if (nodeId !== null) {
		return [nodeId];
	}
	const targets = outputNodeIds({ nodes: graph.nodes, edges: graph.edges }, registry);
	if (targets.length === 0) {
		throw new Error('This graph has no output node, so a build would write nothing.');
	}
	return targets;
}

function evaluateOptions(threadCount: number | null): AddonEvaluateOptions {
	if (threadCount === null) {
		return { isFinalBuild: true };
	}
	return { isFinalBuild: true, threadCount };
}

/// A progress line rewrites itself in place, which only means anything on a
/// terminal. Piped to a file it would be thousands of lines of noise, so there
/// it says nothing at all.
function reportProgress(update: AddonProgress): void {
	if (!process.stderr.isTTY) {
		return;
	}
	const percent = Math.round(update.fraction * 100);
	process.stderr.write(`\r  ${update.nodeId} (${update.typeName}) ${percent}%   `);
}

function clearProgress(): void {
	if (!process.stderr.isTTY) {
		return;
	}
	process.stderr.write('\r\u001b[K');
}

async function buildOne(
	addon: EngineAddon,
	graph: GraphDocument,
	nodeId: string,
	threadCount: number | null
): Promise<void> {
	console.error(`Building ${nodeId}`);
	const outcome = await addon.evaluate(
		graph,
		nodeId,
		0,
		evaluateOptions(threadCount),
		reportProgress
	);
	clearProgress();
	if (outcome.cancelled) {
		throw new Error(`The build of ${nodeId} was cancelled`);
	}
	if (outcome.errors.length > 0) {
		throw new Error(outcome.errors.map(describeError).join('\n'));
	}
}

/// Stops at the first failure rather than writing half a set of files.
export async function buildGraph(
	addon: EngineAddon,
	registry: RegistrySchema,
	request: BuildRequest
): Promise<void> {
	const project = requireProject(request.path);
	const graph = buildDocument(project.graph, request.resolution);
	const targets = targetNodeIds(graph, registry, request.nodeId);
	for (const nodeId of targets) {
		await buildOne(addon, graph, nodeId, request.threadCount);
	}
	console.error(`Built ${countOf(targets.length, 'output')} at ${graph.settings.resolution}`);
}
