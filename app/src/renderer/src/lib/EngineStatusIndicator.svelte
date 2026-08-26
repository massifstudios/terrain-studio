<!--
	The engine's state as a cell of the status bar: a label and a coloured
	value, the same shape as every other stat down there. A `StatusBadge` would
	be the design system's answer, but a pill is heavier than the row it would
	sit in, and the version belongs in the tooltip rather than the bar.
-->
<script lang="ts">
	import { Tooltip } from '@neoworks-dev/ui';
	import { engineClient } from './engine-client.svelte';

	const tones: Record<string, string> = {
		starting: 'text-amber',
		ready: 'text-green',
		crashed: 'text-amber',
		unavailable: 'text-red'
	};

	const label = $derived.by(() => {
		if (engineClient.status === 'ready') {
			return 'Ready';
		}
		if (engineClient.status === 'crashed') {
			return 'Restarting';
		}
		if (engineClient.status === 'unavailable') {
			return 'Unavailable';
		}
		return 'Starting';
	});

	const detail = $derived.by(() => {
		if (engineClient.status === 'ready' && engineClient.version) {
			return `Engine ${engineClient.version}`;
		}
		if (engineClient.status === 'crashed') {
			return 'The engine crashed and is being restarted';
		}
		if (engineClient.status === 'unavailable') {
			return 'The engine crashed on launch too often to keep trying';
		}
		return 'The engine is starting';
	});
</script>

<Tooltip text={detail}>
	<span class="flex items-center gap-1.5 whitespace-nowrap">
		<span class="text-faint">Engine</span>
		<span class="font-medium {tones[engineClient.status]}">{label}</span>
	</span>
</Tooltip>
