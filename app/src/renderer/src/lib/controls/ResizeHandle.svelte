<!--
	The divider between two panes, draggable along one axis.

	The design system has no splitter: `Card` and `NavBar` are containers,
	`PegboardCanvas` lays tiles out on a grid of its own. This is new, drawn in
	the system's line and action tokens so it reads as the border it replaces.

	It reports a delta in pixels rather than a size, because only the parent
	knows what the panes are allowed to shrink to.
-->
<script lang="ts">
	interface Props {
		/// The direction the handle itself runs: a horizontal bar is dragged up
		/// and down, a vertical one left and right.
		orientation: 'horizontal' | 'vertical';
		label: string;
		/// The size of the pane the handle governs, and the range it may take.
		/// A focusable separator is a window splitter, which has to say where it
		/// currently sits.
		value: number;
		minimum: number;
		maximum: number;
		onResize: (deltaPixels: number) => void;
	}

	const { orientation, label, value, minimum, maximum, onResize }: Props = $props();

	const keyboardStepPixels = 16;

	const axisClasses = {
		horizontal: 'h-1 w-full cursor-row-resize',
		vertical: 'h-full w-1 cursor-col-resize'
	};

	let handle = $state<HTMLDivElement | null>(null);
	let dragOrigin: number | null = null;

	function positionAlongAxis(event: PointerEvent): number {
		if (orientation === 'horizontal') {
			return event.clientY;
		}
		return event.clientX;
	}

	function beginDrag(event: PointerEvent): void {
		event.preventDefault();
		dragOrigin = positionAlongAxis(event);
		handle?.setPointerCapture(event.pointerId);
	}

	function updateDrag(event: PointerEvent): void {
		if (dragOrigin === null) {
			return;
		}
		const position = positionAlongAxis(event);
		onResize(position - dragOrigin);
		dragOrigin = position;
	}

	function endDrag(event: PointerEvent): void {
		dragOrigin = null;
		handle?.releasePointerCapture(event.pointerId);
	}

	/// Arrow keys move the divider the way they point, so a handle is usable
	/// without a pointer.
	function stepForKey(key: string): number | null {
		const steps: Record<string, number> = {
			ArrowUp: -keyboardStepPixels,
			ArrowDown: keyboardStepPixels,
			ArrowLeft: -keyboardStepPixels,
			ArrowRight: keyboardStepPixels
		};
		if (key in steps) {
			return steps[key];
		}
		return null;
	}

	function handleKey(event: KeyboardEvent): void {
		const step = stepForKey(event.key);
		if (step === null) {
			return;
		}
		event.preventDefault();
		onResize(step);
	}
</script>

<!--
	Svelte counts every `separator` as non-interactive. ARIA does not: one that
	takes focus is a window splitter, a widget, and is meant to carry both a
	tab stop and key handling.
-->
<!-- svelte-ignore a11y_no_noninteractive_tabindex -->
<!-- svelte-ignore a11y_no_noninteractive_element_interactions -->
<div
	bind:this={handle}
	class="shrink-0 bg-line transition-colors hover:bg-action focus-visible:bg-action focus-visible:outline-none {axisClasses[
		orientation
	]}"
	role="separator"
	aria-orientation={orientation}
	aria-label={label}
	aria-valuenow={Math.round(value)}
	aria-valuemin={Math.round(minimum)}
	aria-valuemax={Math.round(maximum)}
	tabindex="0"
	onpointerdown={beginDrag}
	onpointermove={updateDrag}
	onpointerup={endDrag}
	onkeydown={handleKey}
></div>
