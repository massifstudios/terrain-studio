import type { EvaluateResult } from '$shared/engine-protocol';
import type { GraphDocument } from '$shared/graph-document';
import { engineClient } from '$lib/engine-client.svelte';
import { graphStore } from '$lib/graph/graph-store.svelte';
import { nodeStatusStore } from '$lib/graph/node-status.svelte';
import { findNodeSchema } from '$shared/registry-schema';
import { previewState } from './preview-state.svelte';

const debounceMilliseconds = 150;
const thumbnailResolution = 64;

/**
 * Turns graph edits into preview evaluations. Edits debounce, and a newer
 * request abandons whatever the last one was going to show — the engine is
 * told to cancel, and a late answer is dropped by its generation number.
 */
class PreviewRunner {
	isEvaluating = $state(false);

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
			return;
		}

		const generation = this.#generation + 1;
		this.#generation = generation;
		this.isEvaluating = true;

		const graph = requested.graph;
		const nodeId = requested.nodeId;
		const outcome = await this.#evaluate(graph, nodeId);
		if (generation !== this.#generation) {
			return;
		}
		if (outcome === null) {
			this.isEvaluating = false;
			return;
		}

		nodeStatusStore.applyOutcome(outcome);
		previewState.errors = outcome.errors;
		await this.#show(graph, nodeId, outcome, generation);

		if (generation === this.#generation) {
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

		const isSurface = outcome.field !== null && outcome.field.semantics !== 'mask';
		if (isSurface) {
			previewState.field = outcome.field;
			return;
		}

		// A mask, a colour map and a point set all need ground under them, so
		// each is shown over the nearest surface upstream rather than on its own.
		previewState.overlay = outcome.field;
		previewState.painted = outcome.color;
		previewState.points = outcome.points;

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
}

export const previewRunner = new PreviewRunner();
