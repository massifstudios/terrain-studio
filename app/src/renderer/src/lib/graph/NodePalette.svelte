<script lang="ts">
	import { Card, portal } from '@neoworks-dev/ui';
	import type { NodeSchema, PortTypeName } from '$shared/graph-document';
	import { engineClient } from '$lib/engine-client.svelte';
	import { acceptsInputType, groupByCategory } from '$shared/registry-schema';
	import { iconForCategory } from './category-icons';

	interface Props {
		/// Viewport coordinates: the palette lives on the body, not in the pane
		/// that opened it, so it is free to stand over anything.
		screenX: number;
		screenY: number;
		/// Set when an edge is waiting to be plugged into whatever is picked:
		/// only nodes with an input of this type are offered.
		acceptedType?: PortTypeName | null;
		onPick: (schema: NodeSchema) => void;
		onDismiss: () => void;
	}

	const { screenX, screenY, acceptedType = null, onPick, onDismiss }: Props = $props();

	const paletteWidth = 288;
	const preferredHeight = 460;
	const edgeGap = 8;

	let query = $state('');
	let searchInput = $state<HTMLInputElement | null>(null);
	let palette = $state<HTMLDivElement | null>(null);
	let list = $state<HTMLDivElement | null>(null);
	let highlighted = $state(0);
	let windowWidth = $state(0);
	let windowHeight = $state(0);

	function clamp(value: number, low: number, high: number): number {
		if (high < low) {
			return low;
		}
		return Math.min(Math.max(value, low), high);
	}

	const height = $derived(Math.min(preferredHeight, windowHeight - edgeGap * 2));
	const left = $derived(clamp(screenX, edgeGap, windowWidth - paletteWidth - edgeGap));

	/// Upward from the cursor by preference, which keeps the list over the graph
	/// rather than over whatever is below it; downward when the room is there
	/// instead, and against the nearer edge when it is in neither.
	const top = $derived.by(() => {
		if (screenY - height >= edgeGap) {
			return screenY - height;
		}
		if (screenY + height + edgeGap <= windowHeight) {
			return screenY;
		}
		return clamp(screenY - height, edgeGap, windowHeight - height - edgeGap);
	});

	/// On the body it no longer sits inside the editor, so a click anywhere else
	/// has to be caught here to close it.
	function dismissOnOutsideClick(event: PointerEvent): void {
		const target = event.target as Node | null;
		if (target && palette?.contains(target)) {
			return;
		}
		onDismiss();
	}

	const categories = $derived.by(() => {
		const needle = query.trim().toLowerCase();
		return groupByCategory(engineClient.registry)
			.map((category) => ({
				name: category.name,
				nodes: category.nodes.filter((node) => isOffered(node) && matches(node, needle))
			}))
			.filter((category) => category.nodes.length > 0);
	});

	function isOffered(node: NodeSchema): boolean {
		if (acceptedType === null) {
			return true;
		}
		return acceptsInputType(node, acceptedType);
	}

	const emptyMessage = $derived.by(() => {
		if (acceptedType === null) {
			return 'No node matches';
		}
		return `No node with a ${acceptedType} input matches`;
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

	/// The list as one run of nodes, which is what the arrow keys walk: the
	/// category headings are labels, not stops.
	const listedNodes = $derived(categories.flatMap((category) => category.nodes));

	const sections = $derived.by(() => {
		let index = 0;
		return categories.map((category) => ({
			name: category.name,
			nodes: category.nodes.map((node) => ({ node, index: index++ }))
		}));
	});

	function stepBy(offset: number): void {
		if (listedNodes.length === 0) {
			return;
		}
		const next = highlighted + offset;
		highlighted = Math.min(Math.max(next, 0), listedNodes.length - 1);
		scrollHighlightedIntoView();
	}

	function scrollHighlightedIntoView(): void {
		const row = list?.querySelector(`[data-node-index="${highlighted}"]`);
		row?.scrollIntoView({ block: 'nearest' });
	}

	/// Arrows and Tab walk the list, Enter takes what is under the cursor, and
	/// the search field keeps the focus throughout so typing narrows as you go.
	function handleKey(event: KeyboardEvent): void {
		if (event.key === 'Escape') {
			onDismiss();
			return;
		}
		const offset = stepForKey(event);
		if (offset !== null) {
			event.preventDefault();
			stepBy(offset);
			return;
		}
		if (event.key === 'Enter' && listedNodes[highlighted]) {
			event.preventDefault();
			onPick(listedNodes[highlighted]);
		}
	}

	function stepForKey(event: KeyboardEvent): number | null {
		if (event.key === 'ArrowDown') {
			return 1;
		}
		if (event.key === 'ArrowUp') {
			return -1;
		}
		if (event.key === 'Tab' && event.shiftKey) {
			return -1;
		}
		if (event.key === 'Tab') {
			return 1;
		}
		return null;
	}

	/// Narrowing the search moves the ground under the highlight; it starts over
	/// at the top rather than pointing at whatever now holds that position.
	$effect(() => {
		query;
		highlighted = 0;
	});

	$effect(() => {
		searchInput?.focus();
	});
</script>

<svelte:window bind:innerWidth={windowWidth} bind:innerHeight={windowHeight} />
<svelte:document onpointerdown={dismissOnOutsideClick} />

<div
	use:portal
	bind:this={palette}
	class="fixed z-[var(--z-overlay)] w-72"
	style="left: {left}px; top: {top}px; height: {height}px"
	onkeydown={handleKey}
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
		<div class="min-h-0 flex-1 overflow-y-auto" bind:this={list}>
			{#each sections as section}
				<div class="mb-1">
					<div class="px-1 py-1 text-2xs uppercase tracking-caps text-faint">
						{section.name}
					</div>
					{#each section.nodes as entry}
						{@const Icon = iconForCategory(entry.node.category)}
						<button
							class="flex w-full items-center gap-2 rounded-sm px-2 py-1 text-left hover:bg-hover"
							class:bg-hover={entry.index === highlighted}
							data-node-index={entry.index}
							onclick={() => onPick(entry.node)}
							onpointerenter={() => (highlighted = entry.index)}
						>
							<Icon size={16} class="shrink-0 text-dim" />
							<span class="flex min-w-0 flex-col">
								<span class="text-xs text-default">{entry.node.typeName}</span>
								<span class="truncate text-2xs text-faint">{entry.node.description}</span>
							</span>
						</button>
					{/each}
				</div>
			{/each}
			{#if categories.length === 0}
				<div class="px-2 py-3 text-center text-2xs text-faint">{emptyMessage}</div>
			{/if}
		</div>
	</Card>
</div>
