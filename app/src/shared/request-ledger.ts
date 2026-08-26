import type { EngineResponse } from './engine-protocol';

interface PendingRequest {
	resolve(result: unknown): void;
	reject(error: Error): void;
}

export interface OpenRequest {
	requestId: number;
	result: Promise<unknown>;
}

/**
 * Matches engine responses to the requests that asked for them. Transport
 * agnostic: the caller owns the port and only reports what came back.
 */
export class RequestLedger {
	#nextRequestId = 1;
	#pending = new Map<number, PendingRequest>();

	get pendingCount(): number {
		return this.#pending.size;
	}

	open(): OpenRequest {
		const requestId = this.#nextRequestId;
		this.#nextRequestId += 1;

		const result = new Promise<unknown>((resolve, reject) => {
			this.#pending.set(requestId, { resolve, reject });
		});
		return { requestId, result };
	}

	settle(response: EngineResponse): void {
		const pending = this.#pending.get(response.requestId);
		if (!pending) {
			return;
		}
		this.#pending.delete(response.requestId);
		if (response.status === 'ok') {
			pending.resolve(response.result);
			return;
		}
		pending.reject(new Error(response.message));
	}

	/** The engine process died; nothing in flight will ever be answered. */
	failAll(reason: Error): void {
		const abandoned = [...this.#pending.values()];
		this.#pending.clear();
		abandoned.forEach((pending) => pending.reject(reason));
	}
}
