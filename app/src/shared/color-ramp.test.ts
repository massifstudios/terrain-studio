import { describe, expect, test } from 'bun:test';
import { parseRamp, rampGradientCss, serializeRamp } from './color-ramp';

describe('parsing', () => {
	test('reads stops and sorts them by position', () => {
		expect(parseRamp('1:#ffffff;0.5:#808080;0:#000000')).toEqual([
			{ position: 0, color: '#000000' },
			{ position: 0.5, color: '#808080' },
			{ position: 1, color: '#ffffff' }
		]);
	});

	test('falls back to black and white when the text is unusable', () => {
		for (const text of ['', 'nonsense', '0:#zzz', '0:red']) {
			expect(parseRamp(text)).toEqual([
				{ position: 0, color: '#000000' },
				{ position: 1, color: '#ffffff' }
			]);
		}
	});

	test('drops only the stops it cannot read', () => {
		expect(parseRamp('0:#000000;broken;1:#ffffff')).toHaveLength(2);
	});

	test('round-trips through text', () => {
		const text = '0:#3f5545;0.6:#8d7b5f;1:#ffffff';
		expect(serializeRamp(parseRamp(text))).toBe(text);
	});
});

describe('display', () => {
	test('becomes a CSS gradient the browser can draw', () => {
		expect(rampGradientCss(parseRamp('0:#000000;1:#ffffff'))).toBe(
			'linear-gradient(to right, #000000 0%, #ffffff 100%)'
		);
	});
});
