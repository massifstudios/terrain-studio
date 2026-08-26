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
 * Viridis at nine stops, matching `engine/src/thumbnail.cpp`. Perceptually
 * uniform in lightness, so a mask reads as a magnitude. Change one and change
 * the other.
 */
const maskRampGlsl = /* glsl */ `
vec3 maskRamp(float value) {
	vec3 stops[9];
	stops[0] = vec3(0.267, 0.005, 0.329);
	stops[1] = vec3(0.283, 0.141, 0.458);
	stops[2] = vec3(0.254, 0.265, 0.530);
	stops[3] = vec3(0.207, 0.372, 0.553);
	stops[4] = vec3(0.164, 0.471, 0.558);
	stops[5] = vec3(0.128, 0.567, 0.551);
	stops[6] = vec3(0.135, 0.659, 0.518);
	stops[7] = vec3(0.267, 0.749, 0.441);
	stops[8] = vec3(0.993, 0.906, 0.144);

	float scaled = clamp(value, 0.0, 1.0) * 8.0;
	int low = int(floor(scaled));
	int high = min(low + 1, 8);
	return mix(stops[low], stops[high], scaled - float(low));
}

vec3 drapeMask(vec3 shaded, sampler2D overlay, vec2 gridUv, float strength) {
	if (strength <= 0.0) {
		return shaded;
	}
	float value = texture2D(overlay, gridUv).r;
	// Keep some of the hillshade so the relief still reads under the mask.
	return mix(shaded, maskRamp(value) * (0.55 + 0.45 * dot(shaded, vec3(0.333))), strength);
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
${maskRampGlsl}

void main() {
	float shade = clamp(dot(normalize(worldNormal), sunDirection), 0.0, 1.0);
	vec3 ramp = altitudeRamp(reliefBand(altitudeMeters, lowestAltitude, highestAltitude));
	vec3 colour = surfaceColor(ramp, painted, gridUv, paintedStrength);
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
${maskRampGlsl}

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
	vec3 colour = surfaceColor(ramp, painted, gridUv, paintedStrength);
	vec3 shaded = colour * (ambient + (1.0 - ambient) * shade);
	gl_FragColor = vec4(drapeMask(shaded, overlay, gridUv, overlayStrength), 1.0);
}
`;
