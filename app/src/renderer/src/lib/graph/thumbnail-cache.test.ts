import { describe, expect, test } from 'bun:test';
import type { NodeThumbnail } from '$shared/engine-protocol';
import {
	isThumbnailCurrent,
	retainedThumbnails,
	signaturesById,
	type CachedThumbnail
} from './thumbnail-cache';

function image(nodeId: string): NodeThumbnail {
	return { nodeId, size: 2, rgba: new Uint8Array(16) };
}

function held(nodeId: string, signature: string): CachedThumbnail {
	return { image: image(nodeId), signature };
}

describe('thumbnails across evaluations', () => {
	test('keeps the picture of a node the evaluation never visited', () => {
		const before = { aside: held('aside', 'a1') };
		const after = retainedThumbnails(before, [image('sum')], { aside: 'a1', sum: 's1' });

		expect(after.aside).toBe(before.aside);
		expect(after.sum.signature).toBe('s1');
	});

	test('forgets a node that has left the graph', () => {
		const after = retainedThumbnails({ gone: held('gone', 'g1') }, [], { sum: 's1' });
		expect(after.gone).toBeUndefined();
	});

	test('replaces a picture the evaluation redrew', () => {
		const before = { sum: held('sum', 's1') };
		const after = retainedThumbnails(before, [image('sum')], { sum: 's2' });
		expect(after.sum.signature).toBe('s2');
	});
});

describe('whether a held picture still stands', () => {
	test('stands while the signature is unchanged', () => {
		expect(isThumbnailCurrent(held('sum', 's1'), 's1')).toBe(true);
	});

	test('falls once the node has been changed underneath it', () => {
		expect(isThumbnailCurrent(held('sum', 's1'), 's2')).toBe(false);
	});

	test('falls when there is no picture, or no signature to check it against', () => {
		expect(isThumbnailCurrent(undefined, 's1')).toBe(false);
		expect(isThumbnailCurrent(held('sum', 's1'), undefined)).toBe(false);
	});
});

test('signatures are indexed by node', () => {
	expect(
		signaturesById([
			{ nodeId: 'a', key: '1' },
			{ nodeId: 'b', key: '2' }
		])
	).toEqual({ a: '1', b: '2' });
});
