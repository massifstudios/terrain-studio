import { join } from 'node:path';
import { readFile, writeFile } from 'node:fs/promises';
import { app, BrowserWindow, dialog, ipcMain, shell } from 'electron';
import { connectRenderer, startEngine, stopEngine, trackStatus } from './engine-connection';

function createWindow(): BrowserWindow {
	const window = new BrowserWindow({
		width: 1600,
		height: 1000,
		minWidth: 1100,
		minHeight: 700,
		show: false,
		// Painted before the page is, so it cannot come from a design-system
		// token; it is the system's `--bg` written out, and changes with it.
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

// File-format knowledge is allowed here and nowhere else in the UI.
const fileFilters: Record<string, Electron.FileFilter[]> = {
	graph: [{ name: 'Terrain Studio graph', extensions: ['massifgraph'] }],
	elevation: [
		{ name: 'Elevation model', extensions: ['massifdem', 'png', 'r32', 'raw', 'f32'] }
	],
	dem: [{ name: 'Massif DEM', extensions: ['massifdem'] }]
};

ipcMain.handle('dialog:open-path', async (_event, kind: string) => {
	const result = await dialog.showOpenDialog({
		properties: ['openFile'],
		filters: fileFilters[kind]
	});
	if (result.canceled || result.filePaths.length === 0) {
		return null;
	}
	return result.filePaths[0];
});

ipcMain.handle('dialog:save-path', async (_event, kind: string, suggestedName: string) => {
	const result = await dialog.showSaveDialog({
		defaultPath: suggestedName,
		filters: fileFilters[kind]
	});
	if (result.canceled || !result.filePath) {
		return null;
	}
	return result.filePath;
});

ipcMain.handle('file:read-text', (_event, path: string) => readFile(path, 'utf8'));

ipcMain.handle('file:write-text', (_event, path: string, text: string) =>
	writeFile(path, text, 'utf8')
);

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
