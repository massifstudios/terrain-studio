import { describe, expect, test } from 'bun:test';
import {
	UsageError,
	hasFlag,
	optionalNumber,
	optionalText,
	parseArguments,
	positionalAt
} from './arguments';

describe('parseArguments', () => {
	test('takes the first bare word as the command', () => {
		const parsed = parseArguments(['build', 'valley.massifgraph']);
		expect(parsed.command).toBe('build');
		expect(parsed.positional).toEqual(['valley.massifgraph']);
	});

	test('reads an option written either way', () => {
		const spaced = parseArguments(['build', '--resolution', '8192']);
		const joined = parseArguments(['build', '--resolution=8192']);
		expect(optionalNumber(spaced, 'resolution')).toBe(8192);
		expect(optionalNumber(joined, 'resolution')).toBe(8192);
	});

	test('an option followed by another option is a flag, not a value', () => {
		const parsed = parseArguments(['apply', '--force', '--from', 'plan.dsl']);
		expect(hasFlag(parsed, 'force')).toBe(true);
		expect(optionalText(parsed, 'from')).toBe('plan.dsl');
	});

	test('options do not disturb the order of the positionals around them', () => {
		const parsed = parseArguments(['set', '--seed', '7', 'valley.massifgraph']);
		expect(parsed.positional).toEqual(['valley.massifgraph']);
	});

	test('a missing option is absent rather than zero', () => {
		const parsed = parseArguments(['build', 'valley.massifgraph']);
		expect(optionalNumber(parsed, 'resolution')).toBeNull();
		expect(optionalText(parsed, 'node')).toBeNull();
	});

	test('a negative number is a value, not an option', () => {
		const parsed = parseArguments(['set', 'valley.massifgraph', '--seed', '-3']);
		expect(optionalNumber(parsed, 'seed')).toBe(-3);
	});
});

describe('reading arguments', () => {
	test('refuses an option that was given no value', () => {
		const parsed = parseArguments(['build', '--resolution']);
		expect(() => optionalText(parsed, 'resolution')).toThrow('--resolution needs a value');
	});

	test('refuses a number that is not one', () => {
		const parsed = parseArguments(['build', '--resolution', 'huge']);
		expect(() => optionalNumber(parsed, 'resolution')).toThrow(UsageError);
	});

	test('refuses a positional that is not there', () => {
		const parsed = parseArguments(['print']);
		expect(() => positionalAt(parsed, 0, 'a graph file')).toThrow('Expected a graph file');
	});
});
