import { describe, expect, test } from 'bun:test';
import {
	inputPortPosition,
	nearestInputPort,
	portGrowth,
	type InputPortCandidate
} from './graph-geometry';

const candidates: InputPortCandidate[] = [
	{ nodeId: 'blur', position: { x: 0, y: 0 }, inputCount: 2 },
	{ nodeId: 'erode', position: { x: 400, y: 0 }, inputCount: 1 }
];

describe('dropping an edge near a port', () => {
	test('lands on the port it was released beside', () => {
		const beside = inputPortPosition({ x: 0, y: 0 }, 1);
		expect(nearestInputPort(candidates, { x: beside.x + 6, y: beside.y - 4 }, 22)).toEqual({
			nodeId: 'blur',
			port: 1
		});
	});

	test('lands on nothing when it was released nowhere near one', () => {
		expect(nearestInputPort(candidates, { x: 200, y: 200 }, 22)).toBeNull();
	});

	test('prefers the nearer port when two are in reach', () => {
		const first = inputPortPosition({ x: 0, y: 0 }, 0);
		const second = inputPortPosition({ x: 0, y: 0 }, 1);
		const between = { x: first.x, y: (first.y + second.y) / 2 - 2 };
		expect(nearestInputPort(candidates, between, 40)).toEqual({ nodeId: 'blur', port: 0 });
	});

	test('reaches across nodes, not only within one', () => {
		const far = inputPortPosition({ x: 400, y: 0 }, 0);
		expect(nearestInputPort(candidates, { x: far.x - 5, y: far.y }, 22)).toEqual({
			nodeId: 'erode',
			port: 0
		});
	});
});

describe('a port reaching for the pointer', () => {
	test('sits still until the pointer is inside the radius', () => {
		expect(portGrowth(30, 30)).toBe(1);
		expect(portGrowth(45, 30)).toBe(1);
	});

	test('grows as the pointer closes in', () => {
		expect(portGrowth(15, 30)).toBeCloseTo(1.3, 6);
		expect(portGrowth(0, 30)).toBeCloseTo(1.6, 6);
	});
});
