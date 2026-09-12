<!--
	One texture's picture, or a blank of the same shape until it has loaded.
-->
<script lang="ts">
	import { textureThumbnailUrl } from './texture-thumbnails';

	interface Props {
		name: string;
		class?: string;
	}

	const { name, class: className = '' }: Props = $props();

	let url = $state<string | null>(null);

	/// A picture that arrives after the name has moved on is dropped.
	$effect(() => {
		const wanted = name;
		let isCurrent = true;
		url = null;
		textureThumbnailUrl(wanted).then((loaded) => {
			if (isCurrent) {
				url = loaded;
			}
		});
		return () => {
			isCurrent = false;
		};
	});
</script>

{#if url}
	<img src={url} alt={name} class="{className} bg-input object-cover" draggable="false" />
{:else}
	<div class={`${className} bg-input`}></div>
{/if}
