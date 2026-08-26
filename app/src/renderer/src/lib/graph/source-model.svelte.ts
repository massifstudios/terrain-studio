import type { ElevationModelDescription } from '$shared/engine-protocol';
import type { GraphNodeDocument, RegistrySchema } from '$shared/graph-document';
import { fileSourceNodeIds } from '$shared/graph-operations';
import type { GraphProject } from '$shared/massifgraph';
import { engineClient } from '$lib/engine-client.svelte';
import { graphStore } from './graph-store.svelte';

/**
 * Opening an elevation model sets the project up around it: the ground it
 * covers, and the survey coordinates it was cut from. Both ride through every
 * node untouched and land in the exported file, which is what lets a refined
 * terrain go back exactly where the original stood.
 *
 * Only the moment a different file is chosen. After that the world size is
 * yours to change, and rereading the header would keep overwriting it.
 */
class SourceModel {
	#lastPath: string | null = null;

	async follow(nodes: GraphNodeDocument[], registry: RegistrySchema | null): Promise<void> {
		const path = firstSourcePath(nodes, registry);
		if (path === null || path === this.#lastPath) {
			return;
		}
		this.#lastPath = path;

		const described = await describe(path);
		if (described === null) {
			return;
		}
		adopt(described);
	}

	/// A loaded project brings its own world size and survey coordinates, and
	/// the file its source names has not just been chosen. Recognising that
	/// source cannot wait for the registry, so it goes by shape alone: a node
	/// with a path and nothing feeding it.
	rememberLoadedProject(project: GraphProject): void {
		const fed = new Set(project.graph.edges.map((edge) => edge.toNodeId));
		const sources = project.graph.nodes.filter((node) => !fed.has(node.id));
		this.#lastPath = firstPathAmong(sources);
	}
}

function firstPathAmong(nodes: GraphNodeDocument[]): string | null {
	for (const node of nodes) {
		const path = node.parameters.path;
		if (typeof path === 'string' && path.length > 0) {
			return path;
		}
	}
	return null;
}

function firstSourcePath(
	nodes: GraphNodeDocument[],
	registry: RegistrySchema | null
): string | null {
	const sources = new Set(fileSourceNodeIds(nodes, registry));
	return firstPathAmong(nodes.filter((node) => sources.has(node.id)));
}

async function describe(path: string): Promise<ElevationModelDescription | null> {
	try {
		return (await engineClient.send('describeElevationModel', {
			path
		})) as ElevationModelDescription;
	} catch {
		// An unreadable file is the node's problem to report, not a reason to
		// disturb the settings the project already has.
		return null;
	}
}

function adopt(described: ElevationModelDescription): void {
	graphStore.settings = {
		...graphStore.settings,
		worldSizeMeters: described.width * described.cellSizeMeters,
		georeference: described.georeference
	};
}

export const sourceModel = new SourceModel();
