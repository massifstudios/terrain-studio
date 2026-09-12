import { Vector3 } from 'three';

/**
 * The shading both viewports use, matching the hillshade the engine writes
 * beside an exported DEM: a sun at 315°/45° over an altitude ramp. Keeping the
 * two identical means the preview and the file agree.
 *
 * This is presentation, not terrain math — the shaders read the grid the
 * engine produced and never invent a height value.
 */

/// World axes: +X east, +Y up, -Z north.
export function sunDirection(azimuthDegrees: number, altitudeDegrees: number): Vector3 {
	const azimuth = (azimuthDegrees * Math.PI) / 180;
	const altitude = (altitudeDegrees * Math.PI) / 180;
	return new Vector3(
		Math.sin(azimuth) * Math.cos(altitude),
		Math.sin(altitude),
		-Math.cos(azimuth) * Math.cos(altitude)
	);
}

/// Enough fill that a slope facing away from the sun still reads as terrain.
export const ambientShade = 0.18;

/**
 * Flat grey steps, matching `engine/src/thumbnail.cpp`. A mask has no hue of
 * its own, and banding the lightness makes a threshold show its own edge.
 * Change one and change the other.
 */
const maskStepsGlsl = /* glsl */ `
const float maskStepCount = 8.0;
const float darkestMaskStep = 0.08;
const float lightestMaskStep = 0.94;

vec3 maskSteps(float value) {
	float index = min(floor(clamp(value, 0.0, 1.0) * maskStepCount), maskStepCount - 1.0);
	float band = index / (maskStepCount - 1.0);
	return vec3(darkestMaskStep + (lightestMaskStep - darkestMaskStep) * band);
}

vec3 drapeMask(vec3 shaded, sampler2D overlay, vec2 gridUv, float strength) {
	if (strength <= 0.0) {
		return shaded;
	}
	float value = texture2D(overlay, gridUv).r;
	// Keep some of the hillshade so the relief still reads under the mask.
	return mix(shaded, maskSteps(value) * (0.55 + 0.45 * dot(shaded, vec3(0.333))), strength);
}
`;

const altitudeRampGlsl = /* glsl */ `
vec3 altitudeRamp(float band) {
	return vec3(0.35 + 0.65 * band, 0.40 + 0.55 * band, 0.35 + 0.45 * band);
}

float reliefBand(float altitude, float lowest, float highest) {
	return clamp((altitude - lowest) / max(highest - lowest, 1.0), 0.0, 1.0);
}

// A colour map replaces the altitude ramp rather than tinting the lit result,
// so the terrain keeps its own light and the map keeps its own colour.
vec3 surfaceColor(vec3 rampColor, sampler2D painted, vec2 gridUv, float strength) {
	if (strength <= 0.0) {
		return rampColor;
	}
	return mix(rampColor, texture2D(painted, gridUv).rgb, strength);
}
`;

/**
 * Tiled textures blended by weight. The weights are the engine's splat
 * layers packed one per channel; they add to one, so the blend is a plain
 * weighted sum. Sampler arrays may only be indexed by constants, hence the
 * spelled-out four.
 *
 * A texture repeated as-is reads as a carpet. Every tile instead places the
 * texture its own way, turned by a random angle and slid by a random offset,
 * and neighbouring tiles cross-fade over the middle half of each so neither
 * the seam nor the repeat shows. Gradients are passed explicitly because the
 * per-tile transform would otherwise break mip selection at the tile edges.
 */
const materialGlsl = /* glsl */ `
uniform sampler2D materialWeights;
uniform float materialStrength;
uniform sampler2D materialTiles[4];
uniform float materialRepeat;

vec4 tileHash(vec2 tile) {
	return fract(sin(vec4(
		1.0 + dot(tile, vec2(37.0, 17.0)),
		2.0 + dot(tile, vec2(11.0, 47.0)),
		3.0 + dot(tile, vec2(41.0, 29.0)),
		4.0 + dot(tile, vec2(23.0, 31.0)))) * 103.0);
}

vec3 sampleTile(sampler2D tile, vec2 uv, vec2 cell, vec2 ddx, vec2 ddy) {
	vec4 random = tileHash(cell);
	float angle = random.z * 6.2831853;
	mat2 turn = mat2(cos(angle), sin(angle), -sin(angle), cos(angle));
	return textureGrad(tile, turn * uv + random.xy, turn * ddx, turn * ddy).rgb;
}

vec3 untiledSample(sampler2D tile, vec2 uv) {
	vec2 cell = floor(uv);
	vec2 blend = smoothstep(0.25, 0.75, fract(uv));
	vec2 ddx = dFdx(uv);
	vec2 ddy = dFdy(uv);
	vec3 here = sampleTile(tile, uv, cell, ddx, ddy);
	vec3 east = sampleTile(tile, uv, cell + vec2(1.0, 0.0), ddx, ddy);
	vec3 south = sampleTile(tile, uv, cell + vec2(0.0, 1.0), ddx, ddy);
	vec3 southEast = sampleTile(tile, uv, cell + vec2(1.0, 1.0), ddx, ddy);
	return mix(mix(here, east, blend.x), mix(south, southEast, blend.x), blend.y);
}

vec3 drapeMaterials(vec3 colour, vec2 gridUv) {
	if (materialStrength <= 0.0) {
		return colour;
	}
	vec4 weights = texture2D(materialWeights, gridUv);
	vec2 tileUv = gridUv * materialRepeat;
	vec3 blended = weights.r * untiledSample(materialTiles[0], tileUv)
		+ weights.g * untiledSample(materialTiles[1], tileUv)
		+ weights.b * untiledSample(materialTiles[2], tileUv)
		+ weights.a * untiledSample(materialTiles[3], tileUv);
	return mix(colour, blended, materialStrength);
}
`;

export const surfaceVertexShader = /* glsl */ `
varying vec3 worldNormal;
varying float altitudeMeters;
varying vec2 gridUv;

void main() {
	worldNormal = normalize(mat3(modelMatrix) * normal);
	altitudeMeters = position.y;
	gridUv = vec2(uv.x, 1.0 - uv.y);
	gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);
}
`;

export const surfaceFragmentShader = /* glsl */ `
uniform vec3 sunDirection;
uniform float lowestAltitude;
uniform float highestAltitude;
uniform float ambient;
uniform sampler2D overlay;
uniform float overlayStrength;
uniform sampler2D painted;
uniform float paintedStrength;

varying vec3 worldNormal;
varying float altitudeMeters;
varying vec2 gridUv;

${altitudeRampGlsl}
${maskStepsGlsl}
${materialGlsl}

void main() {
	float shade = clamp(dot(normalize(worldNormal), sunDirection), 0.0, 1.0);
	vec3 ramp = altitudeRamp(reliefBand(altitudeMeters, lowestAltitude, highestAltitude));
	vec3 colour = drapeMaterials(surfaceColor(ramp, painted, gridUv, paintedStrength), gridUv);
	vec3 shaded = colour * (ambient + (1.0 - ambient) * shade);
	gl_FragColor = vec4(drapeMask(shaded, overlay, gridUv, overlayStrength), 1.0);
}
`;

export const topDownVertexShader = /* glsl */ `
varying vec2 gridUv;

void main() {
	// The quad's v grows upward, the grid's row index grows southward.
	gridUv = vec2(uv.x, 1.0 - uv.y);
	gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);
}
`;

/**
 * Hillshade straight from the height texture. The gradients are central
 * differences over the same cell spacing the engine uses, so the picture
 * matches the exported preview.
 */
export const topDownFragmentShader = /* glsl */ `
uniform sampler2D heights;
uniform vec2 texelSize;
uniform float cellSizeMeters;
uniform vec3 sunDirection;
uniform float lowestAltitude;
uniform float highestAltitude;
uniform float ambient;
uniform sampler2D overlay;
uniform float overlayStrength;
uniform sampler2D painted;
uniform float paintedStrength;

varying vec2 gridUv;

${altitudeRampGlsl}
${maskStepsGlsl}
${materialGlsl}

float altitudeAt(vec2 uv) {
	return texture2D(heights, clamp(uv, vec2(0.0), vec2(1.0))).r;
}

void main() {
	float here = altitudeAt(gridUv);
	float east = (altitudeAt(gridUv + vec2(texelSize.x, 0.0))
		- altitudeAt(gridUv - vec2(texelSize.x, 0.0))) / (2.0 * cellSizeMeters);
	// Row 0 is north and v grows with the row index, so northing runs the other way.
	float north = -(altitudeAt(gridUv + vec2(0.0, texelSize.y))
		- altitudeAt(gridUv - vec2(0.0, texelSize.y))) / (2.0 * cellSizeMeters);

	float normalLength = sqrt(east * east + north * north + 1.0);
	float lit = -east * sunDirection.x - north * (-sunDirection.z) + sunDirection.y;
	float shade = clamp(lit / normalLength, 0.0, 1.0);

	vec3 ramp = altitudeRamp(reliefBand(here, lowestAltitude, highestAltitude));
	vec3 colour = drapeMaterials(surfaceColor(ramp, painted, gridUv, paintedStrength), gridUv);
	vec3 shaded = colour * (ambient + (1.0 - ambient) * shade);
	gl_FragColor = vec4(drapeMask(shaded, overlay, gridUv, overlayStrength), 1.0);
}
`;
