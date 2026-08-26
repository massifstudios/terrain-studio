import { graphStore } from '$lib/graph/graph-store.svelte';
import {
	adoptProjectText,
	lastKnownProjectText,
	saveProjectAs
} from '$lib/project-file';
import { answerProjectChange } from '$shared/project-change';

export interface ProjectConflict {
	path: string;
	text: string;
}

/**
 * The open project file, rewritten from outside — by the `massif` command, or
 * by anything else that edits a `.massifgraph`.
 *
 * With nothing unsaved in the editor there is no argument to have and the new
 * contents are simply taken. With unsaved edits there are two answers and only
 * the person at the keyboard can give one, so the graph is left exactly as it
 * is until they do.
 */
class ProjectWatcher {
	conflict = $state<ProjectConflict | null>(null);

	connect(): void {
		window.projectBridge.onChanged((path, text) => this.#handleChange(path, text));
	}

	follow(path: string | null): void {
		window.projectBridge.watch(path);
	}

	/// Takes what is on disk and forgets the edits in the editor.
	reload(): void {
		const conflict = this.conflict;
		if (conflict === null) {
			return;
		}
		this.conflict = null;
		this.#adopt(conflict.path, conflict.text);
	}

	/// Writes the editor's graph over whatever arrived.
	async overwrite(): Promise<void> {
		const conflict = this.conflict;
		if (conflict === null) {
			return;
		}
		this.conflict = null;
		await saveProjectAs(conflict.path);
	}

	#handleChange(path: string, text: string): void {
		const answer = answerProjectChange({
			openPath: graphStore.filePath,
			lastKnownText: lastKnownProjectText(),
			hasUnsavedChanges: graphStore.hasUnsavedChanges,
			changedPath: path,
			changedText: text
		});
		if (answer === 'ignore') {
			return;
		}
		if (answer === 'ask') {
			this.conflict = { path, text };
			return;
		}
		this.#adopt(path, text);
	}

	#adopt(path: string, text: string): void {
		try {
			adoptProjectText(path, text);
		} catch {
			// A file caught mid-write parses as nothing at all. The watcher settles
			// before reading, so this is rare, and the next event brings a whole
			// file to replace it.
		}
	}
}

export const projectWatcher = new ProjectWatcher();
