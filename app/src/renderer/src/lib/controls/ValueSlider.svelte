<!--
	A ranged numeric control: drag the track or type the number.

	The design system has no continuous-value control — `Select` is for discrete
	choices, `WheelColumn` is a scroll picker built for times, and `Checkbox` is
	boolean — so this is new, built from the system's tokens and focus ring.
-->
<script lang="ts">
	interface Props {
		label: string;
		value: number;
		minimum: number;
		maximum: number;
		step?: number;
		unit?: string;
		disabled?: boolean;
		onChange: (value: number) => void;
	}

	const {
		label,
		value,
		minimum,
		maximum,
		step = 0.01,
		unit = '',
		disabled = false,
		onChange
	}: Props = $props();

	function clampToRange(candidate: number): number {
		if (Number.isNaN(candidate)) {
			return value;
		}
		return Math.min(Math.max(candidate, minimum), maximum);
	}

	function handleSlide(event: Event): void {
		onChange(clampToRange(Number((event.currentTarget as HTMLInputElement).value)));
	}

	function handleType(event: Event): void {
		onChange(clampToRange(Number((event.currentTarget as HTMLInputElement).value)));
	}
</script>

<label class="flex flex-col gap-1">
	<span class="flex items-baseline justify-between text-2xs text-muted">
		<span>{label}</span>
		{#if unit}
			<span class="text-faint">{unit}</span>
		{/if}
	</span>
	<span class="flex items-center gap-2">
		<input
			class="h-1 min-w-0 flex-1 cursor-pointer appearance-none rounded-full bg-input accent-action outline-none disabled:cursor-not-allowed"
			type="range"
			min={minimum}
			max={maximum}
			{step}
			{value}
			{disabled}
			oninput={handleSlide}
		/>
		<input
			class="w-20 rounded-sm border border-line bg-input px-2 py-1 text-right font-mono text-2xs text-default outline-none focus:border-line-strong"
			type="number"
			min={minimum}
			max={maximum}
			{step}
			{value}
			{disabled}
			onchange={handleType}
		/>
	</span>
</label>
