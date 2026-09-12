import { contextBridge, ipcRenderer } from 'electron';
import { enginePortChannel, type EngineStatus } from '../shared/engine-protocol';
import type { StoredSession } from '../shared/session';

// MessagePorts cannot cross the context bridge, so the port is re-posted into
// the page itself; the envelope names the channel the renderer listens for.
ipcRenderer.on('engine:port', (event) => {
	window.postMessage({ channel: enginePortChannel }, '*', [...event.ports]);
});

const engineBridge = {
	requestPort(): void {
		ipcRenderer.send('engine:request-port');
	},
	onStatus(handler: (status: EngineStatus) => void): void {
		ipcRenderer.on('engine:status', (_event, status: EngineStatus) => handler(status));
	},
};

const fileBridge = {
	chooseOpenPath(kind: 'graph' | 'elevation'): Promise<string | null> {
		return ipcRenderer.invoke('dialog:open-path', kind);
	},
	chooseSavePath(kind: 'graph' | 'dem', suggestedName: string): Promise<string | null> {
		return ipcRenderer.invoke('dialog:save-path', kind, suggestedName);
	},
	readText(path: string): Promise<string> {
		return ipcRenderer.invoke('file:read-text', path);
	},
	writeText(path: string, text: string): Promise<void> {
		return ipcRenderer.invoke('file:write-text', path, text);
	}
};

const assetBridge = {
	/// The bytes of a shipped or user-installed texture, by file name alone.
	/// Null when no asset directory has it.
	readTexture(name: string): Promise<Uint8Array | null> {
		return ipcRenderer.invoke('asset:read-texture', name);
	}
};

const projectBridge = {
	/// Null stops watching, which is what an unsaved project wants.
	watch(path: string | null): void {
		ipcRenderer.send('project:watch', path);
	},
	onChanged(handler: (path: string, text: string) => void): void {
		ipcRenderer.on('project:changed', (_event, path: string, text: string) => handler(path, text));
	}
};

const sessionBridge = {
	read(): Promise<StoredSession | null> {
		return ipcRenderer.invoke('session:read');
	},
	/// Synchronous: this is also what runs as the window is closing, where an
	/// unsettled promise would simply take the last edit with it.
	write(graph: string, pointer: string): void {
		ipcRenderer.sendSync('session:write', graph, pointer);
	}
};

export type EngineBridge = typeof engineBridge;
export type FileBridge = typeof fileBridge;
export type AssetBridge = typeof assetBridge;
export type ProjectBridge = typeof projectBridge;
export type SessionBridge = typeof sessionBridge;

contextBridge.exposeInMainWorld('engineBridge', engineBridge);
contextBridge.exposeInMainWorld('fileBridge', fileBridge);
contextBridge.exposeInMainWorld('assetBridge', assetBridge);
contextBridge.exposeInMainWorld('projectBridge', projectBridge);
contextBridge.exposeInMainWorld('sessionBridge', sessionBridge);
