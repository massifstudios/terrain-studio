<!--
	A transfer curve drawn by dragging control points.

	Nothing in the design system edits a continuous two-dimensional shape:
	`Select` and `Checkbox` are discrete, `ValueSlider` is one-dimensional, and
	`PegboardCanvas` is a pannable backdrop rather than a control. So this is
	new, built from the system's tokens and focus ring.

	Drag a point to move it, click empty space to add one, right-click a point
	to remove it. The ends stay pinned to the left and right edges, because a
	curve with no value at 0 or 1 would leave part of the field undefined.
-->
<script lang="ts">
	import {
		curvePath,
		parseCurve,
		serializeCurve,
		type CurvePoint
	} from '$shared/transfer-curve';

	interface Props {
		label: string;
		value: string;
		onChange: (value: string) => void;
	}

	const { label, value, onChange }: Props = $props();

	/// The drawing box is a square in view-box units, so a control point stays
	/// round however wide the property panel is.
	const boxSize = 100;
	const grabRadius = 0.05;

	const points = $derived(parseCurve(value));
	const path = $derived(curvePath(points, 64));

	let surface = $state<SVGSVGElement | null>(null);
	let draggingIndex = $state<number | null>(null);

	function toCurveSpace(event: PointerEvent | MouseEvent): CurvePoint {
		const bounds = surface!.getBoundingClientRect();
		return {
			x: Math.min(Math.max((event.clientX - bounds.left) / bounds.width, 0), 1),
			y: Math.min(Math.max(1 - (event.clientY - bounds.top) / bounds.height, 0), 1)
		};
	}

	function nearestPointIndex(target: CurvePoint): number | null {
		let closest: number | null = null;
		let closestDistance = grabRadius;
		points.forEach((point, index) => {
			const distance = Math.hypot(point.x - target.x, point.y - target.y);
			if (distance <= closestDistance) {
				closest = index;
				closestDistance = distance;
			}
		});
		return closest;
	}

	function commit(next: CurvePoint[]): void {
		onChange(serializeCurve(next));
	}

	/// The first and last points carry the ends of the range, so they may rise
	/// and fall but never slide inwards.
	function movedPoint(index: number, target: CurvePoint): CurvePoint {
		if (index === 0) {
			return { x: 0, y: target.y };
		}
		if (index === points.length - 1) {
			return { x: 1, y: target.y };
		}
		const lowerBound = points[index - 1].x + 0.01;
		const upperBound = points[index + 1].x - 0.01;
		return { x: Math.min(Math.max(target.x, lowerBound), upperBound), y: target.y };
	}

	function beginDrag(event: PointerEvent): void {
		const target = toCurveSpace(event);
		const existing = nearestPointIndex(target);
		if (existing !== null) {
			draggingIndex = existing;
			surface?.setPointerCapture(event.pointerId);
			return;
		}

		const inserted = [...points, target].sort((left, right) => left.x - right.x);
		draggingIndex = inserted.indexOf(target);
		surface?.setPointerCapture(event.pointerId);
		commit(inserted);
	}

	function continueDrag(event: PointerEvent): void {
		if (draggingIndex === null) {
			return;
		}
		const next = points.slice();
		next[draggingIndex] = movedPoint(draggingIndex, toCurveSpace(event));
		commit(next);
	}

	function endDrag(event: PointerEvent): void {
		draggingIndex = null;
		surface?.releasePointerCapture(event.pointerId);
	}

	function removePoint(event: MouseEvent): void {
		event.preventDefault();
		const index = nearestPointIndex(toCurveSpace(event));
		if (index === null || index === 0 || index === points.length - 1) {
			return;
		}
		commit(points.filter((_point, at) => at !== index));
	}

	function reset(): void {
		onChange('0,0;1,1');
	}
</script>

<div class="flex flex-col gap-1">
	<span class="flex items-baseline justify-between text-2xs text-muted">
		<span>{label}</span>
		<button class="text-faint hover:text-default" onclick={reset}>Reset</button>
	</span>
	<svg
		class="aspect-square w-full rounded-sm border border-line bg-input"
		viewBox="0 0 {boxSize} {boxSize}"
		preserveAspectRatio="none"
		role="application"
		aria-label="{label} curve"
		bind:this={surface}
		onpointerdown={beginDrag}
		onpointermove={continueDrag}
		onpointerup={endDrag}
		oncontextmenu={removePoint}
	>
		<g stroke="var(--border)" stroke-width="1" vector-effect="non-scaling-stroke">
			{#each [25, 50, 75] as line}
				<line x1={line} y1="0" x2={line} y2={boxSize} />
				<line x1="0" y1={line} x2={boxSize} y2={line} />
			{/each}
		</g>
		<g transform="scale({boxSize})">
			<path
				d={path}
				fill="none"
				stroke="var(--primary)"
				stroke-width="1.5"
				vector-effect="non-scaling-stroke"
			/>
		</g>
		{#each points as point}
			<circle
				cx={point.x * boxSize}
				cy={(1 - point.y) * boxSize}
				r="2.5"
				fill="var(--primary)"
			/>
		{/each}
	</svg>
</div>
