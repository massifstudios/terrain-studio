<script lang="ts">
	import TopBar from '$lib/TopBar.svelte';
	import PreviewArea from '$lib/PreviewArea.svelte';
	import GraphPanel from '$lib/GraphPanel.svelte';
	import PropertiesPanel from '$lib/PropertiesPanel.svelte';
	import { engineClient } from '$lib/engine-client.svelte';

	engineClient.connect();
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
