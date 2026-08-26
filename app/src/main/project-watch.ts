/**
 * Watching the open project file, so a graph rewritten from outside — by the
 * `massif` command, or by anything else — reaches the editor instead of being
 * quietly overwritten by the next save.
 *
 * The containing directory is watched rather than the file itself: a writer
 * that replaces the file instead of truncating it would take a watch on the
 * file down with it, and the editor would then hear nothing ever again.
 */

import { readFile } from 'node:fs/promises';
import { watch, type FSWatcher } from 'node:fs';
import { basename, dirname } from 'node:path';
import type { WebContents } from 'electron';

// A single save arrives as several events. This is long enough to let a write
// finish, so the file is never read back half-written.
const settleMilliseconds = 150;

let watcher: FSWatcher | null = null;
let watchedPath: string | null = null;
let pending: ReturnType<typeof setTimeout> | null = null;

function stopWatching(): void {
	if (pending !== null) {
		clearTimeout(pending);
		pending = null;
	}
	watcher?.close();
	watcher = null;
	watchedPath = null;
}

async function report(sender: WebContents): Promise<void> {
	pending = null;
	const path = watchedPath;
	if (path === null || sender.isDestroyed()) {
		return;
	}
	try {
		sender.send('project:changed', path, await readFile(path, 'utf8'));
	} catch {
		// A file that has just been deleted or is mid-write is not news; the next
		// event brings the version that matters.
	}
}

function scheduleReport(sender: WebContents): void {
	if (pending !== null) {
		clearTimeout(pending);
	}
	pending = setTimeout(() => report(sender), settleMilliseconds);
}

/// Also reports once straight away, which is what catches a file that changed
/// while the app was closed.
export function watchProject(sender: WebContents, path: string | null): void {
	stopWatching();
	if (path === null) {
		return;
	}
	watchedPath = path;
	watcher = openWatch(path, sender);
	scheduleReport(sender);
}

function openWatch(path: string, sender: WebContents): FSWatcher | null {
	try {
		return watch(dirname(path), (_event, changed) => {
			if (changed === basename(path)) {
				scheduleReport(sender);
			}
		});
	} catch {
		// A directory that cannot be watched costs the editor a notification and
		// nothing else; saving and opening still work.
		return null;
	}
}
