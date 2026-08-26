import type { EngineStatus } from '../shared/engine-protocol';

declare global {
	interface Window {
		engineBridge: {
			requestPort(): void;
			onStatus(handler: (status: EngineStatus) => void): void;
			demoElevationModelPath(): Promise<string>;
		};
	}
}

export {};
