/**
 * Small pictures of the textures a texture parameter can name, for picking
 * one by eye. Each is read through the asset bridge once, shrunk, and kept as
 * an object URL for the life of the window: the panel shows the same handful
 * of tiles over and over, and a full-size decode for each would dwarf them.
 */

const thumbnailSize = 128;

const urls = new Map<string, Promise<string | null>>();

function mimeTypeFor(name: string): string {
	if (name.toLowerCase().endsWith('.png')) {
		return 'image/png';
	}
	return 'image/jpeg';
}

async function shrink(name: string): Promise<string | null> {
	const bytes = await window.assetBridge.readTexture(name);
	if (bytes === null) {
		return null;
	}
	const source = new Blob([bytes.slice().buffer as ArrayBuffer], { type: mimeTypeFor(name) });
	const bitmap = await createImageBitmap(source, {
		resizeWidth: thumbnailSize,
		resizeHeight: thumbnailSize,
		resizeQuality: 'medium'
	});
	const canvas = new OffscreenCanvas(thumbnailSize, thumbnailSize);
	canvas.getContext('2d')?.drawImage(bitmap, 0, 0);
	bitmap.close();
	return URL.createObjectURL(await canvas.convertToBlob({ type: 'image/jpeg', quality: 0.85 }));
}

/// Null when the file is missing or not an image.
export function textureThumbnailUrl(name: string): Promise<string | null> {
	const pending = urls.get(name);
	if (pending) {
		return pending;
	}
	const created = shrink(name).catch(() => null);
	urls.set(name, created);
	return created;
}
