import { join } from 'node:path';
import { app, BrowserWindow, ipcMain, shell } from 'electron';
import { connectRenderer, startEngine, stopEngine, trackStatus } from './engine-connection';

function createWindow(): BrowserWindow {
	const window = new BrowserWindow({
		width: 1600,
		height: 1000,
		minWidth: 1100,
		minHeight: 700,
		show: false,
		backgroundColor: '#141416',
		autoHideMenuBar: true,
		webPreferences: {
			preload: join(__dirname, '../preload/index.mjs'),
			sandbox: false,
			contextIsolation: true
		}
	});

	window.once('ready-to-show', () => window.show());
	window.webContents.setWindowOpenHandler(({ url }) => {
		shell.openExternal(url);
		return { action: 'deny' };
	});

	loadRenderer(window);
	trackStatus(window.webContents);
	return window;
}

function loadRenderer(window: BrowserWindow): void {
	const devServerUrl = process.env.ELECTRON_RENDERER_URL;
	if (devServerUrl) {
		window.loadURL(devServerUrl);
		return;
	}
	window.loadFile(join(__dirname, '../renderer/index.html'));
}

ipcMain.on('engine:request-port', (event) => connectRenderer(event.sender));

app.whenReady().then(() => {
	startEngine();
	createWindow();

	app.on('activate', () => {
		if (BrowserWindow.getAllWindows().length === 0) {
			createWindow();
		}
	});
});

app.on('window-all-closed', () => {
	if (process.platform !== 'darwin') {
		app.quit();
	}
});

app.on('will-quit', stopEngine);
