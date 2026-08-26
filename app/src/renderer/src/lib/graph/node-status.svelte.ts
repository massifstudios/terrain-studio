import type { EvaluateResult, NodeThumbnail } from '$shared/engine-protocol';

export type NodeState = 'dirty' | 'evaluating' | 'cached' | 'evaluated' | 'failed';

function stateForTiming(servedFromCache: boolean): NodeState {
	if (servedFromCache) {
		return 'cached';
	}
	return 'evaluated';
}

export interface NodeStatus {
	state: NodeState;
	milliseconds: number;
}

/// What the node editor draws on each node body: how fresh its result is, how
/// long it took, and the thumbnail the engine shaded for it.
class NodeStatusStore {
	statuses = $state<Record<string, NodeStatus>>({});
	thumbnails = $state<Record<string, NodeThumbnail>>({});

	statusOf(nodeId: string): NodeStatus {
		const found = this.statuses[nodeId];
		if (!found) {
			return { state: 'dirty', milliseconds: 0 };
		}
		return found;
	}

	markEvaluating(nodeId: string): void {
		this.statuses = { ...this.statuses, [nodeId]: { state: 'evaluating', milliseconds: 0 } };
	}

	markAllDirty(): void {
		this.statuses = {};
	}

	applyOutcome(outcome: EvaluateResult): void {
		const statuses: Record<string, NodeStatus> = {};
		for (const timing of outcome.timings) {
			statuses[timing.nodeId] = {
				state: stateForTiming(timing.servedFromCache),
				milliseconds: timing.milliseconds
			};
		}
		for (const message of outcome.errors) {
			const nodeId = message.slice(0, message.indexOf(':'));
			if (nodeId) {
				statuses[nodeId] = { state: 'failed', milliseconds: 0 };
			}
		}
		this.statuses = statuses;

		const thumbnails: Record<string, NodeThumbnail> = {};
		for (const thumbnail of outcome.thumbnails) {
			thumbnails[thumbnail.nodeId] = thumbnail;
		}
		this.thumbnails = thumbnails;
	}
}

export const nodeStatusStore = new NodeStatusStore();
