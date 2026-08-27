import { describe, expect, test } from 'bun:test';
import type { FieldResult } from '$shared/engine-protocol';
import { pickHeightfieldCell, type PickRay } from './heightfield-pick';

/// A five by five grid of ten metre cells, so the ground spans -20 to 20.
function fieldOf(altitudes: number[]): FieldResult {
	return {
		samples: Float32Array.from(altitudes),
		width: 5,
		height: 5,
		cellSizeMeters: 10,
		minimum: Math.min(...altitudes),
		maximum: Math.max(...altitudes),
		semantics: 'heightfield'
	};
}

function flatGround(): FieldResult {
	return fieldOf(new Array(25).fill(0));
}

/// The same ground with a tall wall along its southern edge, row four.
function groundWithWall(): FieldResult {
	const altitudes = new Array(25).fill(0);
	for (let column = 0; column < 5; column += 1) {
		altitudes[4 * 5 + column] = 100;
	}
	return fieldOf(altitudes);
}

function rayFrom(origin: [number, number, number], direction: [number, number, number]): PickRay {
	const length = Math.hypot(...direction);
	return {
		origin: { x: origin[0], y: origin[1], z: origin[2] },
		direction: {
			x: direction[0] / length,
			y: direction[1] / length,
			z: direction[2] / length
		}
	};
}

describe('picking a terrain cell', () => {
	test('finds the cell straight below a downward ray', () => {
		const cell = pickHeightfieldCell(flatGround(), rayFrom([0, 200, 0], [0, -1, 0]));
		expect(cell?.row).toBeCloseTo(2, 6);
		expect(cell?.column).toBeCloseTo(2, 6);
	});

	test('follows a slanted ray to where it meets the ground', () => {
		const cell = pickHeightfieldCell(flatGround(), rayFrom([0, 50, 60], [0, -1, -1]));
		expect(cell?.row).toBeCloseTo(3, 1);
		expect(cell?.column).toBeCloseTo(2, 1);
	});

	test('stops at the nearest ridge rather than the ground behind it', () => {
		const cell = pickHeightfieldCell(groundWithWall(), rayFrom([0, 50, 60], [0, -1, -1]));
		expect(cell?.row).toBeCloseTo(4, 1);
	});

	test('misses when the ray passes above the terrain', () => {
		expect(pickHeightfieldCell(flatGround(), rayFrom([0, 500, 60], [0, 0, -1]))).toBeNull();
	});

	test('misses when the ray points away from the terrain', () => {
		expect(pickHeightfieldCell(flatGround(), rayFrom([0, 50, 60], [0, 1, 1]))).toBeNull();
	});

	test('misses when the ray passes beside the terrain', () => {
		expect(pickHeightfieldCell(flatGround(), rayFrom([200, 50, 60], [0, -1, -1]))).toBeNull();
	});
});
