/**
 * Where the session is kept between launches. The main process owns the
 * location for the same reason it owns the plugin directories: only it knows
 * where a platform keeps a program's configuration, and the renderer has no
 * say in it at all.
 */

import { join } from 'node:path';
import { mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import { app } from 'electron';
import type { StoredSession } from '../shared/session';

// `userData` already resolves to `$XDG_CONFIG_HOME/terrain-studio` on Linux,
// and to the platform's own equivalent elsewhere.
const graphFileName = 'session.massifgraph';
const pointerFileName = 'session.json';

function readTextOrNull(path: string): string | null {
	try {
		return readFileSync(path, 'utf8');
	} catch {
		return null;
	}
}

/// Null when there is nothing to restore, which is every first launch. A
/// half-written pair counts as nothing rather than as half a project.
export function readSession(): StoredSession | null {
	const directory = app.getPath('userData');
	const graph = readTextOrNull(join(directory, graphFileName));
	const pointer = readTextOrNull(join(directory, pointerFileName));
	if (graph === null || pointer === null) {
		return null;
	}
	return { graph, pointer };
}

export function writeSession(session: StoredSession): void {
	const directory = app.getPath('userData');
	mkdirSync(directory, { recursive: true });
	writeFileSync(join(directory, graphFileName), session.graph, 'utf8');
	writeFileSync(join(directory, pointerFileName), session.pointer, 'utf8');
}
