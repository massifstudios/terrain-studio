<script lang="ts">
	import { Card } from '@neoworks-dev/ui';
	import type { NodeSchema } from '$shared/graph-document';
	import { engineClient } from '$lib/engine-client.svelte';
	import { groupByCategory } from '$shared/registry-schema';

	interface Props {
		screenX: number;
		screenY: number;
		surfaceWidth: number;
		surfaceHeight: number;
		onPick: (schema: NodeSchema) => void;
		onDismiss: () => void;
	}

	const { screenX, screenY, surfaceWidth, surfaceHeight, onPick, onDismiss }: Props = $props();

	/// The editor surface clips whatever leaves it, so the palette is sized to the
	/// room it has and kept inside the edges rather than hanging off the cursor.
	const paletteWidth = 288;
	const preferredHeight = 460;
	const edgeGap = 8;

	let query = $state('');
	let searchInput = $state<HTMLInputElement | null>(null);

	function clamp(value: number, low: number, high: number): number {
		if (high < low) {
			return low;
		}
		return Math.min(Math.max(value, low), high);
	}

	const height = $derived(Math.min(preferredHeight, surfaceHeight - edgeGap * 2));
	const left = $derived(clamp(screenX, edgeGap, surfaceWidth - paletteWidth - edgeGap));

	/// Below the cursor by preference, above it when the room is up there, and
	/// against the nearest edge when it is in neither.
	const top = $derived.by(() => {
		if (screenY + height + edgeGap <= surfaceHeight) {
			return screenY;
		}
		if (screenY - height >= edgeGap) {
			return screenY - height;
		}
		return clamp(screenY, edgeGap, surfaceHeight - height - edgeGap);
	});

	const categories = $derived.by(() => {
		const needle = query.trim().toLowerCase();
		return groupByCategory(engineClient.registry)
			.map((category) => ({
				name: category.name,
				nodes: category.nodes.filter((node) => matches(node, needle))
			}))
			.filter((category) => category.nodes.length > 0);
	});

	function matches(node: NodeSchema, needle: string): boolean {
		if (needle.length === 0) {
			return true;
		}
		return (
			node.typeName.toLowerCase().includes(needle) ||
			node.category.toLowerCase().includes(needle) ||
			node.description.toLowerCase().includes(needle)
		);
	}

	function handleKey(event: KeyboardEvent): void {
		if (event.key === 'Escape') {
			onDismiss();
		}
	}

	$effect(() => {
		searchInput?.focus();
	});
</script>

<!--
	Wheel and pointer events stop here: the editor surface behind the palette
	reads them as a zoom and as a click on empty canvas, which would scroll the
	graph instead of the list and dismiss the palette before a pick lands.
-->
<div
	class="absolute z-overlay w-72"
	style="left: {left}px; top: {top}px; height: {height}px"
	onkeydown={handleKey}
	onwheel={(event) => event.stopPropagation()}
	onpointerdown={(event) => event.stopPropagation()}
	role="dialog"
	aria-label="Node palette"
	tabindex="-1"
>
	<Card surface="elevated" padding="sm" class="flex h-full flex-col">
		<input
			class="mb-2 w-full rounded-sm border border-line bg-input px-2 py-1 text-xs text-default outline-none focus:border-line-strong"
			placeholder="Search nodes"
			bind:value={query}
			bind:this={searchInput}
		/>
		<div class="min-h-0 flex-1 overflow-y-auto">
			{#each categories as category}
				<div class="mb-1">
					<div class="px-1 py-1 text-2xs uppercase tracking-caps text-faint">
						{category.name}
					</div>
					{#each category.nodes as node}
						<button
							class="flex w-full flex-col rounded-sm px-2 py-1 text-left hover:bg-hover"
							onclick={() => onPick(node)}
						>
							<span class="text-xs text-default">{node.typeName}</span>
							<span class="truncate text-2xs text-faint">{node.description}</span>
						</button>
					{/each}
				</div>
			{/each}
			{#if categories.length === 0}
				<div class="px-2 py-3 text-center text-2xs text-faint">No node matches</div>
			{/if}
		</div>
	</Card>
</div>
