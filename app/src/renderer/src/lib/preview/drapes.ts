import {
	BufferAttribute,
	BufferGeometry,
	DataTexture,
	FloatType,
	LinearFilter,
	NearestFilter,
	Points,
	PointsMaterial,
	RGBAFormat,
	RedFormat,
	type ShaderMaterial,
	type Texture,
	UnsignedByteType
} from 'three';
import type { ColorResult, FieldResult, PointsResult } from '$shared/engine-protocol';
import { valuesPerPoint } from '$shared/engine-protocol';
import { materialLayerLimit } from '$shared/material-preview';
import { spanMetersOf } from './terrain-mesh';

/**
 * The things a preview can lay over the terrain: a mask, a colour map, and a
 * set of scattered instances. Both viewports build them the same way, so the
 * 3D and the 2D view cannot drift apart.
 *
 * All of this is presentation. The samples come from the engine and are handed
 * to the GPU unchanged; nothing here works out a height or a weight.
 */

function sharpTexture(texture: DataTexture): DataTexture {
	texture.minFilter = NearestFilter;
	texture.magFilter = NearestFilter;
	texture.needsUpdate = true;
	return texture;
}

export function maskTexture(mask: FieldResult): DataTexture {
	return sharpTexture(
		new DataTexture(mask.samples, mask.width, mask.height, RedFormat, FloatType)
	);
}

/// The engine sends three floats per cell; WebGL wants four. The fourth is
/// added here rather than in the engine, where it would be a channel of ones
/// travelling across the bridge for nothing.
export function colorTexture(colors: ColorResult): DataTexture {
	const cells = colors.width * colors.height;
	const rgba = new Float32Array(cells * 4);
	for (let cell = 0; cell < cells; cell += 1) {
		rgba[cell * 4] = colors.samples[cell * 3];
		rgba[cell * 4 + 1] = colors.samples[cell * 3 + 1];
		rgba[cell * 4 + 2] = colors.samples[cell * 3 + 2];
		rgba[cell * 4 + 3] = 1;
	}
	return sharpTexture(new DataTexture(rgba, colors.width, colors.height, RGBAFormat, FloatType));
}

interface DrapeUniforms {
	overlay: { value: Texture | null };
	overlayStrength: { value: number };
	painted: { value: Texture | null };
	paintedStrength: { value: number };
}

/// Swaps a drape texture in, disposing whatever was there. Returns the new
/// texture so the caller can dispose it in turn.
export function applyDrape(
	material: ShaderMaterial,
	previous: DataTexture | null,
	next: DataTexture | null,
	slot: 'overlay' | 'painted'
): DataTexture | null {
	previous?.dispose();
	const uniforms = material.uniforms as unknown as DrapeUniforms;
	uniforms[slot].value = next;
	setStrength(uniforms, slot, next);
	return next;
}

function setStrength(
	uniforms: DrapeUniforms,
	slot: 'overlay' | 'painted',
	texture: DataTexture | null
): void {
	let strength = 1;
	if (texture === null) {
		strength = 0;
	}
	if (slot === 'overlay') {
		uniforms.overlayStrength.value = strength;
		return;
	}
	uniforms.paintedStrength.value = strength;
}

/* --------------------------------------------------------------- materials -- */

interface MaterialUniforms {
	materialWeights: { value: Texture | null };
	materialStrength: { value: number };
	materialTiles: { value: Texture[] };
	materialRepeat: { value: number };
}

/// Stands in for a tile that has not loaded or does not exist, so every
/// sampler is always bound to something.
const blankTile = new DataTexture(new Uint8Array([128, 128, 128, 255]), 1, 1, RGBAFormat);
blankTile.needsUpdate = true;

export function materialUniforms(): MaterialUniforms {
	return {
		materialWeights: { value: null },
		materialStrength: { value: 0 },
		materialTiles: { value: Array.from({ length: materialLayerLimit }, () => blankTile) },
		materialRepeat: { value: 1 }
	};
}

/// One layer per channel, so the shader reads every weight in one fetch. A
/// layer the node did not fill weighs nothing. Filtered smoothly, unlike a
/// mask drape: a blend boundary should not show the grid. Eight bits per
/// weight, because a float texture only filters where the GPU offers the
/// extension for it and silently snaps to the nearest cell elsewhere.
export function materialWeightTexture(weights: FieldResult[]): DataTexture {
	const first = weights[0];
	const cells = first.width * first.height;
	const rgba = new Uint8Array(cells * 4);
	const layerCount = Math.min(weights.length, materialLayerLimit);
	for (let layer = 0; layer < layerCount; layer += 1) {
		const samples = weights[layer].samples;
		for (let cell = 0; cell < cells; cell += 1) {
			rgba[cell * 4 + layer] = Math.round(Math.min(Math.max(samples[cell], 0), 1) * 255);
		}
	}
	const texture = new DataTexture(rgba, first.width, first.height, RGBAFormat, UnsignedByteType);
	texture.minFilter = LinearFilter;
	texture.magFilter = LinearFilter;
	texture.needsUpdate = true;
	return texture;
}

export function applyMaterialWeights(
	material: ShaderMaterial,
	previous: DataTexture | null,
	next: DataTexture | null
): DataTexture | null {
	previous?.dispose();
	const uniforms = material.uniforms as unknown as MaterialUniforms;
	uniforms.materialWeights.value = next;
	uniforms.materialStrength.value = 1;
	if (next === null) {
		uniforms.materialStrength.value = 0;
	}
	return next;
}

export function applyMaterialTiles(material: ShaderMaterial, tiles: (Texture | null)[]): void {
	const uniforms = material.uniforms as unknown as MaterialUniforms;
	const bound: Texture[] = [];
	for (let layer = 0; layer < materialLayerLimit; layer += 1) {
		const tile = tiles[layer];
		if (tile) {
			bound.push(tile);
			continue;
		}
		bound.push(blankTile);
	}
	uniforms.materialTiles.value = bound;
}

/// How many times a tile repeats across the grid, from the ground the grid
/// covers and the ground one tile is meant to cover.
export function applyMaterialRepeat(
	material: ShaderMaterial,
	worldSizeMeters: number,
	tileSizeMeters: number
): void {
	const uniforms = material.uniforms as unknown as MaterialUniforms;
	uniforms.materialRepeat.value = worldSizeMeters / Math.max(tileSizeMeters, 0.01);
}

/**
 * Scattered instances as a point cloud. Positions come straight out of the
 * engine's flat array: easting east, altitude up, northing measured from the
 * south edge, which is -Z once the terrain is centred on the origin.
 */
export function buildPointMarkers(
	points: PointsResult,
	field: FieldResult,
	sizeMeters: number,
	color: string
): Points {
	const span = spanMetersOf(field);
	const positions = new Float32Array(points.count * 3);

	for (let index = 0; index < points.count; index += 1) {
		const source = index * valuesPerPoint;
		positions[index * 3] = points.values[source] - span / 2;
		positions[index * 3 + 1] = points.values[source + 2];
		positions[index * 3 + 2] = span / 2 - points.values[source + 1];
	}

	const geometry = new BufferGeometry();
	geometry.setAttribute('position', new BufferAttribute(positions, 3));
	geometry.computeBoundingSphere();

	return new Points(
		geometry,
		new PointsMaterial({ color, size: sizeMeters, sizeAttenuation: true })
	);
}
