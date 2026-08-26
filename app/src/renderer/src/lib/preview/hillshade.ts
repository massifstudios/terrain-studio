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

const altitudeRampGlsl = /* glsl */ `
vec3 altitudeRamp(float band) {
	return vec3(0.35 + 0.65 * band, 0.40 + 0.55 * band, 0.35 + 0.45 * band);
}

float reliefBand(float altitude, float lowest, float highest) {
	return clamp((altitude - lowest) / max(highest - lowest, 1.0), 0.0, 1.0);
}
`;

export const surfaceVertexShader = /* glsl */ `
varying vec3 worldNormal;
varying float altitudeMeters;

void main() {
	worldNormal = normalize(mat3(modelMatrix) * normal);
	altitudeMeters = position.y;
	gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);
}
`;

export const surfaceFragmentShader = /* glsl */ `
uniform vec3 sunDirection;
uniform float lowestAltitude;
uniform float highestAltitude;
uniform float ambient;

varying vec3 worldNormal;
varying float altitudeMeters;

${altitudeRampGlsl}

void main() {
	float shade = clamp(dot(normalize(worldNormal), sunDirection), 0.0, 1.0);
	vec3 colour = altitudeRamp(reliefBand(altitudeMeters, lowestAltitude, highestAltitude));
	gl_FragColor = vec4(colour * (ambient + (1.0 - ambient) * shade), 1.0);
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

varying vec2 gridUv;

${altitudeRampGlsl}

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

	vec3 colour = altitudeRamp(reliefBand(here, lowestAltitude, highestAltitude));
	gl_FragColor = vec4(colour * (ambient + (1.0 - ambient) * shade), 1.0);
}
`;
