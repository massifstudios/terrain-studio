import {
	enginePortChannel,
	isProgressEvent,
	type EngineCommandName,
	type EngineOutbound,
	type EngineProgressEvent,
	type EngineStatus,
	type EvaluateRequest,
	type EvaluateResult,
	type PluginReport
} from '$shared/engine-protocol';
import type { RegistrySchema } from '$shared/graph-document';
import { RequestLedger } from '$shared/request-ledger';

/**
 * Renderer end of the engine link: one MessagePort straight to the engine
 * utilityProcess, with requests matched to responses by the ledger.
 */
class EngineClient {
	status = $state<EngineStatus>('starting');
	version = $state<string | null>(null);
	registry = $state<RegistrySchema | null>(null);
	plugins = $state<PluginReport[]>([]);
	progress = $state<EngineProgressEvent | null>(null);

	#port: MessagePort | null = null;
	#ledger = new RequestLedger();

	connect(): void {
		window.engineBridge.onStatus((status) => this.#handleStatus(status));
		window.addEventListener('message', (event) => this.#handleWindowMessage(event));
		window.engineBridge.requestPort();
	}

	get isReady(): boolean {
		return this.#port !== null;
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

	/// Starting a new preview evaluation abandons the one before it.
	async evaluate(request: EvaluateRequest): Promise<EvaluateResult> {
		await this.send('cancelEvaluation');
		const result = (await this.send('evaluate', request)) as EvaluateResult;
		this.progress = null;
		return result;
	}

	/// Looks for plugin libraries again. Anything already loaded stays loaded —
	/// the registry holds function pointers into it.
	async rescanPlugins(): Promise<void> {
		this.plugins = (await this.send('rescanPlugins')) as PluginReport[];
		this.registry = (await this.send('nodeRegistry')) as RegistrySchema;
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
		this.registry = null;
		this.plugins = [];
		this.progress = null;
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
		port.onmessage = (event) => this.#receive(event.data as EngineOutbound);
		port.start();
		this.#loadDescription();
	}

	#receive(message: EngineOutbound): void {
		if (isProgressEvent(message)) {
			this.progress = message;
			return;
		}
		this.#ledger.settle(message);
	}

	async #loadDescription(): Promise<void> {
		this.version = String(await this.send('engineVersion'));
		this.plugins = (await this.send('pluginReports')) as PluginReport[];
		this.registry = (await this.send('nodeRegistry')) as RegistrySchema;
	}
}

export const engineClient = new EngineClient();
