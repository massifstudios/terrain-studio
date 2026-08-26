import { describe, expect, test } from 'bun:test';
import { curveValueAt, parseCurve, serializeCurve } from './transfer-curve';

describe('parsing', () => {
	test('reads points and sorts them by position', () => {
		expect(parseCurve('1,1;0.5,0.2;0,0')).toEqual([
			{ x: 0, y: 0 },
			{ x: 0.5, y: 0.2 },
			{ x: 1, y: 1 }
		]);
	});

	test('falls back to the identity when the text is unusable', () => {
		for (const text of ['', 'nonsense', '0,0', '0,0;0,1']) {
			expect(parseCurve(text)).toEqual([
				{ x: 0, y: 0 },
				{ x: 1, y: 1 }
			]);
		}
	});

	test('round-trips through text', () => {
		const text = '0,0;0.25,0.7;1,1';
		expect(serializeCurve(parseCurve(text))).toBe(text);
	});
});

describe('evaluation', () => {
	test('the identity curve returns what it is given', () => {
		const points = parseCurve('0,0;1,1');
		expect(curveValueAt(points, 0.25)).toBeCloseTo(0.25, 6);
		expect(curveValueAt(points, 0.75)).toBeCloseTo(0.75, 6);
	});

	test('passes through every point it was given', () => {
		const points = parseCurve('0,0;0.5,0.9;1,1');
		expect(curveValueAt(points, 0.5)).toBeCloseTo(0.9, 6);
	});

	test('never overshoots a steep rise', () => {
		// The same guarantee the engine's curve makes, and the reason both use
		// a monotone fit rather than a natural spline.
		const points = parseCurve('0,0;0.1,0.95;1,1');
		for (let step = 0; step <= 100; step += 1) {
			const value = curveValueAt(points, step / 100);
			expect(value).toBeGreaterThanOrEqual(0);
			expect(value).toBeLessThanOrEqual(1);
		}
	});

	test('reads flat outside its end points', () => {
		const points = parseCurve('0.25,0.4;0.75,0.6');
		expect(curveValueAt(points, 0)).toBeCloseTo(0.4, 6);
		expect(curveValueAt(points, 1)).toBeCloseTo(0.6, 6);
	});
});
