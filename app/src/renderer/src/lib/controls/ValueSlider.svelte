<!--
	A ranged numeric control: scrub the track or type the number.

	The design system has no continuous-value control — `Select` is for discrete
	choices, `WheelColumn` is a scroll picker built for times, and `Checkbox` is
	boolean — so this is new, built from the system's tokens and focus ring.

	Dragging locks the pointer, so the travel is not limited by the width of the
	track or by the edge of the screen: a parameter that runs to ten thousand is
	reachable from a control a hundred pixels wide. What a pixel is worth is
	`scrub.ts`'s business.
-->
<script lang="ts">
	import {
		quantizeValue,
		scrubAcceleration,
		scrubAccelerationLimit,
		scrubQuantum,
		spanOf
	} from './scrub';

	interface Props {
		label: string;
		value: number;
		minimum: number;
		maximum: number;
		/// The quantum the value snaps to. Left out, one is derived from the range.
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
		step,
		unit = '',
		disabled = false,
		onChange
	}: Props = $props();

	const coarseModifierFactor = 10;
	const fineModifierFactor = 0.1;

	let track = $state<HTMLDivElement | null>(null);
	let scrubbedValue: number | null = $state(null);

	const span = $derived(spanOf(minimum, maximum));
	const quantum = $derived(scrubQuantum(minimum, maximum, step));
	const accelerationLimit = $derived(scrubAccelerationLimit(span, quantum));
	const fillPercent = $derived(Math.min(Math.max(((value - minimum) / span) * 100, 0), 100));

	function clampToRange(candidate: number): number {
		if (Number.isNaN(candidate)) {
			return value;
		}
		return Math.min(Math.max(candidate, minimum), maximum);
	}

	function quantize(candidate: number): number {
		if (Number.isNaN(candidate)) {
			return value;
		}
		return quantizeValue(candidate, quantum, minimum, maximum);
	}

	function modifierFactor(event: { shiftKey: boolean; altKey: boolean }): number {
		if (event.shiftKey) {
			return coarseModifierFactor;
		}
		if (event.altKey) {
			return fineModifierFactor;
		}
		return 1;
	}

	/// Without the lock the drag still works, it just stops at the edge of the
	/// screen, so a refusal is nothing to report.
	function lockPointer(target: HTMLElement): void {
		const request: unknown = target.requestPointerLock();
		if (request instanceof Promise) {
			request.catch(() => {});
		}
	}

	function beginScrub(event: PointerEvent): void {
		if (disabled || !track) {
			return;
		}
		event.preventDefault();
		scrubbedValue = value;
		lockPointer(track);
	}

	function updateScrub(event: PointerEvent): void {
		if (scrubbedValue === null) {
			return;
		}
		const pixels = event.movementX;
		const acceleration = scrubAcceleration(pixels, accelerationLimit);
		const travelled = pixels * quantum * acceleration * modifierFactor(event);
		scrubbedValue = clampToRange(scrubbedValue + travelled);
		onChange(quantize(scrubbedValue));
	}

	function endScrub(): void {
		if (scrubbedValue === null) {
			return;
		}
		scrubbedValue = null;
		if (document.pointerLockElement) {
			document.exitPointerLock();
		}
	}

	function stepForKey(key: string): number | null {
		if (key === 'ArrowRight' || key === 'ArrowUp') {
			return quantum;
		}
		if (key === 'ArrowLeft' || key === 'ArrowDown') {
			return -quantum;
		}
		return null;
	}

	function handleKey(event: KeyboardEvent): void {
		const keyStep = stepForKey(event.key);
		if (keyStep === null || disabled) {
			return;
		}
		event.preventDefault();
		onChange(quantize(value + keyStep * modifierFactor(event)));
	}

	/// Escape releases the lock without a pointerup, which ends the drag too.
	$effect(() => {
		const release = (): void => {
			if (!document.pointerLockElement) {
				endScrub();
			}
		};
		document.addEventListener('pointerlockchange', release);
		return () => document.removeEventListener('pointerlockchange', release);
	});

	function handleType(event: Event): void {
		onChange(quantize(Number((event.currentTarget as HTMLInputElement).value)));
	}
</script>

<svelte:document onpointermove={updateScrub} onpointerup={endScrub} />

<div class="flex flex-col gap-1">
	<span class="flex items-baseline justify-between text-2xs text-muted">
		<span>{label}</span>
		{#if unit}
			<span class="text-faint">{unit}</span>
		{/if}
	</span>
	<span class="flex items-center gap-2">
		<div
			bind:this={track}
			class="flex h-4 min-w-0 flex-1 cursor-ew-resize items-center rounded-sm outline-none focus-visible:ring-1 focus-visible:ring-action aria-disabled:cursor-not-allowed"
			role="slider"
			aria-label={label}
			aria-valuenow={value}
			aria-valuemin={minimum}
			aria-valuemax={maximum}
			aria-disabled={disabled}
			tabindex="0"
			onpointerdown={beginScrub}
			onkeydown={handleKey}
		>
			<div class="h-1 w-full overflow-hidden rounded-full bg-input">
				<div class="h-full rounded-full bg-action" style="width: {fillPercent}%"></div>
			</div>
		</div>
		<input
			class="w-20 rounded-sm border border-line bg-input px-2 py-1 text-right font-mono text-2xs text-default outline-none focus:border-line-strong"
			type="number"
			min={minimum}
			max={maximum}
			step={quantum}
			{value}
			{disabled}
			aria-label={label}
			onchange={handleType}
		/>
	</span>
</div>
