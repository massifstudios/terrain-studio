<script lang="ts">
	import { Button } from '@neoworks-dev/ui';
	import CodeIcon from 'phosphor-svelte/lib/Code';
	import GraphIcon from 'phosphor-svelte/lib/Graph';
	import GraphEditor from '$lib/graph/GraphEditor.svelte';

	type PanelView = 'graph' | 'code';

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

<section class="flex h-80 shrink-0 flex-col border-t border-line bg-elevated">
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
			<div class="flex h-full items-center justify-center text-xs text-faint">
				Graph as code
			</div>
		{/if}
	</div>
</section>
