import { describe, expect, test } from 'bun:test';
import {
	quantizeValue,
	scrubAcceleration,
	scrubAccelerationLimit,
	scrubDecimals,
	scrubQuantum
} from './scrub';

describe('the drag quantum', () => {
	test('follows the order of magnitude of the range', () => {
		expect(scrubQuantum(0, 1)).toBeCloseTo(0.001, 6);
		expect(scrubQuantum(0, 360)).toBeCloseTo(0.1, 6);
		expect(scrubQuantum(1, 100000)).toBeCloseTo(10, 6);
	});

	test('yields to a step the caller asked for', () => {
		expect(scrubQuantum(1, 12, 1)).toBe(1);
	});

	test('carries as many decimals as it needs and no more', () => {
		expect(scrubDecimals(1)).toBe(0);
		expect(scrubDecimals(0.001)).toBe(3);
		expect(scrubDecimals(1e-9)).toBe(6);
	});
});

describe('drag acceleration', () => {
	const wideLimit = scrubAccelerationLimit(99999, 10);

	test('leaves a slow drag alone', () => {
		expect(scrubAcceleration(1, wideLimit)).toBe(1);
		expect(scrubAcceleration(-2, wideLimit)).toBe(1);
	});

	test('grows with speed, in either direction', () => {
		expect(scrubAcceleration(12, wideLimit)).toBeCloseTo(4, 6);
		expect(scrubAcceleration(-12, wideLimit)).toBeCloseTo(4, 6);
	});

	test('stops at the ceiling however hard the flick', () => {
		expect(scrubAcceleration(4000, wideLimit)).toBe(wideLimit);
	});

	test('does not accelerate a range a short drag already crosses', () => {
		const octaves = scrubAccelerationLimit(11, 1);
		expect(octaves).toBe(1);
		expect(scrubAcceleration(60, octaves)).toBe(1);
	});
});

describe('quantizing a scrubbed value', () => {
	test('snaps to the quantum without floating-point dust', () => {
		expect(quantizeValue(0.30000000000000004, 0.001, 0, 1)).toBe(0.3);
		expect(quantizeValue(7.6, 1, 1, 12)).toBe(8);
	});

	test('holds the value inside the range', () => {
		expect(quantizeValue(-5, 0.001, 0, 1)).toBe(0);
		expect(quantizeValue(500000, 10, 1, 100000)).toBe(100000);
	});
});
