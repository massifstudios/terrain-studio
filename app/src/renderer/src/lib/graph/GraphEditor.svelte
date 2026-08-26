<script lang="ts">
	import Warning from 'phosphor-svelte/lib/Warning';
	import type { GraphNodeDocument, NodeSchema } from '$shared/graph-document';
	import { placeholderSchema } from '$shared/graph-operations';
	import { engineClient } from '$lib/engine-client.svelte';
	import {
		edgePath,
		inputPortPosition,
		nearestInputPort,
		nodeHeight,
		nodeIntersects,
		nodeWidth,
		outputPortPosition,
		portSnapRadius,
		rectangleFromCorners,
		type InputPortCandidate,
		type Point,
		type PortTarget
	} from './graph-geometry';
	import { graphStore } from './graph-store.svelte';
	import { nodeStatusStore, type NodeStatus } from './node-status.svelte';
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

	const gridSpacing = 24;
	const cellsPerMajorLine = 5;

	/// The grid is painted on the surface rather than on the transformed layer:
	/// scaling a background of hairlines thickens them, so the spacing follows
	/// the zoom instead and the lines stay one pixel wide.
	const gridStyle = $derived.by(() => {
		const minor = gridSpacing * view.scale;
		const major = minor * cellsPerMajorLine;
		const lines = [
			'linear-gradient(to right, var(--border) 1px, transparent 1px)',
			'linear-gradient(to bottom, var(--border) 1px, transparent 1px)',
			'linear-gradient(to right, var(--border-faint) 1px, transparent 1px)',
			'linear-gradient(to bottom, var(--border-faint) 1px, transparent 1px)'
		];
		const sizes = [`${major}px ${major}px`, `${major}px ${major}px`, `${minor}px ${minor}px`, `${minor}px ${minor}px`];
		return `background-image: ${lines.join(', ')}; background-size: ${sizes.join(', ')}; background-position: ${view.offsetX}px ${view.offsetY}px`;
	});
	let view = $state({ offsetX: 40, offsetY: 20, scale: 1 });
	let pendingEdge = $state<PendingEdge | null>(null);
	let selectionBox = $state<{ start: Point; end: Point } | null>(null);
	let palette = $state<{
		screenX: number;
		screenY: number;
		graph: Point;
		/// Set when the palette was opened by dropping an edge on open canvas.
		source: PendingEdge | null;
	} | null>(null);

	let hoverTarget = $state<HoverTarget | null>(null);
	let isSpaceHeld = $state(false);
	let cursor = $state<Point | null>(null);

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

	/// What the previewed node cost, in the corner of the editor: the node bodies
	/// say the same thing in six-pixel type, and this one is readable.
	const timingReadout = $derived.by(() => {
		const nodeId = graphStore.previewNodeId;
		if (nodeId === null) {
			return null;
		}
		return `${nodeId} · ${timingText(nodeStatusStore.statusOf(nodeId))}`;
	});

	/* ------------------------------------------------------------ errors -- */

	const shownErrorLimit = 3;

	const shownErrors = $derived(nodeStatusStore.errors.slice(0, shownErrorLimit));
	const hiddenErrorCount = $derived(
		Math.max(nodeStatusStore.errors.length - shownErrorLimit, 0)
	);

	/// Puts the node the error belongs to in the middle of the editor and selects
	/// it, so its parameters are the ones the properties panel is showing.
	function focusNode(nodeId: string): void {
		const node = graphStore.findNode(nodeId);
		if (!node || !surface) {
			return;
		}
		const bounds = surface.getBoundingClientRect();
		const centreX = node.positionX + nodeWidth / 2;
		const centreY = node.positionY + nodeHeight(displayedSchema(node)) / 2;
		view = {
			...view,
			offsetX: bounds.width / 2 - centreX * view.scale,
			offsetY: bounds.height / 2 - centreY * view.scale
		};
		graphStore.select([nodeId]);
	}

	function timingText(status: NodeStatus): string {
		if (status.state === 'evaluating') {
			return `${Math.round(status.fraction * 100)}%`;
		}
		if (status.state === 'evaluated') {
			return `${status.milliseconds.toFixed(1)} ms`;
		}
		return status.state;
	}

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
		cursor = toGraphSpace(event);
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

	function inputPortCandidates(): InputPortCandidate[] {
		return graphStore.nodes.map((node) => ({
			nodeId: node.id,
			position: { x: node.positionX, y: node.positionY },
			inputCount: displayedSchema(node).inputs.length
		}));
	}

	function requestFor(edge: PendingEdge, target: PortTarget) {
		return {
			fromNodeId: edge.fromNodeId,
			fromPort: edge.fromPort,
			toNodeId: target.nodeId,
			toPort: target.port
		};
	}

	/// An edge dropped beside a port still lands on it. Only a drop that reached
	/// no port at all gets here; one released on a port has already connected.
	function connectToNearestPort(edge: PendingEdge, point: Point): boolean {
		const target = nearestInputPort(inputPortCandidates(), point, portSnapRadius);
		if (target === null) {
			return false;
		}
		const request = requestFor(edge, target);
		if (!graphStore.canConnect(engineClient.registry, request)) {
			return false;
		}
		graphStore.connect(request);
		return true;
	}

	/// An edge let go over open canvas asks what should be at the other end of
	/// it, and the node picked is wired up where it lands.
	function finishPendingEdge(edge: PendingEdge, event: PointerEvent): void {
		const point = toGraphSpace(event);
		if (connectToNearestPort(edge, point)) {
			return;
		}
		palette = { screenX: event.clientX, screenY: event.clientY, graph: point, source: edge };
	}

	/// While an edge is in flight, every port it could legally land on says so.
	function isPortEligible(nodeId: string, side: 'input' | 'output', index: number): boolean {
		if (!pendingEdge || side !== 'input') {
			return false;
		}
		return graphStore.canConnect(
			engineClient.registry,
			requestFor(pendingEdge, { nodeId, port: index })
		);
	}

	function endGesture(event: PointerEvent): void {
		if (selectionBox) {
			applySelectionBox();
		}
		if (pendingEdge) {
			finishPendingEdge(pendingEdge, event);
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
		palette = {
			screenX: event.clientX,
			screenY: event.clientY,
			graph: toGraphSpace(event),
			source: null
		};
	}

	/// Opened from the keyboard, with no pointer to sit under: the palette goes
	/// to the bottom corner of the editor and the node lands near it.
	function openPaletteAtCorner(): void {
		if (!surface) {
			return;
		}
		const bounds = surface.getBoundingClientRect();
		palette = {
			screenX: bounds.left + 24,
			screenY: bounds.bottom - 24,
			graph: { x: 40, y: 40 },
			source: null
		};
	}

	function insertNode(schema: NodeSchema): void {
		if (!palette) {
			return;
		}
		const nodeId = graphStore.addNode(schema, palette.graph.x, palette.graph.y);
		connectSource(palette.source, nodeId, schema);
		palette = null;
	}

	/// A node created by dropping an edge takes that edge on the first input that
	/// will have it, and none at all if the type suits no input it has.
	function connectSource(source: PendingEdge | null, nodeId: string, schema: NodeSchema): void {
		if (source === null) {
			return;
		}
		for (let port = 0; port < schema.inputs.length; port += 1) {
			const request = requestFor(source, { nodeId, port });
			if (graphStore.canConnect(engineClient.registry, request)) {
				graphStore.connect(request);
				return;
			}
		}
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
			openPaletteAtCorner();
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
	style={gridStyle}
	role="application"
	aria-label="Node graph editor"
	bind:this={surface}
	onpointerdown={beginSurfaceGesture}
	onpointermove={updateGesture}
	onpointerup={endGesture}
	onpointerleave={() => (cursor = null)}
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
				{cursor}
				schema={displayedSchema(node)}
				isPlaceholder={schemaOf(node.typeName) === null}
				isPortEligible={(side, index) => isPortEligible(node.id, side, index)}
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

	<div
		class="pointer-events-none absolute bottom-2 left-2 flex max-w-[28rem] flex-col items-start gap-1"
	>
		{#each shownErrors as failure (failure.nodeId + failure.message)}
			<button
				class="pointer-events-auto flex max-w-full items-center gap-1.5 rounded-sm border border-red bg-red-soft px-2 py-1 text-left text-2xs text-red"
				disabled={failure.nodeId === ''}
				onpointerdown={(event) => event.stopPropagation()}
				onclick={() => focusNode(failure.nodeId)}
			>
				<Warning class="shrink-0" size={12} weight="fill" />
				{#if failure.nodeId}
					<span class="shrink-0 font-mono">{failure.nodeId}</span>
				{/if}
				<span class="truncate">{failure.message}</span>
			</button>
		{/each}
		{#if hiddenErrorCount > 0}
			<span class="px-2 text-2xs text-red">and {hiddenErrorCount} more</span>
		{/if}

		{#if timingReadout}
			<div
				class="pointer-events-none rounded-sm border border-line bg-elevated px-2 py-1 font-mono text-2xs text-dim"
			>
				{timingReadout}
			</div>
		{/if}
	</div>

	{#if graphStore.nodes.length === 0 && !palette}
		<div
			class="pointer-events-none absolute inset-0 flex items-center justify-center text-xs text-faint"
		>
			Right-click or press Tab to add a node
		</div>
	{/if}

	{#if palette}
		<NodePalette
			screenX={palette.screenX}
			screenY={palette.screenY}
			onPick={insertNode}
			onDismiss={() => (palette = null)}
		/>
	{/if}
</div>
