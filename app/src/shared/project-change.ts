/**
 * What to do when the open project file changes underneath the editor.
 *
 * Kept apart from the watcher so the three answers can be pinned down without a
 * file system or a component tree behind them.
 */

export type ProjectChangeAnswer = 'ignore' | 'adopt' | 'ask';

export interface ProjectChange {
	/// The file the editor has open, null when the project was never saved.
	openPath: string | null;
	/// What that file held the last time the editor read or wrote it.
	lastKnownText: string | null;
	hasUnsavedChanges: boolean;
	changedPath: string;
	changedText: string;
}

/**
 * `ignore` for a change to a file we are not in, and for our own save arriving
 * back. `adopt` when there is nothing to lose. `ask` when there is: reloading
 * and overwriting are both defensible, and picking one on the user's behalf
 * would throw away work either way.
 */
export function answerProjectChange(change: ProjectChange): ProjectChangeAnswer {
	if (change.changedPath !== change.openPath) {
		return 'ignore';
	}
	if (change.changedText === change.lastKnownText) {
		return 'ignore';
	}
	if (change.hasUnsavedChanges) {
		return 'ask';
	}
	return 'adopt';
}
