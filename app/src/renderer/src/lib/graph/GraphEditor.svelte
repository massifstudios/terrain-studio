<script lang="ts">
	import type { GraphNodeDocument, NodeSchema } from '$shared/graph-document';
	import { placeholderSchema } from '$shared/graph-operations';
	import { engineClient } from '$lib/engine-client.svelte';
	import {
		edgePath,
		inputPortPosition,
		nodeIntersects,
		outputPortPosition,
		rectangleFromCorners,
		type Point
	} from './graph-geometry';
	import { graphStore } from './graph-store.svelte';
	import GraphNodeView from './GraphNodeView.svelte';
	import NodePalette from './NodePalette.svelte';
	import { findNodeSchema } from '$shared/registry-schema';

	interface PendingEdge {
		fromNodeId: string;
		fromPort: number;
		cursor: Point;
	}

	interface HoverTarget {
		nodeId: string;
		port: number;
	}

	let surface = $state<HTMLDivElement | null>(null);
	let surfaceWidth = $state(0);
	let surfaceHeight = $state(0);
	let view = $state({ offsetX: 40, offsetY: 20, scale: 1 });
	let pendingEdge = $state<PendingEdge | null>(null);
	let selectionBox = $state<{ start: Point; end: Point } | null>(null);
	let palette = $state<{ screenX: number; screenY: number; graph: Point } | null>(null);

	let hoverTarget = $state<HoverTarget | null>(null);
	let isSpaceHeld = $state(false);

	let panOrigin: Point | null = null;
	let dragOrigin: Point | null = null;

	function schemaOf(typeName: string): NodeSchema | null {
		return findNodeSchema(engineClient.registry, typeName);
	}

	/// A node whose type is not installed still draws, with ports read back off
	/// the edges it already has.
	function displayedSchema(node: GraphNodeDocument): NodeSchema {
		const known = schemaOf(node.typeName);
		if (known) {
			return known;
		}
		return placeholderSchema(node, graphStore.edges);
	}

	function toGraphSpace(event: { clientX: number; clientY: number }): Point {
		const bounds = surface!.getBoundingClientRect();
		return {
			x: (event.clientX - bounds.left - view.offsetX) / view.scale,
			y: (event.clientY - bounds.top - view.offsetY) / view.scale
		};
	}

	/* ------------------------------------------------------------- edges -- */

	const edgeShapes = $derived.by(() => {
		return graphStore.edges.flatMap((edge) => {
			const from = graphStore.findNode(edge.fromNodeId);
			const to = graphStore.findNode(edge.toNodeId);
			if (!from || !to) {
				return [];
			}
			const start = outputPortPosition({ x: from.positionX, y: from.positionY }, edge.fromPort);
			const end = inputPortPosition({ x: to.positionX, y: to.positionY }, edge.toPort);
			return [{ key: `${edge.toNodeId}:${edge.toPort}`, path: edgePath(start, end) }];
		});
	});

	const pendingPath = $derived.by(() => {
		if (!pendingEdge) {
			return null;
		}
		const from = graphStore.findNode(pendingEdge.fromNodeId);
		if (!from) {
			return null;
		}
		const start = outputPortPosition(
			{ x: from.positionX, y: from.positionY },
			pendingEdge.fromPort
		);
		return edgePath(start, pendingEdge.cursor);
	});

	/* ------------------------------------------------------- interaction -- */

	function beginNodeDrag(event: PointerEvent, nodeId: string): void {
		event.stopPropagation();
		if (event.shiftKey) {
			graphStore.select([...graphStore.selectedNodeIds, nodeId]);
		} else if (!graphStore.selectedNodeIds.includes(nodeId)) {
			graphStore.select([nodeId]);
		} else {
			graphStore.previewNodeId = nodeId;
		}
		dragOrigin = toGraphSpace(event);
		surface?.setPointerCapture(event.pointerId);
	}

	function beginPortDrag(
		event: PointerEvent,
		nodeId: string,
		side: 'input' | 'output',
		index: number
	): void {
		event.stopPropagation();
		if (side === 'input') {
			graphStore.disconnectInput(nodeId, index);
			return;
		}
		// No pointer capture here: the drop has to reach the port it lands on,
		// and moves still bubble up to the surface because ports live inside it.
		pendingEdge = { fromNodeId: nodeId, fromPort: index, cursor: toGraphSpace(event) };
	}

	function completePortDrag(nodeId: string, side: 'input' | 'output', index: number): void {
		if (!pendingEdge || side !== 'input') {
			return;
		}
		const request = {
			fromNodeId: pendingEdge.fromNodeId,
			fromPort: pendingEdge.fromPort,
			toNodeId: nodeId,
			toPort: index
		};
		if (graphStore.canConnect(engineClient.registry, request)) {
			graphStore.connect(request);
		}
		pendingEdge = null;
	}

	/// Whether the edge being dragged would be accepted where the pointer is.
	const isPendingEdgeValid = $derived.by(() => {
		if (!pendingEdge || !hoverTarget) {
			return true;
		}
		return graphStore.canConnect(engineClient.registry, {
			fromNodeId: pendingEdge.fromNodeId,
			fromPort: pendingEdge.fromPort,
			toNodeId: hoverTarget.nodeId,
			toPort: hoverTarget.port
		});
	});

	function pendingEdgeStroke(): string {
		if (isPendingEdgeValid) {
			return 'var(--primary)';
		}
		return 'var(--ctx-red)';
	}

	function beginSurfaceGesture(event: PointerEvent): void {
		if (palette) {
			palette = null;
		}
		if (event.button === 1 || isSpaceHeld) {
			panOrigin = { x: event.clientX, y: event.clientY };
			surface?.setPointerCapture(event.pointerId);
			return;
		}
		if (event.button !== 0) {
			return;
		}
		const start = toGraphSpace(event);
		selectionBox = { start, end: start };
		graphStore.select([]);
		surface?.setPointerCapture(event.pointerId);
	}

	function updateGesture(event: PointerEvent): void {
		if (panOrigin) {
			view = {
				...view,
				offsetX: view.offsetX + (event.clientX - panOrigin.x),
				offsetY: view.offsetY + (event.clientY - panOrigin.y)
			};
			panOrigin = { x: event.clientX, y: event.clientY };
			return;
		}
		if (dragOrigin) {
			const now = toGraphSpace(event);
			graphStore.moveNodes(graphStore.selectedNodeIds, now.x - dragOrigin.x, now.y - dragOrigin.y);
			dragOrigin = now;
			return;
		}
		if (pendingEdge) {
			pendingEdge = { ...pendingEdge, cursor: toGraphSpace(event) };
			return;
		}
		if (selectionBox) {
			selectionBox = { ...selectionBox, end: toGraphSpace(event) };
		}
	}

	function endGesture(event: PointerEvent): void {
		if (selectionBox) {
			applySelectionBox();
		}
		panOrigin = null;
		dragOrigin = null;
		selectionBox = null;
		pendingEdge = null;
		hoverTarget = null;
		surface?.releasePointerCapture(event.pointerId);
	}

	function applySelectionBox(): void {
		if (!selectionBox) {
			return;
		}
		const box = rectangleFromCorners(selectionBox.start, selectionBox.end);
		const inside = graphStore.nodes
			.filter((node) =>
				nodeIntersects({ x: node.positionX, y: node.positionY }, schemaOf(node.typeName), box)
			)
			.map((node) => node.id);
		if (inside.length > 0) {
			graphStore.select(inside);
		}
	}

	/// Wheel zooms about the cursor, so the node under it stays put.
	function zoom(event: WheelEvent): void {
		event.preventDefault();
		const bounds = surface!.getBoundingClientRect();
		const pointerX = event.clientX - bounds.left;
		const pointerY = event.clientY - bounds.top;
		const factor = Math.exp(-event.deltaY * 0.0012);
		const scale = Math.min(Math.max(view.scale * factor, 0.2), 2.5);
		view = {
			scale,
			offsetX: pointerX - ((pointerX - view.offsetX) / view.scale) * scale,
			offsetY: pointerY - ((pointerY - view.offsetY) / view.scale) * scale
		};
	}

	function openPalette(event: MouseEvent): void {
		event.preventDefault();
		const bounds = surface!.getBoundingClientRect();
		palette = {
			screenX: event.clientX - bounds.left,
			screenY: event.clientY - bounds.top,
			graph: toGraphSpace(event)
		};
	}

	function insertNode(schema: NodeSchema): void {
		if (!palette) {
			return;
		}
		graphStore.addNode(schema, palette.graph.x, palette.graph.y);
		palette = null;
	}

	/// A key pressed while typing belongs to the field, not to the editor.
	function isTyping(event: KeyboardEvent): boolean {
		const target = event.target as HTMLElement | null;
		if (!target) {
			return false;
		}
		return target.tagName === 'INPUT' || target.tagName === 'TEXTAREA' || target.isContentEditable;
	}

	function handleKeyUp(event: KeyboardEvent): void {
		if (event.key === ' ') {
			isSpaceHeld = false;
		}
	}

	function handleKey(event: KeyboardEvent): void {
		if (isTyping(event)) {
			return;
		}
		if (event.key === ' ') {
			isSpaceHeld = true;
			return;
		}
		if (event.key === 'Tab') {
			event.preventDefault();
			palette = { screenX: 24, screenY: 24, graph: { x: 40, y: 40 } };
			return;
		}
		if (event.key === 'Delete' || event.key === 'Backspace') {
			graphStore.removeNodes(graphStore.selectedNodeIds);
		}
	}
</script>

<svelte:window onkeydown={handleKey} onkeyup={handleKeyUp} />

<div
	class="relative h-full w-full overflow-hidden bg-canvas"
	role="application"
	aria-label="Node graph editor"
	bind:this={surface}
	bind:clientWidth={surfaceWidth}
	bind:clientHeight={surfaceHeight}
	onpointerdown={beginSurfaceGesture}
	onpointermove={updateGesture}
	onpointerup={endGesture}
	oncontextmenu={openPalette}
	onwheel={zoom}
>
	<div
		class="absolute left-0 top-0 origin-top-left"
		style="transform: translate({view.offsetX}px, {view.offsetY}px) scale({view.scale})"
	>
		<svg class="pointer-events-none absolute overflow-visible" width="1" height="1">
			{#each edgeShapes as shape (shape.key)}
				<path d={shape.path} fill="none" stroke="var(--border-strong)" stroke-width="1.5" />
			{/each}
			{#if pendingPath}
				<path
					d={pendingPath}
					fill="none"
					stroke={pendingEdgeStroke()}
					stroke-width="1.5"
					stroke-dasharray="4 3"
				/>
			{/if}
		</svg>

		{#each graphStore.nodes as node (node.id)}
			<GraphNodeView
				{node}
				schema={displayedSchema(node)}
				isPlaceholder={schemaOf(node.typeName) === null}
				isSelected={graphStore.selectedNodeIds.includes(node.id)}
				isPreviewed={graphStore.previewNodeId === node.id}
				onPointerDown={(event) => beginNodeDrag(event, node.id)}
				onPortPointerDown={(event, side, index) => beginPortDrag(event, node.id, side, index)}
				onPortPointerUp={(_event, side, index) => completePortDrag(node.id, side, index)}
				onPortHover={(side, index) => {
					if (side === 'input') {
						hoverTarget = { nodeId: node.id, port: index };
					}
				}}
				onPortLeave={() => (hoverTarget = null)}
			/>
		{/each}

		{#if selectionBox}
			<div
				class="pointer-events-none absolute border border-action bg-action/10"
				style="left: {Math.min(selectionBox.start.x, selectionBox.end.x)}px; top: {Math.min(
					selectionBox.start.y,
					selectionBox.end.y
				)}px; width: {Math.abs(selectionBox.end.x - selectionBox.start.x)}px; height: {Math.abs(
					selectionBox.end.y - selectionBox.start.y
				)}px"
			></div>
		{/if}
	</div>

	{#if palette}
		<NodePalette
			screenX={palette.screenX}
			screenY={palette.screenY}
			{surfaceWidth}
			{surfaceHeight}
			onPick={insertNode}
			onDismiss={() => (palette = null)}
		/>
	{/if}

	{#if graphStore.nodes.length === 0}
		<div class="pointer-events-none absolute inset-0 flex items-center justify-center text-xs text-faint">
			Right-click or press Tab to add a node
		</div>
	{/if}
</div>
