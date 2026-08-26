/**
 * The command line as data. Parsing is kept apart from the commands so it can
 * be tested without an engine behind it.
 *
 * Only long options: `--resolution 8192` and `--resolution=8192` mean the same
 * thing, and an option with nothing after it is a flag that was present.
 */

export class UsageError extends Error {}

export interface ParsedArguments {
	command: string;
	positional: string[];
	options: Record<string, string | true>;
}

interface ReadOption {
	name: string;
	value: string | true;
	consumed: number;
}

function isOption(token: string): boolean {
	return token.startsWith('--');
}

function readOption(token: string, next: string | undefined): ReadOption {
	const body = token.slice(2);
	const equals = body.indexOf('=');
	if (equals >= 0) {
		return { name: body.slice(0, equals), value: body.slice(equals + 1), consumed: 1 };
	}
	if (next === undefined || isOption(next)) {
		return { name: body, value: true, consumed: 1 };
	}
	return { name: body, value: next, consumed: 2 };
}

export function parseArguments(argv: string[]): ParsedArguments {
	const positional: string[] = [];
	const options: Record<string, string | true> = {};
	let index = 0;
	while (index < argv.length) {
		const token = argv[index];
		if (!isOption(token)) {
			positional.push(token);
			index += 1;
			continue;
		}
		const option = readOption(token, argv[index + 1]);
		options[option.name] = option.value;
		index += option.consumed;
	}

	let command = '';
	if (positional.length > 0) {
		command = positional[0];
	}
	return { command, positional: positional.slice(1), options };
}

export function hasFlag(parsed: ParsedArguments, name: string): boolean {
	return parsed.options[name] !== undefined;
}

export function optionalText(parsed: ParsedArguments, name: string): string | null {
	const value = parsed.options[name];
	if (value === undefined) {
		return null;
	}
	if (value === true) {
		throw new UsageError(`--${name} needs a value`);
	}
	return value;
}

export function optionalNumber(parsed: ParsedArguments, name: string): number | null {
	const text = optionalText(parsed, name);
	if (text === null) {
		return null;
	}
	const value = Number(text);
	if (!Number.isFinite(value)) {
		throw new UsageError(`--${name} needs a number, not '${text}'`);
	}
	return value;
}

export function positionalAt(parsed: ParsedArguments, index: number, what: string): string {
	const value = parsed.positional[index];
	if (value === undefined) {
		throw new UsageError(`Expected ${what}`);
	}
	return value;
}
