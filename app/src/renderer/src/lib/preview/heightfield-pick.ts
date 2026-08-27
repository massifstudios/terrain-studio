import type { FieldResult } from '$shared/engine-protocol';
import { cellAtGroundPosition, spanMetersOf } from './terrain-mesh';

/**
 * Picking against the terrain by walking the grid instead of testing every
 * triangle. A 2048² preview mesh has over eight million of them, and Three's
 * raycaster tests all of them for each pointer event, which stalls the frame
 * while the camera is being dragged.
 *
 * Nothing here derives a height: the march compares the ray's altitude against
 * the engine's own samples, read at cell centres.
 */

/// Structural, so a Three `Ray` satisfies it without importing Three here.
export interface PickRay {
	origin: { x: number; y: number; z: number };
	direction: { x: number; y: number; z: number };
}

interface ParameterRange {
	enter: number;
	exit: number;
}

const negligibleDirection = 1e-9;
const refinementSteps = 12;

/// The stretch of the ray that lies within one pair of parallel planes.
function slabRange(
	origin: number,
	direction: number,
	low: number,
	high: number
): ParameterRange | null {
	if (Math.abs(direction) < negligibleDirection) {
		if (origin < low || origin > high) {
			return null;
		}
		return { enter: -Infinity, exit: Infinity };
	}
	const atLow = (low - origin) / direction;
	const atHigh = (high - origin) / direction;
	return { enter: Math.min(atLow, atHigh), exit: Math.max(atLow, atHigh) };
}

/// The stretch of the ray inside the terrain's bounding box.
function boundingRange(field: FieldResult, ray: PickRay): ParameterRange | null {
	const half = spanMetersOf(field) / 2;
	const east = slabRange(ray.origin.x, ray.direction.x, -half, half);
	const altitude = slabRange(ray.origin.y, ray.direction.y, field.minimum, field.maximum);
	const south = slabRange(ray.origin.z, ray.direction.z, -half, half);
	if (!east || !altitude || !south) {
		return null;
	}
	const enter = Math.max(0, east.enter, altitude.enter, south.enter);
	const exit = Math.min(east.exit, altitude.exit, south.exit);
	if (enter > exit) {
		return null;
	}
	return { enter, exit };
}

function clampToGrid(value: number, limit: number): number {
	if (value < 0) {
		return 0;
	}
	if (value > limit) {
		return limit;
	}
	return value;
}

/// The engine's sample for the cell a ground position falls in.
function sampleAtGroundPosition(
	field: FieldResult,
	eastMeters: number,
	southMeters: number
): number {
	const cell = cellAtGroundPosition(field, eastMeters, southMeters);
	const row = clampToGrid(Math.round(cell.row), field.height - 1);
	const column = clampToGrid(Math.round(cell.column), field.width - 1);
	return field.samples[row * field.width + column];
}

/// How far the ray stands above the surface at `distance`. Negative is below.
function clearanceAt(field: FieldResult, ray: PickRay, distance: number): number {
	const eastMeters = ray.origin.x + ray.direction.x * distance;
	const southMeters = ray.origin.z + ray.direction.z * distance;
	const altitudeMeters = ray.origin.y + ray.direction.y * distance;
	return altitudeMeters - sampleAtGroundPosition(field, eastMeters, southMeters);
}

/// One cell of horizontal travel per step, so no column is stepped over.
function marchStep(field: FieldResult, ray: PickRay, range: ParameterRange): number {
	const horizontal = Math.hypot(ray.direction.x, ray.direction.z);
	if (horizontal < negligibleDirection) {
		return range.exit - range.enter;
	}
	return field.cellSizeMeters / horizontal;
}

/// Bisects the step that crossed the surface down to the crossing itself.
function refineCrossing(
	field: FieldResult,
	ray: PickRay,
	above: number,
	below: number
): number {
	let outside = above;
	let inside = below;
	for (let step = 0; step < refinementSteps; step += 1) {
		const middle = (outside + inside) / 2;
		if (clearanceAt(field, ray, middle) > 0) {
			outside = middle;
			continue;
		}
		inside = middle;
	}
	return inside;
}

function cellAtDistance(
	field: FieldResult,
	ray: PickRay,
	distance: number
): { row: number; column: number } {
	return cellAtGroundPosition(
		field,
		ray.origin.x + ray.direction.x * distance,
		ray.origin.z + ray.direction.z * distance
	);
}

/// The grid cell the ray first meets, in continuous grid coordinates.
export function pickHeightfieldCell(
	field: FieldResult,
	ray: PickRay
): { row: number; column: number } | null {
	const range = boundingRange(field, ray);
	if (!range) {
		return null;
	}
	if (clearanceAt(field, ray, range.enter) <= 0) {
		return cellAtDistance(field, ray, range.enter);
	}
	const step = marchStep(field, ray, range);
	if (step <= 0) {
		return null;
	}
	return marchToSurface(field, ray, range, step);
}

function marchToSurface(
	field: FieldResult,
	ray: PickRay,
	range: ParameterRange,
	step: number
): { row: number; column: number } | null {
	let previous = range.enter;
	for (let distance = previous + step; previous < range.exit; distance += step) {
		const bounded = Math.min(distance, range.exit);
		if (clearanceAt(field, ray, bounded) <= 0) {
			return cellAtDistance(field, ray, refineCrossing(field, ray, previous, bounded));
		}
		previous = bounded;
	}
	return null;
}
