import { graphStore } from '$lib/graph/graph-store.svelte';
import { sourceModel } from '$lib/graph/source-model.svelte';
import type { GraphProject } from '$shared/massifgraph';
import { parseGraph, serializeGraph } from '$shared/massifgraph';
import {
	parseSessionPointer,
	serializeSessionPointer,
	type SessionPointer,
	type StoredSession
} from '$shared/session';

const debounceMilliseconds = 500;

interface RestoredSession {
	project: GraphProject;
	pointer: SessionPointer;
}

/// A session written by an older build, or left half-written by a crash, is not
/// worth failing a launch over: the app opens empty instead.
function parseStoredSession(stored: StoredSession): RestoredSession | null {
	try {
		return { project: parseGraph(stored.graph), pointer: parseSessionPointer(stored.pointer) };
	} catch {
		return null;
	}
}

/**
 * The graph outlives the window. Every edit is written to the program's own
 * configuration directory and the next launch picks it up exactly where it was
 * left, unsaved changes and all.
 *
 * This is not saving. A project file is somewhere the user chose and only
 * `Save` writes there; this is only so that closing the window never costs
 * work.
 */
class SessionStore {
	#pending: ReturnType<typeof setTimeout> | null = null;
	#isRestored = false;

	async restore(): Promise<void> {
		const stored = await window.sessionBridge.read();
		this.#isRestored = true;
		if (stored === null) {
			return;
		}
		const session = parseStoredSession(stored);
		if (session === null) {
			return;
		}
		sourceModel.rememberLoadedProject(session.project);
		graphStore.load(session.project, session.pointer.filePath);
		// `load` calls a graph freshly read from its file clean, which a restored
		// one is not: the edits it carries were never written to that file.
		graphStore.hasUnsavedChanges = session.pointer.hasUnsavedChanges;
	}

	/// Debounced, because a slider drag would otherwise write the file on every
	/// frame. Ignored until the stored session has been read back, so an empty
	/// first frame cannot overwrite what is waiting on disk.
	remember(): void {
		if (!this.#isRestored) {
			return;
		}
		if (this.#pending !== null) {
			clearTimeout(this.#pending);
		}
		this.#pending = setTimeout(() => this.flush(), debounceMilliseconds);
	}

	flush(): void {
		if (this.#pending !== null) {
			clearTimeout(this.#pending);
			this.#pending = null;
		}
		if (!this.#isRestored) {
			return;
		}
		const pointer: SessionPointer = {
			filePath: graphStore.filePath,
			hasUnsavedChanges: graphStore.hasUnsavedChanges
		};
		window.sessionBridge.write(
			serializeGraph(graphStore.project()),
			serializeSessionPointer(pointer)
		);
	}
}

export const sessionStore = new SessionStore();
