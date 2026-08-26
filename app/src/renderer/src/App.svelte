<script lang="ts">
	import TopBar from '$lib/TopBar.svelte';
	import PreviewArea from '$lib/preview/PreviewArea.svelte';
	import GraphPanel from '$lib/GraphPanel.svelte';
	import PropertiesPanel from '$lib/properties/PropertiesPanel.svelte';
	import { engineClient } from '$lib/engine-client.svelte';
	import { nodeStatusStore } from '$lib/graph/node-status.svelte';
	import { graphStore } from '$lib/graph/graph-store.svelte';
	import { previewRunner } from '$lib/preview/preview-runner.svelte';
	import { sourceModel } from '$lib/graph/source-model.svelte';
	import { saveProject } from '$lib/project-file';
	import { sessionStore } from '$lib/session';
	import ResizeHandle from '$lib/controls/ResizeHandle.svelte';

	engineClient.connect();
	sessionStore.restore();

	// The debounce is generous, so the window closing is where the last edit of
	// a session is actually written.
	window.addEventListener('beforeunload', () => sessionStore.flush());

	function handleKeyDown(event: KeyboardEvent): void {
		if (event.key !== 's' && event.key !== 'S') {
			return;
		}
		if (!event.ctrlKey && !event.metaKey) {
			return;
		}
		event.preventDefault();
		saveProject();
	}

	const minimumGraphPanelHeight = 160;
	const minimumPreviewHeight = 200;

	let graphPanelHeight = $state(320);
	let workspaceHeight = $state(0);

	/// Dragging the divider down takes height from the graph panel and gives it
	/// to the preview, until one of the two hits its floor.
	function resizeGraphPanel(deltaPixels: number): void {
		const largest = Math.max(minimumGraphPanelHeight, workspaceHeight - minimumPreviewHeight);
		const wanted = graphPanelHeight - deltaPixels;
		graphPanelHeight = Math.min(Math.max(wanted, minimumGraphPanelHeight), largest);
	}

	// Choosing a different elevation model sets the world size and the survey
	// coordinates from its header, before anything is evaluated against them.
	$effect(() => {
		sourceModel.follow(graphStore.nodes, engineClient.registry);
	});

	// Any edit to the graph, its settings, or the selected preview node asks for
	// a fresh evaluation; the runner debounces and cancels what it replaces.
	$effect(() => {
		engineClient.registry;
		previewRunner.request(graphStore.document(), graphStore.previewNodeId);
	});

	$effect(() => {
		const progress = engineClient.progress;
		if (progress) {
			nodeStatusStore.markEvaluating(progress.nodeId, progress.fraction);
		}
	});

	// Everything that would be missed if the window went now.
	$effect(() => {
		graphStore.settings;
		graphStore.nodes;
		graphStore.edges;
		graphStore.previewNodeId;
		graphStore.filePath;
		graphStore.hasUnsavedChanges;
		sessionStore.remember();
	});
</script>

<svelte:window onkeydown={handleKeyDown} />

<div class="flex h-full flex-col bg-canvas text-default">
	<TopBar />
	<div class="flex min-h-0 flex-1">
		<div class="flex min-w-0 flex-1 flex-col" bind:clientHeight={workspaceHeight}>
			<PreviewArea />
			<ResizeHandle
				orientation="horizontal"
				label="Resize the graph panel"
				value={graphPanelHeight}
				minimum={minimumGraphPanelHeight}
				maximum={Math.max(minimumGraphPanelHeight, workspaceHeight - minimumPreviewHeight)}
				onResize={resizeGraphPanel}
			/>
			<GraphPanel height={graphPanelHeight} />
		</div>
		<PropertiesPanel />
	</div>
</div>
