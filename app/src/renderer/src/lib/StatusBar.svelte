<!--
	The bar along the bottom of the window: what the engine is doing on the
	right, and what the session is made of on the left.

	`Tooltip` comes from the design system. The progress bar does not: the
	system has `LoadingSpinner`, which says only "working", and an evaluation
	that takes minutes needs to say how far along it is. It is two divs and a
	width, drawn in the system's tokens.
-->
<script lang="ts">
	import { Tooltip } from '@neoworks-dev/ui';
	import { engineClient } from '$lib/engine-client.svelte';
	import { buildRunner } from '$lib/build-runner.svelte';
	import { graphStore } from '$lib/graph/graph-store.svelte';
	import { nodeStatusStore } from '$lib/graph/node-status.svelte';
	import { previewRunner } from '$lib/preview/preview-runner.svelte';
	import { previewState } from '$lib/preview/preview-state.svelte';
	import EngineStatusIndicator from './EngineStatusIndicator.svelte';

	interface Activity {
		label: string;
		detail: string;
		fraction: number;
	}

	function formatDuration(milliseconds: number): string {
		if (milliseconds < 1000) {
			return `${Math.round(milliseconds)} ms`;
		}
		if (milliseconds < 60_000) {
			return `${(milliseconds / 1000).toFixed(1)} s`;
		}
		const minutes = Math.floor(milliseconds / 60_000);
		const seconds = Math.round((milliseconds % 60_000) / 1000);
		return `${minutes} m ${String(seconds).padStart(2, '0')} s`;
	}

	function formatCount(value: number): string {
		if (value < 1000) {
			return String(value);
		}
		if (value < 1_000_000) {
			return `${(value / 1000).toFixed(1)}k`;
		}
		return `${(value / 1_000_000).toFixed(1)}M`;
	}

	/// How far the node the engine is chewing on has come. Zero until the first
	/// progress event of a run arrives.
	function nodeFraction(): number {
		if (!engineClient.progress) {
			return 0;
		}
		return engineClient.progress.fraction;
	}

	function nodeDetail(): string {
		if (!engineClient.progress) {
			return 'Waiting for the engine';
		}
		return `${engineClient.progress.typeName} — ${Math.round(engineClient.progress.fraction * 100)}%`;
	}

	/// A build walks its output nodes one at a time, so overall progress is the
	/// outputs already written plus how far the current one has come.
	function buildFraction(): number {
		if (buildRunner.total === 0) {
			return 0;
		}
		return (buildRunner.completed + nodeFraction()) / buildRunner.total;
	}

	const activity = $derived.by((): Activity | null => {
		if (buildRunner.isBuilding) {
			return {
				label: `Building ${buildRunner.completed + 1} of ${buildRunner.total}`,
				detail: nodeDetail(),
				fraction: buildFraction()
			};
		}
		if (previewRunner.isEvaluating) {
			return { label: 'Rendering', detail: nodeDetail(), fraction: nodeFraction() };
		}
		return null;
	});

	const selection = $derived.by(() => {
		if (graphStore.selectedNodeIds.length > 1) {
			return `${graphStore.selectedNodeIds.length} nodes`;
		}
		const node = graphStore.selectedNode;
		if (!node) {
			return 'None';
		}
		return node.typeName;
	});

	const grid = $derived(`${graphStore.settings.resolution} × ${graphStore.settings.resolution}`);

	/// Two triangles per cell of the field on screen — the viewport displaces one
	/// vertex per sample and never decimates.
	const triangles = $derived.by(() => {
		const field = previewState.field;
		if (!field) {
			return '—';
		}
		return formatCount((field.width - 1) * (field.height - 1) * 2);
	});

	const nodeCount = $derived.by(() => {
		if (graphStore.nodes.length === 1) {
			return '1 node';
		}
		return `${graphStore.nodes.length} nodes`;
	});

	/// How much of the last evaluation the engine could answer from its cache.
	const cache = $derived.by(() => {
		const states = Object.values(nodeStatusStore.statuses);
		const cached = states.filter((status) => status.state === 'cached').length;
		return { cached, total: states.length };
	});

	const cacheLabel = $derived.by(() => {
		if (cache.total === 0) {
			return 'Empty';
		}
		if (cache.cached === cache.total) {
			return 'Warm';
		}
		return `${cache.cached} of ${cache.total}`;
	});

	const cacheTone = $derived.by(() => {
		if (cache.total > 0 && cache.cached === cache.total) {
			return 'text-green';
		}
		return 'text-default';
	});

	const buildFailure = $derived.by(() => {
		if (buildRunner.state !== 'failed') {
			return null;
		}
		return buildRunner.errors.join('\n');
	});
</script>

{#snippet stat(label: string, value: string, tone: string)}
	<span class="flex items-center gap-1.5 whitespace-nowrap">
		<span class="text-faint">{label}</span>
		<span class="font-medium {tone}">{value}</span>
	</span>
{/snippet}

<footer
	class="flex h-7 shrink-0 items-center gap-5 border-t border-line bg-elevated px-3 text-2xs text-muted"
>
	<EngineStatusIndicator />
	{@render stat('Selected', selection, 'text-default')}
	{@render stat('Grid', grid, 'text-default')}
	{@render stat('Triangles', triangles, 'text-default')}
	{@render stat('Graph', nodeCount, 'text-default')}
	{@render stat('Cache', cacheLabel, cacheTone)}

	<div class="ml-auto flex items-center gap-3">
		{#if activity}
			<span class="text-faint">{activity.label}</span>
			<Tooltip text={activity.detail}>
				<div class="h-1 w-32 overflow-hidden rounded-full bg-input">
					<div class="h-full bg-action" style="width: {activity.fraction * 100}%"></div>
				</div>
			</Tooltip>
		{:else}
			{#if previewRunner.lastMilliseconds !== null}
				{@render stat('Preview', formatDuration(previewRunner.lastMilliseconds), 'text-default')}
			{/if}
			{#if buildRunner.state === 'finished' && buildRunner.lastMilliseconds !== null}
				{@render stat('Build', formatDuration(buildRunner.lastMilliseconds), 'text-green')}
			{/if}
			{#if buildFailure}
				<Tooltip text={buildFailure}>
					{@render stat('Build', 'Failed', 'text-red')}
				</Tooltip>
			{/if}
			{#if buildRunner.state === 'cancelled'}
				{@render stat('Build', 'Cancelled', 'text-muted')}
			{/if}
		{/if}
	</div>
</footer>
