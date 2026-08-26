<script lang="ts">
	import type { NodeThumbnail } from '$shared/engine-protocol';

	interface Props {
		thumbnail: NodeThumbnail | null;
		isStale: boolean;
	}

	const { thumbnail, isStale }: Props = $props();

	let canvas = $state<HTMLCanvasElement | null>(null);

	/// The engine shades the thumbnail; this only blits it.
	$effect(() => {
		if (!canvas || !thumbnail) {
			return;
		}
		canvas.width = thumbnail.size;
		canvas.height = thumbnail.size;
		const context = canvas.getContext('2d');
		if (!context) {
			return;
		}
		const pixels = new Uint8ClampedArray(thumbnail.rgba);
		context.putImageData(new ImageData(pixels, thumbnail.size, thumbnail.size), 0, 0);
	});
</script>

<div class="relative h-full w-full overflow-hidden bg-input">
	{#if thumbnail}
		<canvas
			class="h-full w-full [image-rendering:pixelated]"
			class:opacity-40={isStale}
			bind:this={canvas}
		></canvas>
	{:else}
		<div class="flex h-full w-full items-center justify-center text-2xs text-faint">—</div>
	{/if}
</div>
