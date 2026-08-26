import { describe, expect, test } from 'bun:test';
import { parseSessionPointer, serializeSessionPointer } from './session';

describe('session pointer', () => {
	test('round-trips a session that has a project file', () => {
		const pointer = { filePath: '/terrain/valley.massifgraph', hasUnsavedChanges: true };
		expect(parseSessionPointer(serializeSessionPointer(pointer))).toEqual(pointer);
	});

	test('round-trips a session that has never been saved', () => {
		const pointer = { filePath: null, hasUnsavedChanges: false };
		expect(parseSessionPointer(serializeSessionPointer(pointer))).toEqual(pointer);
	});

	test('refuses a format version it does not know', () => {
		const text = JSON.stringify({ formatVersion: 99, filePath: null, hasUnsavedChanges: false });
		expect(() => parseSessionPointer(text)).toThrow('Unsupported session format version 99');
	});

	test('treats a missing path as never saved rather than as a path', () => {
		const text = JSON.stringify({ formatVersion: 1 });
		expect(parseSessionPointer(text)).toEqual({ filePath: null, hasUnsavedChanges: false });
	});
});
