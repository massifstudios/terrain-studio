import { join } from 'node:path';
import { readFile, writeFile } from 'node:fs/promises';
import { app, BrowserWindow, dialog, ipcMain, shell } from 'electron';
import {
	assetDirectories,
	connectRenderer,
	startEngine,
	stopEngine,
	trackStatus
} from './engine-connection';
import { watchProject } from './project-watch';
import { readSession, writeSession } from './session-store';

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

/// A texture is named by file alone and looked up in the asset directories in
/// order, so the renderer never handles a path and cannot reach outside them.
async function readTextureAsset(name: string): Promise<Uint8Array | null> {
	if (name.includes('/') || name.includes('\\') || name.includes('..')) {
		return null;
	}
	for (const directory of assetDirectories()) {
		try {
			return await readFile(join(directory, 'textures', name));
		} catch {
			continue;
		}
	}
	return null;
}

ipcMain.handle('asset:read-texture', (_event, name: string) => readTextureAsset(name));

ipcMain.on('project:watch', (event, path: string | null) => watchProject(event.sender, path));

ipcMain.handle('session:read', () => readSession());

// Synchronous, so that the edit made a moment before a quit cannot be lost to a
// promise the closing window will never settle. A disk that refuses the write
// is reported and nothing more; it is not worth taking the app down for.
ipcMain.on('session:write', (event, graph: string, pointer: string) => {
	try {
		writeSession({ graph, pointer });
	} catch (error) {
		console.warn(`Could not remember the session: ${String(error)}`);
	}
	event.returnValue = null;
});

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
