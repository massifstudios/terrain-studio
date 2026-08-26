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
	demoElevationModelPath(): Promise<string> {
		return ipcRenderer.invoke('app:demo-dem-path');
	}
};

export type EngineBridge = typeof engineBridge;

contextBridge.exposeInMainWorld('engineBridge', engineBridge);
