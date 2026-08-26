<!--
	What the engine made of the plugin directories, and the button that looks
	again. Built from `StatusBadge` and `Tooltip`; nothing new was needed.

	It stays out of the way when there is nothing to say — no plugins at all is
	the ordinary case, and a badge reading "0 plugins" would be noise.
-->
<script lang="ts">
	import { Button, StatusBadge, Tooltip } from '@neoworks-dev/ui';
	import type { StatusTone } from '@neoworks-dev/ui';
	import ArrowsClockwise from 'phosphor-svelte/lib/ArrowsClockwise';
	import { engineClient } from './engine-client.svelte';

	const refused = $derived(engineClient.plugins.filter((plugin) => !plugin.loaded));
	const loaded = $derived(engineClient.plugins.filter((plugin) => plugin.loaded));

	const tone = $derived.by((): StatusTone => {
		if (refused.length > 0) {
			return 'red';
		}
		return 'green';
	});

	const label = $derived.by(() => {
		if (refused.length > 0) {
			return `${refused.length} plugin refused`;
		}
		if (loaded.length === 1) {
			return '1 plugin';
		}
		return `${loaded.length} plugins`;
	});

	/// The refusals first: they are the reason anyone opens this.
	const detail = $derived(
		[...refused, ...loaded]
			.map((plugin) => {
				if (plugin.message) {
					return `${plugin.name} — ${plugin.message}`;
				}
				return `${plugin.name} — ${plugin.nodeCount} nodes`;
			})
			.join('\n')
	);
</script>

{#if engineClient.plugins.length > 0}
	<Tooltip text={detail}>
		<StatusBadge {tone}>{label}</StatusBadge>
	</Tooltip>
{/if}

<Button
	size="sm"
	variant="ghost"
	icon={ArrowsClockwise}
	onclick={() => engineClient.rescanPlugins()}
>
	Plugins
</Button>
