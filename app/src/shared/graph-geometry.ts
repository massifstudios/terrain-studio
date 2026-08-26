import type { NodeSchema } from './graph-document';

/// Node body metrics, in graph-space pixels. The editor scales the whole
/// surface with one CSS transform, so nothing here needs a zoom factor.
export const nodeWidth = 168;
export const nodeHeaderHeight = 26;
export const nodeThumbnailHeight = 96;
export const nodeFooterHeight = 20;
export const portSpacing = 18;
export const portRadius = 5;

export function nodeHeight(schema: NodeSchema | null): number {
	const portRows = Math.max(schema?.inputs.length ?? 0, schema?.outputs.length ?? 0);
	return (
		nodeHeaderHeight + nodeThumbnailHeight + nodeFooterHeight + Math.max(portRows, 1) * portSpacing
	);
}

export interface Point {
	x: number;
	y: number;
}

function portRowCentre(index: number): number {
	return nodeHeaderHeight + nodeThumbnailHeight + (index + 0.5) * portSpacing;
}

export function inputPortPosition(node: Point, index: number): Point {
	return { x: node.x, y: node.y + portRowCentre(index) };
}

export function outputPortPosition(node: Point, index: number): Point {
	return { x: node.x + nodeWidth, y: node.y + portRowCentre(index) };
}

/// A cubic bézier whose handles run horizontally, so edges leave and enter
/// ports flat and never double back through a node body.
export function edgePath(from: Point, to: Point): string {
	const reach = Math.max(40, Math.abs(to.x - from.x) * 0.5);
	return `M ${from.x} ${from.y} C ${from.x + reach} ${from.y}, ${to.x - reach} ${to.y}, ${to.x} ${to.y}`;
}

export interface Rectangle {
	left: number;
	top: number;
	right: number;
	bottom: number;
}

export function rectangleFromCorners(first: Point, second: Point): Rectangle {
	return {
		left: Math.min(first.x, second.x),
		top: Math.min(first.y, second.y),
		right: Math.max(first.x, second.x),
		bottom: Math.max(first.y, second.y)
	};
}

/// How near the pointer has to come before a port starts reaching for it, and
/// how far a dropped edge will look for a port it missed. Both are generous:
/// the dot is five pixels across and the pointer is not a scalpel.
export const portApproachRadius = 30;
export const portSnapRadius = 22;
const largestPortGrowth = 0.6;

/// A port swells as the pointer nears it, at rest by the edge of the radius.
export function portGrowth(distance: number, radius: number): number {
	if (distance >= radius) {
		return 1;
	}
	return 1 + (1 - distance / radius) * largestPortGrowth;
}

export function distanceBetween(first: Point, second: Point): number {
	return Math.hypot(first.x - second.x, first.y - second.y);
}

export interface PortTarget {
	nodeId: string;
	port: number;
}

export interface InputPortCandidate {
	nodeId: string;
	position: Point;
	inputCount: number;
}

/// The input port nearest a dropped edge, so releasing beside a port connects
/// to it rather than to nothing.
export function nearestInputPort(
	candidates: InputPortCandidate[],
	point: Point,
	radius: number
): PortTarget | null {
	let best: PortTarget | null = null;
	let bestDistance = radius;
	for (const candidate of candidates) {
		const found = nearestPortOfNode(candidate, point, bestDistance);
		if (found !== null) {
			best = found.target;
			bestDistance = found.distance;
		}
	}
	return best;
}

function nearestPortOfNode(
	candidate: InputPortCandidate,
	point: Point,
	within: number
): { target: PortTarget; distance: number } | null {
	let best: { target: PortTarget; distance: number } | null = null;
	for (let index = 0; index < candidate.inputCount; index += 1) {
		const distance = distanceBetween(inputPortPosition(candidate.position, index), point);
		const limit = best?.distance ?? within;
		if (distance < limit) {
			best = { target: { nodeId: candidate.nodeId, port: index }, distance };
		}
	}
	return best;
}

export function nodeIntersects(node: Point, schema: NodeSchema | null, box: Rectangle): boolean {
	const right = node.x + nodeWidth;
	const bottom = node.y + nodeHeight(schema);
	return node.x < box.right && right > box.left && node.y < box.bottom && bottom > box.top;
}
