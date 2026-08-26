import type { EngineStatus } from '../shared/engine-protocol';

declare global {
	interface Window {
		engineBridge: {
			requestPort(): void;
			onStatus(handler: (status: EngineStatus) => void): void;
		};
		fileBridge: {
			chooseOpenPath(kind: 'graph' | 'elevation'): Promise<string | null>;
			chooseSavePath(kind: 'graph' | 'dem', suggestedName: string): Promise<string | null>;
			readText(path: string): Promise<string>;
			writeText(path: string, text: string): Promise<void>;
		};
	}
}

export {};
