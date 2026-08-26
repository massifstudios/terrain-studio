<script lang="ts">
	import type { GraphNodeDocument, NodeSchema, PortSchema } from '$shared/graph-document';
	import {
		distanceBetween,
		inputPortPosition,
		nodeHeaderHeight,
		nodeHeight,
		nodeThumbnailHeight,
		nodeWidth,
		outputPortPosition,
		portApproachRadius,
		portGrowth,
		portSpacing,
		type Point
	} from './graph-geometry';
	import { nodeStatusStore, type NodeState } from './node-status.svelte';
	import NodeThumbnailView from './NodeThumbnailView.svelte';

	interface Props {
		node: GraphNodeDocument;
		schema: NodeSchema | null;
		isSelected: boolean;
		isPreviewed: boolean;
		/// The registry does not know this type: a plugin is missing.
		isPlaceholder: boolean;
		/// Where the pointer is in graph space, so ports can reach for it.
		cursor: Point | null;
		isPortEligible: (side: 'input' | 'output', index: number) => boolean;
		onPointerDown: (event: PointerEvent) => void;
		onPortPointerDown: (event: PointerEvent, side: 'input' | 'output', index: number) => void;
		onPortPointerUp: (event: PointerEvent, side: 'input' | 'output', index: number) => void;
		onPortHover: (side: 'input' | 'output', index: number) => void;
		onPortLeave: () => void;
	}

	const {
		node,
		schema,
		isSelected,
		isPreviewed,
		isPlaceholder,
		cursor,
		isPortEligible,
		onPointerDown,
		onPortPointerDown,
		onPortPointerUp,
		onPortHover,
		onPortLeave
	}: Props = $props();

	const status = $derived(nodeStatusStore.statusOf(node.id));
	const thumbnail = $derived(nodeStatusStore.thumbnails[node.id]);

	const inputs = $derived.by((): PortSchema[] => {
		if (!schema) {
			return [];
		}
		return schema.inputs;
	});

	const outputs = $derived.by((): PortSchema[] => {
		if (!schema) {
			return [];
		}
		return schema.outputs;
	});

	const portRowsHeight = $derived(
		Math.max(inputs.length, outputs.length, 1) * portSpacing
	);

	const stateColour: Record<NodeState, string> = {
		dirty: 'bg-line-strong',
		evaluating: 'bg-amber',
		cached: 'bg-blue',
		evaluated: 'bg-green',
		failed: 'bg-red'
	};

	function dotColour(): string {
		if (isPlaceholder) {
			return 'bg-red';
		}
		return stateColour[status.state];
	}

	/// How far a port's hit area reaches past its row, so a pointer a few pixels
	/// off still lands on it.
	const portHitPadding = 5;

	const nodePosition = $derived({ x: node.positionX, y: node.positionY });

	function portPosition(side: 'input' | 'output', index: number): Point {
		if (side === 'input') {
			return inputPortPosition(nodePosition, index);
		}
		return outputPortPosition(nodePosition, index);
	}

	function portScale(side: 'input' | 'output', index: number): number {
		if (!cursor) {
			return 1;
		}
		return portGrowth(distanceBetween(portPosition(side, index), cursor), portApproachRadius);
	}

	/// White marks a port the pointer can act on: one it has come close enough to
	/// grab, or one the edge in flight is allowed to land on.
	function isPortLit(side: 'input' | 'output', index: number): boolean {
		if (isPortEligible(side, index)) {
			return true;
		}
		return portScale(side, index) > 1;
	}

	function timingLabel(): string {
		if (isPlaceholder) {
			return 'not installed';
		}
		if (status.state === 'cached') {
			return 'cached';
		}
		if (status.state === 'evaluated') {
			return `${status.milliseconds.toFixed(1)} ms`;
		}
		return status.state;
	}
</script>

<div
	class="absolute select-none rounded-md border bg-elevated shadow-sm"
	class:border-line={!isSelected}
	class:border-action={isSelected}
	style="left: {node.positionX}px; top: {node.positionY}px; width: {nodeWidth}px; height: {nodeHeight(
		schema
	)}px"
	role="button"
	tabindex="0"
	onpointerdown={onPointerDown}
>
	<div
		class="flex items-center gap-1 truncate rounded-t-md border-b border-line px-2 text-2xs"
		style="height: {nodeHeaderHeight}px"
	>
		<span class="h-1.5 w-1.5 shrink-0 rounded-full {dotColour()}"></span>
		<span class="truncate font-medium">{node.id}</span>
		{#if isPreviewed}
			<span class="ml-auto shrink-0 text-2xs text-dim">preview</span>
		{/if}
	</div>

	<div style="height: {nodeThumbnailHeight}px">
		<NodeThumbnailView {thumbnail} isStale={status.state === 'dirty'} />
	</div>

	<div class="relative" style="height: {portRowsHeight}px">
		{#each inputs as port, index}
			<button
				class="absolute flex items-center gap-1 pl-2 text-2xs text-muted"
				style="left: -13px; top: {index * portSpacing - portHitPadding}px; height: {portSpacing +
					portHitPadding * 2}px"
				title={port.description}
				onpointerdown={(event) => onPortPointerDown(event, 'input', index)}
				onpointerup={(event) => onPortPointerUp(event, 'input', index)}
				onpointerenter={() => onPortHover('input', index)}
				onpointerleave={onPortLeave}
			>
				<span
					class="h-2.5 w-2.5 shrink-0 rounded-full border border-line-strong transition-transform"
					class:bg-default={isPortLit('input', index)}
					class:bg-canvas={port.optional && !isPortLit('input', index)}
					class:bg-line-strong={!port.optional && !isPortLit('input', index)}
					style="transform: scale({portScale('input', index)})"
				></span>
				<span class="pointer-events-none">{port.name}</span>
			</button>
		{/each}

		{#each outputs as port, index}
			<button
				class="absolute flex flex-row-reverse items-center gap-1 pr-2 text-2xs text-muted"
				style="right: -13px; top: {index * portSpacing - portHitPadding}px; height: {portSpacing +
					portHitPadding * 2}px"
				title={port.description}
				onpointerdown={(event) => onPortPointerDown(event, 'output', index)}
				onpointerup={(event) => onPortPointerUp(event, 'output', index)}
			>
				<span
					class="h-2.5 w-2.5 shrink-0 rounded-full border border-line-strong transition-transform"
					class:bg-default={isPortLit('output', index)}
					class:bg-line-strong={!isPortLit('output', index)}
					style="transform: scale({portScale('output', index)})"
				></span>
				<span class="pointer-events-none">{port.name}</span>
			</button>
		{/each}
	</div>

	<div class="flex items-center justify-between px-2 text-2xs text-faint">
		<span class="truncate">{node.typeName}</span>
		<span class="shrink-0 font-mono">{timingLabel()}</span>
	</div>
</div>
