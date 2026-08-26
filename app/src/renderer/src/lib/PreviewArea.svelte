<script lang="ts">
	import { Button } from '@neoworks-dev/ui';
	import Columns from 'phosphor-svelte/lib/Columns';
	import Sun from 'phosphor-svelte/lib/Sun';

	let showTopDownView = $state(true);

	function toggleTopDownView(): void {
		showTopDownView = !showTopDownView;
	}
</script>

<section class="flex min-h-0 flex-1 flex-col bg-canvas">
	<div class="flex h-9 shrink-0 items-center gap-2 border-b border-line px-3">
		<span class="text-2xs uppercase tracking-caps text-dim">Preview</span>
		<div class="ml-auto flex items-center gap-2">
			<Button size="sm" variant="ghost" icon={Sun} disabled>Sun</Button>
			<Button size="sm" variant="ghost" icon={Columns} onclick={toggleTopDownView}>
				2D view
			</Button>
		</div>
	</div>

	<div class="flex min-h-0 flex-1">
		<div class="relative min-w-0 flex-1 border-r border-line">
			<div class="flex h-full items-center justify-center text-xs text-faint">
				3D viewport
			</div>
			<span class="absolute bottom-2 left-3 font-mono text-2xs text-faint">— m, — m, — m</span>
		</div>

		{#if showTopDownView}
			<div class="relative min-w-0 flex-1">
				<div class="flex h-full items-center justify-center text-xs text-faint">
					2D top-down
				</div>
				<span class="absolute bottom-2 left-3 font-mono text-2xs text-faint">— m, — m</span>
			</div>
		{/if}
	</div>
</section>
