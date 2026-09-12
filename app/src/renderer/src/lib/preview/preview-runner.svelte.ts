import type { EvaluateResult, FieldResult } from '$shared/engine-protocol';
import type { GraphDocument } from '$shared/graph-document';
import { engineClient } from '$lib/engine-client.svelte';
import { graphStore } from '$lib/graph/graph-store.svelte';
import { nodeStatusStore } from '$lib/graph/node-status.svelte';
import { findNode } from '$shared/graph-operations';
import { materialLayers, type MaterialLayer } from '$shared/material-preview';
import { findNodeSchema } from '$shared/registry-schema';
import { previewState, type MaterialPreview } from './preview-state.svelte';

const debounceMilliseconds = 150;
const thumbnailResolution = 64;

/**
 * Turns graph edits into preview evaluations. Edits debounce, and a newer
 * request abandons whatever the last one was going to show — the engine is
 * told to cancel, and a late answer is dropped by its generation number.
 */
class PreviewRunner {
	isEvaluating = $state(false);
	/// Wall-clock cost of the last preview that reached the viewport, for the
	/// status bar. Null until one has.
	lastMilliseconds = $state<number | null>(null);

	#pending: ReturnType<typeof setTimeout> | null = null;
	#generation = 0;
	#requested: { graph: GraphDocument; nodeId: string | null } | null = null;
	#isPaused = false;

	/// The caller passes a snapshot so the reactive read happens where the edit
	/// was made, not later inside the debounce.
	request(graph: GraphDocument, nodeId: string | null): void {
		this.#requested = { graph, nodeId };
		if (this.#pending !== null) {
			clearTimeout(this.#pending);
		}
		this.#pending = setTimeout(() => this.#run(), debounceMilliseconds);
	}

	/// Held while a build runs: a build owns the engine, and a preview at 512
	/// alongside it would only evict the very results the build is filling the
	/// cache with. The last request is kept and runs once the build lets go.
	setPaused(paused: boolean): void {
		this.#isPaused = paused;
		if (paused || this.#requested === null) {
			return;
		}
		this.#pending = setTimeout(() => this.#run(), debounceMilliseconds);
	}

	/// The heightfield a mask or colour map is draped over: the first surface
	/// found walking upstream from the selected node.
	#nearestSurfaceNodeId(startNodeId: string): string | null {
		const pending = [startNodeId];
		const seen = new Set<string>();
		while (pending.length > 0) {
			const current = pending.shift() as string;
			if (seen.has(current)) {
				continue;
			}
			seen.add(current);

			const node = graphStore.findNode(current);
			if (!node) {
				continue;
			}
			const schema = findNodeSchema(engineClient.registry, node.typeName);
			const output = schema?.outputs[0];
			if (current !== startNodeId && output?.fieldSemantics === 'heightfield') {
				return current;
			}
			for (const edge of graphStore.edges) {
				if (edge.toNodeId === current) {
					pending.push(edge.fromNodeId);
				}
			}
		}
		return null;
	}

	/// Null when the engine went away mid-evaluation. Nothing is drawn and
	/// nothing throws; the crash is reported by the status indicator, and the
	/// next request will find a restarted engine.
	async #evaluate(graph: GraphDocument, nodeId: string): Promise<EvaluateResult | null> {
		try {
			return await engineClient.evaluate({ graph, nodeId, outputPort: 0, thumbnailResolution });
		} catch {
			return null;
		}
	}

	async #run(): Promise<void> {
		this.#pending = null;
		if (this.#isPaused) {
			return;
		}
		const requested = this.#requested;
		if (!engineClient.isReady || !requested || requested.nodeId === null) {
			previewState.field = null;
			previewState.overlay = null;
			previewState.painted = null;
			previewState.points = null;
			previewState.materials = null;
			return;
		}

		const generation = this.#generation + 1;
		this.#generation = generation;
		this.isEvaluating = true;

		const graph = requested.graph;
		const nodeId = requested.nodeId;
		const startedAt = performance.now();
		const outcome = await this.#evaluate(graph, nodeId);
		if (generation !== this.#generation) {
			return;
		}
		if (outcome === null) {
			this.isEvaluating = false;
			return;
		}

		nodeStatusStore.applyOutcome(outcome);
		await this.#show(graph, nodeId, outcome, generation);

		if (generation === this.#generation) {
			this.lastMilliseconds = performance.now() - startedAt;
			this.isEvaluating = false;
		}
	}

	async #show(
		graph: GraphDocument,
		nodeId: string,
		outcome: EvaluateResult,
		generation: number
	): Promise<void> {
		previewState.overlay = null;
		previewState.painted = null;
		previewState.points = null;
		previewState.materials = null;

		const isSurface = outcome.field !== null && outcome.field.semantics !== 'mask';
		if (isSurface) {
			previewState.field = outcome.field;
			return;
		}

		// A mask, a colour map and a point set all need ground under them, so
		// each is shown over the nearest surface upstream rather than on its own.
		previewState.painted = outcome.color;
		previewState.points = outcome.points;
		const materials = await this.#materialsFor(graph, nodeId, outcome);
		if (generation !== this.#generation) {
			return;
		}
		previewState.materials = materials;
		if (materials === null) {
			previewState.overlay = outcome.field;
		}

		const surfaceNodeId = this.#nearestSurfaceNodeId(nodeId);
		if (surfaceNodeId === null) {
			previewState.field = null;
			return;
		}
		const surface = await this.#evaluate(graph, surfaceNodeId);
		if (generation !== this.#generation || surface === null) {
			return;
		}
		previewState.field = surface.field;
	}

	/// A node with texture parameters shows as textures tiled over all of its
	/// mask outputs rather than as the first mask alone. The extra ports cost
	/// little: the engine has the node cached from the first evaluation.
	async #materialsFor(
		graph: GraphDocument,
		nodeId: string,
		outcome: EvaluateResult
	): Promise<MaterialPreview | null> {
		const node = findNode(graph.nodes, nodeId);
		if (!node) {
			return null;
		}
		const schema = findNodeSchema(engineClient.registry, node.typeName);
		if (!schema) {
			return null;
		}
		const layers = materialLayers(schema, node.parameters);
		if (layers.length === 0) {
			return null;
		}
		const weights: FieldResult[] = [];
		for (const layer of layers) {
			const field = await this.#layerField(graph, nodeId, layer, outcome);
			if (field === null) {
				return null;
			}
			weights.push(field);
		}
		return { weights, textureNames: layers.map((layer) => layer.textureName) };
	}

	async #layerField(
		graph: GraphDocument,
		nodeId: string,
		layer: MaterialLayer,
		outcome: EvaluateResult
	): Promise<FieldResult | null> {
		if (layer.outputPort === 0) {
			return outcome.field;
		}
		try {
			const result = await engineClient.evaluate({
				graph,
				nodeId,
				outputPort: layer.outputPort,
				thumbnailResolution
			});
			return result.field;
		} catch {
			return null;
		}
	}
}

export const previewRunner = new PreviewRunner();
