<script lang="ts">
	import { Card } from '@neoworks-dev/ui';
	import type { NodeSchema } from '$shared/graph-document';
	import { engineClient } from '$lib/engine-client.svelte';
	import { groupByCategory } from './registry-schema';

	interface Props {
		screenX: number;
		screenY: number;
		onPick: (schema: NodeSchema) => void;
		onDismiss: () => void;
	}

	const { screenX, screenY, onPick, onDismiss }: Props = $props();

	let query = $state('');
	let searchInput = $state<HTMLInputElement | null>(null);

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

<div
	class="absolute z-overlay w-72"
	style="left: {screenX}px; top: {screenY}px"
	onkeydown={handleKey}
	role="dialog"
	aria-label="Node palette"
	tabindex="-1"
>
	<Card surface="elevated" padding="sm">
		<input
			class="mb-2 w-full rounded-sm border border-line bg-input px-2 py-1 text-xs text-default outline-none focus:border-line-strong"
			placeholder="Search nodes"
			bind:value={query}
			bind:this={searchInput}
		/>
		<div class="max-h-72 overflow-y-auto">
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
