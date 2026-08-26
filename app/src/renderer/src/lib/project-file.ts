/**
 * Opening and saving the `.massifgraph` project. Every read and write of that
 * file goes through here — the top bar, the keyboard and the watcher all reach
 * for it, and none of them may drift from the others.
 */

import { graphStore } from '$lib/graph/graph-store.svelte';
import { sourceModel } from '$lib/graph/source-model.svelte';
import { parseGraph, serializeGraph } from '$shared/massifgraph';

let lastKnownText: string | null = null;

/// What the project file held the last time it was read or written here. The
/// watcher compares against this, so our own save does not come back looking
/// like somebody else's edit.
export function lastKnownProjectText(): string | null {
	return lastKnownText;
}

/// Takes the file's contents as the project, wherever they came from.
export function adoptProjectText(path: string, text: string): void {
	const project = parseGraph(text);
	sourceModel.rememberLoadedProject(project);
	graphStore.load(project, path);
	lastKnownText = text;
}

export async function saveProjectAs(path: string): Promise<void> {
	const text = serializeGraph(graphStore.project());
	await window.fileBridge.writeText(path, text);
	lastKnownText = text;
	graphStore.markSaved(path);
}

export async function openProject(): Promise<void> {
	const path = await window.fileBridge.chooseOpenPath('graph');
	if (path === null) {
		return;
	}
	adoptProjectText(path, await window.fileBridge.readText(path));
}

/// A project that has never been saved asks where to go; one that has goes back
/// where it came from without a dialog.
export async function saveProject(): Promise<void> {
	let path = graphStore.filePath;
	if (path === null) {
		path = await window.fileBridge.chooseSavePath('graph', 'terrain.massifgraph');
	}
	if (path === null) {
		return;
	}
	await saveProjectAs(path);
}
