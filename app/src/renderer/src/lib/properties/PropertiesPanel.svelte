<script lang="ts">
	import { SectionHeader } from '@neoworks-dev/ui';
	import { engineClient } from '$lib/engine-client.svelte';
	import { graphStore } from '$lib/graph/graph-store.svelte';
	import { defaultParameterValue, findNodeSchema } from '$shared/registry-schema';
	import ParameterControl from './ParameterControl.svelte';

	const node = $derived(graphStore.selectedNode);
	const schema = $derived.by(() => {
		if (!node) {
			return null;
		}
		return findNodeSchema(engineClient.registry, node.typeName);
	});

	/// Settings a missing plugin's node carried in from the project file. They
	/// are shown as stored and left untouched, so saving the project back does
	/// not quietly drop them.
	const preserved = $derived.by(() => {
		if (!node || schema) {
			return [];
		}
		return Object.keys(node.parameters)
			.sort()
			.map((name) => ({ name, value: String(node.parameters[name]) }));
	});

	function valueOf(name: string) {
		const stored = node?.parameters[name];
		if (stored !== undefined) {
			return stored;
		}
		const parameter = schema?.parameters.find((entry) => entry.name === name);
		if (!parameter) {
			return 0;
		}
		return defaultParameterValue(parameter);
	}
</script>

<aside class="flex w-80 shrink-0 flex-col border-l border-line bg-elevated">
	<div class="border-b border-line px-4 py-3">
		<SectionHeader title="Properties" />
	</div>

	{#if !node}
		<div class="flex flex-1 items-center justify-center px-4 text-center text-xs text-faint">
			Select a node to edit its parameters
		</div>
	{:else if !schema}
		<div class="flex flex-col gap-3 overflow-y-auto px-4 py-3">
			<div class="flex flex-col gap-1">
				<span class="text-sm font-medium">{node.typeName}</span>
				<span class="text-2xs text-red">
					No loaded plugin provides this node. Its settings and connections are kept, and
					it will not evaluate.
				</span>
			</div>

			{#each preserved as entry (entry.name)}
				<div class="flex items-baseline justify-between gap-2 text-2xs">
					<span class="text-muted">{entry.name}</span>
					<span class="truncate font-mono text-faint" title={entry.value}>{entry.value}</span>
				</div>
			{/each}
		</div>
	{:else}
		<div class="flex flex-col gap-3 overflow-y-auto px-4 py-3">
			<div class="flex flex-col gap-1">
				<span class="text-sm font-medium">{node.typeName}</span>
				<span class="text-2xs text-faint">{schema.description}</span>
			</div>

			{#if schema.parameters.length === 0}
				<span class="text-2xs text-faint">This node has no parameters.</span>
			{/if}

			{#each schema.parameters as parameter (parameter.name)}
				<ParameterControl
					schema={parameter}
					value={valueOf(parameter.name)}
					onChange={(value) => graphStore.setParameter(node.id, parameter.name, value)}
				/>
			{/each}
		</div>
	{/if}
</aside>
