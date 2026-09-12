<!--
	A texture chosen by eye: the current one with its name, and a grid of every
	texture's picture to pick from. `Select` is the design system's discrete
	picker but lists text alone, and `ListRow` lays choices out one per row;
	neither shows what a texture looks like, so this is new, built from the
	system's tokens.
-->
<script lang="ts">
	import CaretDown from 'phosphor-svelte/lib/CaretDown';
	import CaretUp from 'phosphor-svelte/lib/CaretUp';
	import type { EnumOptionSchema } from '$shared/graph-document';
	import TextureThumbnail from './TextureThumbnail.svelte';

	interface Props {
		label: string;
		options: EnumOptionSchema[];
		/// The texture's file name.
		value: string;
		onChange: (value: string) => void;
	}

	const { label, options, value, onChange }: Props = $props();

	let isOpen = $state(false);

	/// A value naming no texture on disk shows as the first, matching what the
	/// viewport tiles for it.
	const chosen = $derived.by(() => {
		const found = options.find((option) => option.name === value);
		if (found) {
			return found;
		}
		if (options.length === 0) {
			return null;
		}
		return options[0];
	});

	const Caret = $derived.by(() => {
		if (isOpen) {
			return CaretUp;
		}
		return CaretDown;
	});

	function frameClass(option: EnumOptionSchema): string {
		if (chosen !== null && option.name === chosen.name) {
			return 'border-action';
		}
		return 'border-line group-hover:border-line-strong';
	}

	function pick(option: EnumOptionSchema): void {
		onChange(option.name);
		isOpen = false;
	}
</script>

<div class="flex flex-col gap-1 text-2xs text-muted">
	<span>{label}</span>
	<button
		class="flex items-center gap-2 rounded-sm border border-line bg-input px-1.5 py-1 text-left hover:border-line-strong"
		onclick={() => (isOpen = !isOpen)}
	>
		{#if chosen}
			<TextureThumbnail name={chosen.name} class="h-6 w-6 shrink-0 rounded-sm" />
			<span class="min-w-0 flex-1 truncate text-default">{chosen.label}</span>
		{:else}
			<span class="min-w-0 flex-1 truncate text-faint">No textures found</span>
		{/if}
		<Caret size={12} class="shrink-0 text-dim" />
	</button>

	{#if isOpen}
		<div class="grid grid-cols-4 gap-1.5 pt-1">
			{#each options as option (option.name)}
				<button
					class="group flex min-w-0 flex-col gap-0.5 text-left"
					title={option.label}
					onclick={() => pick(option)}
				>
					<TextureThumbnail
						name={option.name}
						class="aspect-square w-full rounded-sm border {frameClass(option)}"
					/>
					<span class="w-full truncate text-2xs text-faint">{option.label}</span>
				</button>
			{/each}
		</div>
	{/if}
</div>
