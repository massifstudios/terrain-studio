import { LinearFilter, LinearMipmapLinearFilter, RepeatWrapping, Texture } from 'three';

/**
 * The image tiles a material preview repeats over the terrain. Each is read
 * through the asset bridge once and kept for the life of the window, so
 * switching previews never re-decodes the same rock.
 */

const loaded = new Map<string, Promise<Texture | null>>();

async function decode(name: string): Promise<Texture | null> {
	const bytes = await window.assetBridge.readTexture(name);
	if (bytes === null) {
		return null;
	}
	const bitmap = await createImageBitmap(new Blob([bytes.slice().buffer as ArrayBuffer]));
	const texture = new Texture(bitmap);
	texture.wrapS = RepeatWrapping;
	texture.wrapT = RepeatWrapping;
	texture.minFilter = LinearMipmapLinearFilter;
	texture.magFilter = LinearFilter;
	texture.anisotropy = 8;
	texture.needsUpdate = true;
	return texture;
}

/// Null when the file is missing or not an image; the caller shows a blank
/// tile there rather than nothing at all.
export function surfaceTexture(name: string): Promise<Texture | null> {
	const pending = loaded.get(name);
	if (pending) {
		return pending;
	}
	const created = decode(name).catch(() => null);
	loaded.set(name, created);
	return created;
}

export function surfaceTextures(names: string[]): Promise<(Texture | null)[]> {
	return Promise.all(names.map(surfaceTexture));
}
