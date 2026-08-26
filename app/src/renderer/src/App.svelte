<script lang="ts">
	import TopBar from '$lib/TopBar.svelte';
	import PreviewArea from '$lib/preview/PreviewArea.svelte';
	import GraphPanel from '$lib/GraphPanel.svelte';
	import PropertiesPanel from '$lib/PropertiesPanel.svelte';
	import { engineClient } from '$lib/engine-client.svelte';
	import { demoPreviewNodeId, demoRefinementGraph } from '$lib/demo-graph';
	import { previewState } from '$lib/preview/preview-state.svelte';
	import type { ElevationModelDescription } from '$shared/engine-protocol';

	const previewResolution = 512;

	engineClient.connect();

	async function loadDemoTerrain(): Promise<void> {
		const path = await window.engineBridge.demoElevationModelPath();
		const description = (await engineClient.send('describeElevationModel', {
			path
		})) as ElevationModelDescription;

		const graph = demoRefinementGraph(path, description, previewResolution);
		const outcome = await engineClient.evaluate({
			graph,
			nodeId: demoPreviewNodeId,
			outputPort: 0
		});

		previewState.errors = outcome.errors;
		previewState.field = outcome.field;
	}

	$effect(() => {
		if (engineClient.registry) {
			loadDemoTerrain();
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
