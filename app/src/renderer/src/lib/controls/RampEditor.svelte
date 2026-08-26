<!--
	A colour ramp: a gradient strip over a list of stops.

	Nothing in the design system edits a gradient — `Select` is discrete,
	`ValueSlider` is a single number, and `Checkbox` is boolean — so this is
	new, built from the system's tokens. Colour picking uses the platform's own
	swatch control, which is the one thing the design system deliberately does
	not reimplement.
-->
<script lang="ts">
	import { Button } from '@neoworks-dev/ui';
	import Plus from 'phosphor-svelte/lib/Plus';
	import Trash from 'phosphor-svelte/lib/Trash';
	import { parseRamp, rampGradientCss, serializeRamp, type RampStop } from '$shared/color-ramp';

	interface Props {
		label: string;
		value: string;
		onChange: (value: string) => void;
	}

	const { label, value, onChange }: Props = $props();

	const stops = $derived(parseRamp(value));
	const gradient = $derived(rampGradientCss(stops));

	function commit(next: RampStop[]): void {
		onChange(serializeRamp(next));
	}

	function replaceStop(index: number, changes: Partial<RampStop>): void {
		commit(
			stops.map((stop, at) => {
				if (at !== index) {
					return stop;
				}
				return { ...stop, ...changes };
			})
		);
	}

	/// A new stop lands halfway to the next one, keeping its neighbour's colour
	/// so adding one never changes what the ramp looks like.
	function addStop(): void {
		const last = stops[stops.length - 1];
		const gap = stops[stops.length - 2];
		let position = 0.5;
		if (gap) {
			position = (gap.position + last.position) / 2;
		}
		commit([...stops, { position, color: last.color }]);
	}

	function removeStop(index: number): void {
		if (stops.length <= 2) {
			return;
		}
		commit(stops.filter((_stop, at) => at !== index));
	}
</script>

<div class="flex flex-col gap-2">
	<span class="text-2xs text-muted">{label}</span>
	<div class="h-5 w-full rounded-sm border border-line" style="background: {gradient}"></div>

	<div class="flex flex-col gap-1">
		{#each stops as stop, index}
			<div class="flex items-center gap-2">
				<input
					class="h-5 w-6 shrink-0 cursor-pointer rounded-sm border border-line bg-transparent"
					type="color"
					value={stop.color}
					oninput={(event) => replaceStop(index, { color: event.currentTarget.value })}
				/>
				<input
					class="h-1 min-w-0 flex-1 cursor-pointer appearance-none rounded-full bg-input accent-action outline-none"
					type="range"
					min="0"
					max="1"
					step="0.01"
					value={stop.position}
					oninput={(event) =>
						replaceStop(index, { position: Number(event.currentTarget.value) })}
				/>
				<span class="w-8 shrink-0 text-right font-mono text-2xs text-faint">
					{stop.position.toFixed(2)}
				</span>
				<button
					class="shrink-0 text-faint hover:text-default disabled:opacity-40"
					disabled={stops.length <= 2}
					aria-label="Remove stop"
					onclick={() => removeStop(index)}
				>
					<Trash size={12} />
				</button>
			</div>
		{/each}
	</div>

	<Button size="sm" variant="surface" icon={Plus} onclick={addStop}>Add stop</Button>
</div>
