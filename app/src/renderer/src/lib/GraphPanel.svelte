<script lang="ts">
	import { Button } from '@neoworks-dev/ui';
	import CodeIcon from 'phosphor-svelte/lib/Code';
	import GraphIcon from 'phosphor-svelte/lib/Graph';
	import CodeView from '$lib/graph/CodeView.svelte';
	import GraphEditor from '$lib/graph/GraphEditor.svelte';

	type PanelView = 'graph' | 'code';

	interface Props {
		height: number;
	}

	const { height }: Props = $props();

	let view = $state<PanelView>('graph');

	function selectView(next: PanelView): void {
		view = next;
	}

	function tabVariant(target: PanelView): 'surface' | 'ghost' {
		if (view === target) {
			return 'surface';
		}
		return 'ghost';
	}
</script>

<section class="flex shrink-0 flex-col bg-elevated" style="height: {height}px">
	<div class="flex h-9 shrink-0 items-center gap-2 border-b border-line px-3">
		<Button
			size="sm"
			variant={tabVariant('graph')}
			icon={GraphIcon}
			onclick={() => selectView('graph')}
		>
			Graph
		</Button>
		<Button
			size="sm"
			variant={tabVariant('code')}
			icon={CodeIcon}
			onclick={() => selectView('code')}
		>
			Code
		</Button>
	</div>

	<div class="min-h-0 flex-1">
		{#if view === 'graph'}
			<GraphEditor />
		{:else}
			<CodeView />
		{/if}
	</div>
</section>
