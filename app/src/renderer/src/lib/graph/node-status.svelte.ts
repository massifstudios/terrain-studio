import type { EvaluateResult, NodeThumbnail } from '$shared/engine-protocol';
import {
	isThumbnailCurrent,
	retainedThumbnails,
	signaturesById,
	type CachedThumbnail
} from './thumbnail-cache';

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
	/// How far through its own work an evaluating node is, 0 to 1.
	fraction: number;
}

/// What the node editor draws on each node body: how fresh its result is, how
/// long it took, and the thumbnail the engine shaded for it. A node the last
/// evaluation had no reason to visit keeps the thumbnail it already had, for as
/// long as the engine's signature for it stands.
class NodeStatusStore {
	statuses = $state<Record<string, NodeStatus>>({});
	thumbnails = $state<Record<string, CachedThumbnail>>({});
	signatures = $state<Record<string, string>>({});

	statusOf(nodeId: string): NodeStatus {
		const found = this.statuses[nodeId];
		if (found) {
			return found;
		}
		if (this.isCurrent(nodeId)) {
			return { state: 'cached', milliseconds: 0, fraction: 1 };
		}
		return { state: 'dirty', milliseconds: 0, fraction: 0 };
	}

	thumbnailOf(nodeId: string): NodeThumbnail | null {
		const cached = this.thumbnails[nodeId];
		if (!cached) {
			return null;
		}
		return cached.image;
	}

	isCurrent(nodeId: string): boolean {
		return isThumbnailCurrent(this.thumbnails[nodeId], this.signatures[nodeId]);
	}

	markEvaluating(nodeId: string, fraction: number): void {
		this.statuses = {
			...this.statuses,
			[nodeId]: { state: 'evaluating', milliseconds: 0, fraction }
		};
	}

	markAllDirty(): void {
		this.statuses = {};
		this.signatures = {};
	}

	applyOutcome(outcome: EvaluateResult): void {
		const statuses: Record<string, NodeStatus> = {};
		for (const timing of outcome.timings) {
			statuses[timing.nodeId] = {
				state: stateForTiming(timing.servedFromCache),
				milliseconds: timing.milliseconds,
				fraction: 1
			};
		}
		for (const message of outcome.errors) {
			const nodeId = message.slice(0, message.indexOf(':'));
			if (nodeId) {
				statuses[nodeId] = { state: 'failed', milliseconds: 0, fraction: 0 };
			}
		}
		this.statuses = statuses;

		const signatures = signaturesById(outcome.signatures);
		this.thumbnails = retainedThumbnails(this.thumbnails, outcome.thumbnails, signatures);
		this.signatures = signatures;
	}
}

export const nodeStatusStore = new NodeStatusStore();
