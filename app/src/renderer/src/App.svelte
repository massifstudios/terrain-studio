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

	engineClient.connect();

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
			nodeStatusStore.markEvaluating(progress.nodeId);
		}
	});
</script>

<div class="flex h-full flex-col bg-canvas text-default">
	<TopBar />
	<div class="flex min-h-0 flex-1">
		<div class="flex min-w-0 flex-1 flex-col">
			<PreviewArea />
			<GraphPanel />
		</div>
		<PropertiesPanel />
	</div>
</div>
