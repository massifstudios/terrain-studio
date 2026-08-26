import { contextBridge, ipcRenderer } from 'electron';
import { enginePortChannel, type EngineStatus } from '../shared/engine-protocol';

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

export type EngineBridge = typeof engineBridge;
export type FileBridge = typeof fileBridge;

contextBridge.exposeInMainWorld('engineBridge', engineBridge);
contextBridge.exposeInMainWorld('fileBridge', fileBridge);
