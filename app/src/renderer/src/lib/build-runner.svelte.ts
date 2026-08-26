import type { EngineError, EvaluateResult } from '$shared/engine-protocol';
import type { GraphDocument } from '$shared/graph-document';
import { outputNodeIds } from '$shared/graph-operations';
import { engineClient } from './engine-client.svelte';
import { graphStore } from './graph/graph-store.svelte';
import { previewRunner } from './preview/preview-runner.svelte';

export type BuildState = 'idle' | 'running' | 'finished' | 'failed' | 'cancelled';

/// The build badge is nowhere near the graph, so its errors carry the node they
/// belong to in the text.
function describeError(error: EngineError): string {
	if (!error.nodeId) {
		return error.message;
	}
	return `${error.nodeId}: ${error.message}`;
}

/**
 * A full-resolution run of every output node in the graph.
 *
 * Each is evaluated in turn rather than all at once: they share most of their
 * upstream work, and the cache means the second exporter of a pair pays for
 * almost none of it. A build is the only evaluation allowed to write files,
 * which the engine decides from `isFinalBuild`, not from anything here.
 */
class BuildRunner {
	state = $state<BuildState>('idle');
	completed = $state(0);
	total = $state(0);
	currentNodeId = $state<string | null>(null);
	errors = $state<string[]>([]);
	/// Wall-clock cost of the last build that ran to the end, for the status
	/// bar. Null until one has.
	lastMilliseconds = $state<number | null>(null);

	#isCancelling = false;
	#startedAt = 0;

	get isBuilding(): boolean {
		return this.state === 'running';
	}

	async start(): Promise<void> {
		if (this.isBuilding) {
			return;
		}
		const targets = this.#outputNodeIds();
		this.#begin(targets.length);
		if (targets.length === 0) {
			this.state = 'failed';
			this.errors = ['This graph has no output node, so a build would write nothing.'];
			return;
		}

		previewRunner.setPaused(true);
		await this.#runAll(targets, this.#buildDocument());
		previewRunner.setPaused(false);
	}

	async cancel(): Promise<void> {
		if (!this.isBuilding) {
			return;
		}
		this.#isCancelling = true;
		try {
			await engineClient.send('cancelEvaluation');
		} catch {
			// The engine is already gone, which the running build will report.
		}
	}

	#begin(total: number): void {
		this.state = 'running';
		this.completed = 0;
		this.total = total;
		this.currentNodeId = null;
		this.errors = [];
		this.#isCancelling = false;
		this.#startedAt = performance.now();
	}

	/// The build resolution replaces the preview one for this run only; it is a
	/// session preference, not something the project file carries.
	#buildDocument(): GraphDocument {
		const document = graphStore.document();
		return {
			...document,
			settings: { ...document.settings, resolution: graphStore.buildResolution }
		};
	}

	#outputNodeIds(): string[] {
		const contents = { nodes: graphStore.nodes, edges: graphStore.edges };
		return outputNodeIds(contents, engineClient.registry);
	}

	async #runAll(nodeIds: string[], graph: GraphDocument): Promise<void> {
		for (const nodeId of nodeIds) {
			if (this.#isCancelling) {
				this.state = 'cancelled';
				return;
			}
			this.currentNodeId = nodeId;
			const outcome = await this.#evaluate(graph, nodeId);
			if (!this.#accept(outcome)) {
				return;
			}
			this.completed += 1;
		}
		this.currentNodeId = null;
		this.lastMilliseconds = performance.now() - this.#startedAt;
		this.state = 'finished';
	}

	/// Whether the build should carry on. A cancelled or failed run stops at the
	/// first sign of trouble rather than writing half a set of files.
	#accept(outcome: EvaluateResult | null): boolean {
		if (outcome === null) {
			this.state = 'failed';
			this.errors = ['The engine stopped while building.'];
			return false;
		}
		if (outcome.cancelled || this.#isCancelling) {
			this.state = 'cancelled';
			return false;
		}
		if (outcome.errors.length > 0) {
			this.state = 'failed';
			this.errors = outcome.errors.map(describeError);
			return false;
		}
		return true;
	}

	async #evaluate(graph: GraphDocument, nodeId: string): Promise<EvaluateResult | null> {
		try {
			return await engineClient.evaluate({
				graph,
				nodeId,
				outputPort: 0,
				isFinalBuild: true
			});
		} catch {
			return null;
		}
	}
}

export const buildRunner = new BuildRunner();
