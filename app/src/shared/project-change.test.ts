import { describe, expect, test } from 'bun:test';
import { answerProjectChange, type ProjectChange } from './project-change';

function change(overrides: Partial<ProjectChange>): ProjectChange {
	return {
		openPath: '/terrain/valley.massifgraph',
		lastKnownText: 'before',
		hasUnsavedChanges: false,
		changedPath: '/terrain/valley.massifgraph',
		changedText: 'after',
		...overrides
	};
}

describe('answerProjectChange', () => {
	test('takes a change nobody would lose work over', () => {
		expect(answerProjectChange(change({}))).toBe('adopt');
	});

	test('asks when the editor has edits the file does not', () => {
		expect(answerProjectChange(change({ hasUnsavedChanges: true }))).toBe('ask');
	});

	test('ignores our own save coming back', () => {
		expect(answerProjectChange(change({ changedText: 'before' }))).toBe('ignore');
	});

	test('ignores a change to a file the editor does not have open', () => {
		expect(answerProjectChange(change({ changedPath: '/terrain/other.massifgraph' }))).toBe(
			'ignore'
		);
	});

	test('ignores anything at all while the project has never been saved', () => {
		expect(answerProjectChange(change({ openPath: null }))).toBe('ignore');
	});

	test('a first sight of the file is a change, not a match', () => {
		expect(answerProjectChange(change({ lastKnownText: null }))).toBe('adopt');
	});

	test('unsaved edits win over a first sight of the file', () => {
		expect(answerProjectChange(change({ lastKnownText: null, hasUnsavedChanges: true }))).toBe(
			'ask'
		);
	});
});
