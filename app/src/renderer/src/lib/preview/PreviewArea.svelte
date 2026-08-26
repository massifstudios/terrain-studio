<script lang="ts">
	import { Button, Card } from '@neoworks-dev/ui';
	import Columns from 'phosphor-svelte/lib/Columns';
	import Sun from 'phosphor-svelte/lib/Sun';
	import ValueSlider from '$lib/controls/ValueSlider.svelte';
	import CursorReadout from './CursorReadout.svelte';
	import { previewState } from './preview-state.svelte';
	import Viewport2D from './Viewport2D.svelte';
	import Viewport3D from './Viewport3D.svelte';

	let showSunControl = $state(false);

	function toggleTopDownView(): void {
		previewState.showTopDownView = !previewState.showTopDownView;
	}

	function toggleSunControl(): void {
		showSunControl = !showSunControl;
	}
</script>

<section class="flex min-h-0 flex-1 flex-col bg-canvas">
	<div class="relative flex h-9 shrink-0 items-center gap-2 border-b border-line px-3">
		<span class="text-2xs uppercase tracking-caps text-dim">Preview</span>
		<div class="ml-auto flex items-center gap-2">
			<Button size="sm" variant="ghost" icon={Sun} onclick={toggleSunControl}>Sun</Button>
			<Button size="sm" variant="ghost" icon={Columns} onclick={toggleTopDownView}>
				2D view
			</Button>
		</div>

		{#if showSunControl}
			<div class="absolute right-3 top-10 z-raised w-64">
				<Card surface="elevated" padding="md">
					<div class="flex flex-col gap-3">
						<ValueSlider
							label="Azimuth"
							unit="°"
							value={previewState.sunAzimuthDegrees}
							minimum={0}
							maximum={360}
							step={1}
							onChange={(value) => (previewState.sunAzimuthDegrees = value)}
						/>
						<ValueSlider
							label="Altitude"
							unit="°"
							value={previewState.sunAltitudeDegrees}
							minimum={1}
							maximum={89}
							step={1}
							onChange={(value) => (previewState.sunAltitudeDegrees = value)}
						/>
					</div>
				</Card>
			</div>
		{/if}
	</div>

	<div class="flex min-h-0 flex-1">
		<div class="relative min-w-0 flex-1 border-r border-line">
			<Viewport3D />
			<CursorReadout />
		</div>

		{#if previewState.showTopDownView}
			<div class="relative min-w-0 flex-1">
				<Viewport2D />
				<CursorReadout />
			</div>
		{/if}
	</div>

	{#if previewState.errors.length > 0}
		<div class="shrink-0 border-t border-line bg-red-soft px-3 py-2 text-2xs text-red">
			{previewState.errors.join(' · ')}
		</div>
	{/if}
</section>
