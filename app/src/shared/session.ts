/**
 * What the program remembers about itself between launches: the graph as a
 * plain `.massifgraph`, and beside it which project file that graph came from.
 *
 * The pointer is a separate file rather than a field in the graph, because a
 * path on this machine is not part of a project and must not travel with one.
 */

export const sessionFormatVersion = 1;

/** Which project file the remembered graph belongs to, and whether it matched. */
export interface SessionPointer {
	filePath: string | null;
	hasUnsavedChanges: boolean;
}

/** The two files as the main process read them, still unparsed. */
export interface StoredSession {
	graph: string;
	pointer: string;
}

export function serializeSessionPointer(pointer: SessionPointer): string {
	return `${JSON.stringify(
		{
			formatVersion: sessionFormatVersion,
			filePath: pointer.filePath,
			hasUnsavedChanges: pointer.hasUnsavedChanges
		},
		null,
		'\t'
	)}\n`;
}

export function parseSessionPointer(text: string): SessionPointer {
	const parsed = JSON.parse(text) as Record<string, unknown>;
	if (parsed.formatVersion !== sessionFormatVersion) {
		throw new Error(`Unsupported session format version ${String(parsed.formatVersion)}`);
	}

	let filePath: string | null = null;
	if (typeof parsed.filePath === 'string') {
		filePath = parsed.filePath;
	}

	return { filePath, hasUnsavedChanges: parsed.hasUnsavedChanges === true };
}
