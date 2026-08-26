import type { EngineStatus } from '../shared/engine-protocol';
import type { StoredSession } from '../shared/session';

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
		sessionBridge: {
			read(): Promise<StoredSession | null>;
			write(graph: string, pointer: string): void;
		};
	}
}

export {};
