<script lang="ts">
	import { Button, Checkbox, Select } from '@neoworks-dev/ui';
	import ArrowsClockwise from 'phosphor-svelte/lib/ArrowsClockwise';
	import FolderOpen from 'phosphor-svelte/lib/FolderOpen';
	import type { ParameterSchema, ParameterValue } from '$shared/graph-document';
	import CurveEditor from '$lib/controls/CurveEditor.svelte';
	import ValueSlider from '$lib/controls/ValueSlider.svelte';

	interface Props {
		schema: ParameterSchema;
		value: ParameterValue;
		onChange: (value: ParameterValue) => void;
	}

	const { schema, value, onChange }: Props = $props();

	const numberValue = $derived(Number(value));
	const textValue = $derived(String(value));

	const stepForType = $derived.by(() => {
		if (schema.type === 'int') {
			return 1;
		}
		return (schema.maximum - schema.minimum) / 1000;
	});

	const enumOptions = $derived(
		schema.options.map((option) => ({ value: String(option.value), label: option.label }))
	);

	/// Seeds are drawn from the same 32-bit space the engine hashes.
	function rerollSeed(): void {
		onChange(Math.floor(Math.random() * 0xffffffff));
	}

	async function browse(): Promise<void> {
		const chosen = await window.fileBridge.chooseOpenPath('elevation');
		if (chosen !== null) {
			onChange(chosen);
		}
	}

	function fileName(path: string): string {
		const cut = Math.max(path.lastIndexOf('/'), path.lastIndexOf('\\'));
		return path.slice(cut + 1);
	}
</script>

{#if schema.type === 'bool'}
	<label class="flex items-center justify-between gap-2 text-2xs text-muted">
		<span>{schema.label}</span>
		<Checkbox
			checked={Boolean(value)}
			onchange={(event: Event) => onChange((event.currentTarget as HTMLInputElement).checked)}
		/>
	</label>
{:else if schema.type === 'enum'}
	<label class="flex flex-col gap-1 text-2xs text-muted">
		<span>{schema.label}</span>
		<Select
			value={String(numberValue)}
			options={enumOptions}
			onChange={(picked) => onChange(Number(picked))}
		/>
	</label>
{:else if schema.type === 'seed'}
	<label class="flex flex-col gap-1 text-2xs text-muted">
		<span>{schema.label}</span>
		<span class="flex items-center gap-2">
			<input
				class="min-w-0 flex-1 rounded-sm border border-line bg-input px-2 py-1 font-mono text-2xs text-default outline-none focus:border-line-strong"
				type="number"
				value={numberValue}
				onchange={(event) => onChange(Number(event.currentTarget.value))}
			/>
			<Button size="sm" variant="surface" icon={ArrowsClockwise} onclick={rerollSeed}>Roll</Button>
		</span>
	</label>
{:else if schema.type === 'path'}
	<div class="flex flex-col gap-1 text-2xs text-muted">
		<span>{schema.label}</span>
		<span class="flex items-center gap-2">
			<span
				class="min-w-0 flex-1 truncate rounded-sm border border-line bg-input px-2 py-1 font-mono text-2xs text-default"
				title={textValue}
			>
				{fileName(textValue)}
			</span>
			<Button size="sm" variant="surface" icon={FolderOpen} onclick={browse}>Browse</Button>
		</span>
	</div>
{:else if schema.type === 'curve'}
	<CurveEditor label={schema.label} value={textValue} onChange={onChange} />
{:else if schema.type === 'text'}
	<label class="flex flex-col gap-1 text-2xs text-muted">
		<span>{schema.label}</span>
		<input
			class="rounded-sm border border-line bg-input px-2 py-1 text-2xs text-default outline-none focus:border-line-strong"
			value={textValue}
			onchange={(event) => onChange(event.currentTarget.value)}
		/>
	</label>
{:else}
	<ValueSlider
		label={schema.label}
		unit={schema.unit}
		value={numberValue}
		minimum={schema.minimum}
		maximum={schema.maximum}
		step={stepForType}
		onChange={onChange}
	/>
{/if}
