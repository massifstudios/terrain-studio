/**
 * Opening and saving the `.massifgraph` project. One place, because the top bar
 * and the keyboard both reach for it and neither may drift from the other.
 */

import { graphStore } from '$lib/graph/graph-store.svelte';
import { sourceModel } from '$lib/graph/source-model.svelte';
import { parseGraph, serializeGraph } from '$shared/massifgraph';

export async function openProject(): Promise<void> {
	const path = await window.fileBridge.chooseOpenPath('graph');
	if (path === null) {
		return;
	}
	const project = parseGraph(await window.fileBridge.readText(path));
	sourceModel.rememberLoadedProject(project);
	graphStore.load(project, path);
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
	await window.fileBridge.writeText(path, serializeGraph(graphStore.project()));
	graphStore.markSaved(path);
}
