/**
 * Which of the node editor's small previews are still worth looking at.
 *
 * An evaluation only visits the nodes feeding the previewed one, so most of the
 * graph hears nothing back. Its pictures are kept rather than thrown away, and
 * each is held against the signature the engine reports for that node: while
 * the signature stands, the picture is what the node would draw now. When it
 * changes, the picture is out of date and says so.
 */

import type { NodeSignature, NodeThumbnail } from '$shared/engine-protocol';

export interface CachedThumbnail {
	image: NodeThumbnail;
	signature: string;
}

export function signaturesById(signatures: NodeSignature[]): Record<string, string> {
	const byId: Record<string, string> = {};
	for (const signature of signatures) {
		byId[signature.nodeId] = signature.key;
	}
	return byId;
}

/// The thumbnails an evaluation leaves behind: the ones it drew, plus those
/// already held for nodes it did not visit. A node that has left the graph —
/// and so has no signature — loses its picture.
export function retainedThumbnails(
	held: Record<string, CachedThumbnail>,
	arrived: NodeThumbnail[],
	signatures: Record<string, string>
): Record<string, CachedThumbnail> {
	const kept: Record<string, CachedThumbnail> = {};
	for (const [nodeId, cached] of Object.entries(held)) {
		if (nodeId in signatures) {
			kept[nodeId] = cached;
		}
	}
	for (const image of arrived) {
		kept[image.nodeId] = { image, signature: signatureOf(signatures, image.nodeId) };
	}
	return kept;
}

function signatureOf(signatures: Record<string, string>, nodeId: string): string {
	const found = signatures[nodeId];
	if (found === undefined) {
		return '';
	}
	return found;
}

export function isThumbnailCurrent(
	cached: CachedThumbnail | undefined,
	signature: string | undefined
): boolean {
	if (!cached || signature === undefined) {
		return false;
	}
	return cached.signature === signature;
}
