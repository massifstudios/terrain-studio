import {
	enginePortChannel,
	type EngineCommandName,
	type EngineResponse,
	type EngineStatus
} from '$shared/engine-protocol';
import { RequestLedger } from '$shared/request-ledger';

/**
 * Renderer end of the engine link: one MessagePort straight to the engine
 * utilityProcess, with requests matched to responses by the ledger.
 */
class EngineClient {
	status = $state<EngineStatus>('starting');
	version = $state<string | null>(null);

	#port: MessagePort | null = null;
	#ledger = new RequestLedger();

	connect(): void {
		window.engineBridge.onStatus((status) => this.#handleStatus(status));
		window.addEventListener('message', (event) => this.#handleWindowMessage(event));
		window.engineBridge.requestPort();
	}

	send(command: EngineCommandName, payload?: unknown): Promise<unknown> {
		const port = this.#port;
		if (!port) {
			return Promise.reject(new Error('Engine is not connected'));
		}
		const request = this.#ledger.open();
		port.postMessage({ requestId: request.requestId, command, payload });
		return request.result;
	}

	#handleStatus(status: EngineStatus): void {
		this.status = status;
		if (status === 'crashed') {
			this.#dropPort();
		}
	}

	#dropPort(): void {
		this.#port = null;
		this.version = null;
		this.#ledger.failAll(new Error('Engine process exited'));
	}

	#handleWindowMessage(event: MessageEvent): void {
		if (event.source !== window) {
			return;
		}
		if (event.data?.channel !== enginePortChannel) {
			return;
		}
		this.#adoptPort(event.ports[0]);
	}

	#adoptPort(port: MessagePort): void {
		this.#port = port;
		port.onmessage = (event) => this.#ledger.settle(event.data as EngineResponse);
		port.start();
		this.#refreshVersion();
	}

	async #refreshVersion(): Promise<void> {
		const version = await this.send('engineVersion');
		this.version = String(version);
	}
}

export const engineClient = new EngineClient();
