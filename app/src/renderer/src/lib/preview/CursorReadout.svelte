<script lang="ts">
	import { previewState } from './preview-state.svelte';

	const readout = $derived.by(() => {
		const field = previewState.field;
		const cursor = previewState.cursor;
		if (!field || !cursor) {
			return null;
		}
		return {
			eastMeters: cursor.column * field.cellSizeMeters,
			southMeters: cursor.row * field.cellSizeMeters,
			altitudeMeters: cursor.altitudeMeters
		};
	});

	function format(value: number): string {
		return value.toFixed(1);
	}
</script>

<span class="pointer-events-none absolute bottom-2 left-3 font-mono text-2xs text-faint">
	{#if readout}
		E {format(readout.eastMeters)} m · S {format(readout.southMeters)} m ·
		{format(readout.altitudeMeters)} m
	{:else}
		— · — · —
	{/if}
</span>
