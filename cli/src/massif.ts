#!/usr/bin/env bun
/**
 * `massif` — Terrain Studio's engine from a terminal.
 *
 * The same graph file and the same engine binary the app uses, with no window
 * in the way. Results go to standard output; everything a person
 * reads while a command runs goes to standard error, so a pipe carries only the
 * answer.
 */

import type { EngineAddon } from '$shared/engine-addon';
import type { RegistrySchema } from '$shared/graph-document';
import { buildGraph } from './build-command';
import { applyGraph, printGraph, setSettings, validateGraph } from './graph-commands';
import { openEngine, readRegistry } from './engine';
import { describeNode, listNodes } from './registry-commands';
import {
	UsageError,
	optionalNumber,
	optionalText,
	parseArguments,
	positionalAt,
	type ParsedArguments
} from './arguments';

const usage = `massif — Terrain Studio without a window

  massif nodes                     every node type the engine offers
  massif describe <TypeName>       one node's ports and parameters
  massif registry                  the whole registry as JSON

  massif print <graph>             the graph as .massifgraph JSON
  massif apply <graph> [--from <file>]
                                   replace the graph with JSON, stdin by default
  massif validate <graph>          check every type, edge and port
  massif set <graph> [--resolution N] [--seed N] [--world-size M]
                                   [--preview-node <id>]

  massif build <graph> [--resolution N] [--node <id>] [--threads N]
                                   evaluate the output nodes and write files

  massif version                   the engine's version

The engine binary comes from TERRAIN_ENGINE_ADDON, or from the repository's own
build. Plugin directories come from TERRAIN_PLUGIN_PATH, and asset directories
(satellite colour strips) from TERRAIN_ASSET_PATH, else the repository's own.
`;

/// Reads and writes the file without asking the registry anything, so these
/// work on a machine where the addon has not been built.
function runOfflineCommand(parsed: ParsedArguments): boolean {
	if (parsed.command === 'print') {
		printGraph(positionalAt(parsed, 0, 'a graph file'));
		return true;
	}
	if (parsed.command !== 'set') {
		return false;
	}
	setSettings(positionalAt(parsed, 0, 'a graph file'), {
		resolution: optionalNumber(parsed, 'resolution'),
		seed: optionalNumber(parsed, 'seed'),
		worldSizeMeters: optionalNumber(parsed, 'world-size'),
		previewNodeId: optionalText(parsed, 'preview-node')
	});
	return true;
}

function runRegistryCommand(parsed: ParsedArguments, registry: RegistrySchema): boolean {
	if (parsed.command === 'nodes') {
		listNodes(registry);
		return true;
	}
	if (parsed.command === 'describe') {
		describeNode(registry, positionalAt(parsed, 0, 'a node type name'));
		return true;
	}
	if (parsed.command === 'registry') {
		console.log(JSON.stringify(registry, null, '\t'));
		return true;
	}
	return false;
}

async function runGraphCommand(
	parsed: ParsedArguments,
	addon: EngineAddon,
	registry: RegistrySchema
): Promise<boolean> {
	if (parsed.command === 'apply') {
		applyGraph(registry, positionalAt(parsed, 0, 'a graph file'), optionalText(parsed, 'from'));
		return true;
	}
	if (parsed.command === 'validate') {
		validateGraph(registry, positionalAt(parsed, 0, 'a graph file'));
		return true;
	}
	if (parsed.command === 'build') {
		await buildGraph(addon, registry, {
			path: positionalAt(parsed, 0, 'a graph file'),
			resolution: optionalNumber(parsed, 'resolution'),
			nodeId: optionalText(parsed, 'node'),
			threadCount: optionalNumber(parsed, 'threads')
		});
		return true;
	}
	return false;
}

async function run(argv: string[]): Promise<void> {
	const parsed = parseArguments(argv);
	if (parsed.command === '' || parsed.command === 'help') {
		process.stdout.write(usage);
		return;
	}
	if (runOfflineCommand(parsed)) {
		return;
	}

	const addon = openEngine();
	if (parsed.command === 'version') {
		console.log(addon.engineVersion());
		return;
	}
	const registry = readRegistry(addon);
	if (runRegistryCommand(parsed, registry)) {
		return;
	}
	if (await runGraphCommand(parsed, addon, registry)) {
		return;
	}
	throw new UsageError(`Unknown command '${parsed.command}'`);
}

function describeFailure(error: unknown): string {
	if (error instanceof Error) {
		return error.message;
	}
	return String(error);
}

try {
	await run(process.argv.slice(2));
} catch (error) {
	console.error(describeFailure(error));
	if (error instanceof UsageError) {
		process.stderr.write(`\n${usage}`);
	}
	process.exit(1);
}
