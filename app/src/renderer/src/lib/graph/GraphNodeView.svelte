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
		portSnapRadius,
		portSpacing,
		type Point
	} from './graph-geometry';
	import Warning from 'phosphor-svelte/lib/Warning';
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
	const thumbnail = $derived(nodeStatusStore.thumbnailOf(node.id));
	const failure = $derived(nodeStatusStore.errorOf(node.id));

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

	function borderColour(): string {
		if (failure) {
			return 'border-red';
		}
		if (isSelected) {
			return 'border-action';
		}
		return 'border-line';
	}

	/// The node carries its own failure: hovering it says what the engine
	/// refused, so nothing has to be read off a banner somewhere else.
	function failureMessage(): string | undefined {
		if (!failure) {
			return undefined;
		}
		return failure.message;
	}

	/// An error the engine blamed on one input port marks that port, so a node
	/// with four inputs does not leave the reader guessing which is empty.
	function portBorderColour(portName: string): string {
		if (failure && failure.portName === portName) {
			return 'border-red';
		}
		return 'border-line-strong';
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

	/// White marks a port a drop would actually land on: legal for the edge in
	/// flight, and near enough for the release to snap to it. Grabbing a port is
	/// the hover state, which the browser's own hit test decides.
	function isPortLit(side: 'input' | 'output', index: number): boolean {
		if (!cursor || !isPortEligible(side, index)) {
			return false;
		}
		return distanceBetween(portPosition(side, index), cursor) <= portSnapRadius;
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
	class="absolute select-none rounded-md border bg-elevated shadow-sm {borderColour()}"
	style="left: {node.positionX}px; top: {node.positionY}px; width: {nodeWidth}px; height: {nodeHeight(
		schema
	)}px"
	role="button"
	tabindex="0"
	title={failureMessage()}
	onpointerdown={onPointerDown}
>
	<div
		class="flex items-center gap-1 truncate rounded-t-md border-b border-line px-2 text-2xs"
		style="height: {nodeHeaderHeight}px"
	>
		<span class="h-1.5 w-1.5 shrink-0 rounded-full {dotColour()}"></span>
		<span class="truncate font-medium">{node.id}</span>
		<span class="ml-auto flex shrink-0 items-center gap-1">
			{#if failure}
				<Warning class="text-red" size={11} weight="fill" />
			{/if}
			{#if isPreviewed}
				<span class="text-2xs text-dim">preview</span>
			{/if}
		</span>
	</div>

	<div class="relative" style="height: {nodeThumbnailHeight}px">
		<NodeThumbnailView {thumbnail} isStale={status.state === 'dirty'} />
		{#if status.state === 'evaluating'}
			<div class="absolute inset-x-0 bottom-0 h-0.5 bg-input">
				<div
					class="h-full bg-amber transition-[width] duration-100"
					style="width: {status.fraction * 100}%"
				></div>
			</div>
		{/if}
	</div>

	<div class="relative" style="height: {portRowsHeight}px">
		{#each inputs as port, index}
			<button
				class="group absolute flex items-center gap-1 pl-2 text-2xs text-muted"
				style="left: -13px; top: {index * portSpacing - portHitPadding}px; height: {portSpacing +
					portHitPadding * 2}px"
				title={port.description}
				onpointerdown={(event) => onPortPointerDown(event, 'input', index)}
				onpointerup={(event) => onPortPointerUp(event, 'input', index)}
				onpointerenter={() => onPortHover('input', index)}
				onpointerleave={onPortLeave}
			>
				<span
					class="h-2.5 w-2.5 shrink-0 rounded-full border transition-transform group-hover:bg-default {portBorderColour(
						port.name
					)}"
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
				class="group absolute flex flex-row-reverse items-center gap-1 pr-2 text-2xs text-muted"
				style="right: -13px; top: {index * portSpacing - portHitPadding}px; height: {portSpacing +
					portHitPadding * 2}px"
				title={port.description}
				onpointerdown={(event) => onPortPointerDown(event, 'output', index)}
				onpointerup={(event) => onPortPointerUp(event, 'output', index)}
			>
				<span
					class="h-2.5 w-2.5 shrink-0 rounded-full border border-line-strong transition-transform group-hover:bg-default"
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
