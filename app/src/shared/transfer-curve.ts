/**
 * The editor's half of a transfer curve. The engine owns what a curve does to
 * a field (`engine/src/curve.cpp`); this owns the same shape only so the
 * control can draw what the artist is about to get. Both read and write the
 * one text form, "x,y;x,y;...", which is what crosses the plugin ABI and what
 * `.massifgraph` stores.
 *
 * The interpolation here mirrors the engine's monotone cubic exactly. Change
 * one and change the other, or the preview starts lying.
 */

export interface CurvePoint {
	x: number;
	y: number;
}

const identity: CurvePoint[] = [
	{ x: 0, y: 0 },
	{ x: 1, y: 1 }
];

function clampUnit(value: number): number {
	return Math.min(Math.max(value, 0), 1);
}

function readPoint(text: string): CurvePoint | null {
	const parts = text.split(',');
	if (parts.length !== 2) {
		return null;
	}
	const x = Number(parts[0]);
	const y = Number(parts[1]);
	if (!Number.isFinite(x) || !Number.isFinite(y)) {
		return null;
	}
	return { x: clampUnit(x), y: clampUnit(y) };
}

/// Unusable text yields the identity curve rather than an error, matching the
/// engine: a curve is an artistic control, and refusing to draw helps nobody.
export function parseCurve(text: string): CurvePoint[] {
	const parsed = text
		.split(';')
		.map(readPoint)
		.filter((point): point is CurvePoint => point !== null)
		.sort((left, right) => left.x - right.x)
		.filter((point, index, all) => index === 0 || point.x !== all[index - 1].x);

	if (parsed.length < 2) {
		return identity.map((point) => ({ ...point }));
	}
	return parsed;
}

function formatNumber(value: number): string {
	return Number(value.toFixed(4)).toString();
}

export function serializeCurve(points: CurvePoint[]): string {
	return points
		.slice()
		.sort((left, right) => left.x - right.x)
		.map((point) => `${formatNumber(point.x)},${formatNumber(point.y)}`)
		.join(';');
}

/// Fritsch-Carlson: clamp the slopes so no segment can turn back on itself.
function limitedTangent(tangent: number, leftSlope: number, rightSlope: number): number {
	if (leftSlope * rightSlope <= 0) {
		return 0;
	}
	const limit = 3 * Math.min(Math.abs(leftSlope), Math.abs(rightSlope));
	return Math.min(Math.max(tangent, -limit), limit);
}

function tangentsFor(points: CurvePoint[]): number[] {
	const slopes: number[] = [];
	for (let index = 0; index + 1 < points.length; index += 1) {
		slopes.push((points[index + 1].y - points[index].y) / (points[index + 1].x - points[index].x));
	}

	const tangents = points.map(() => 0);
	tangents[0] = slopes[0];
	tangents[tangents.length - 1] = slopes[slopes.length - 1];
	for (let index = 1; index + 1 < points.length; index += 1) {
		const average = (slopes[index - 1] + slopes[index]) / 2;
		tangents[index] = limitedTangent(average, slopes[index - 1], slopes[index]);
	}
	return tangents;
}

function segmentIndexFor(points: CurvePoint[], position: number): number {
	for (let index = 1; index < points.length; index += 1) {
		if (position < points[index].x) {
			return index - 1;
		}
	}
	return points.length - 2;
}

export function curveValueAt(points: CurvePoint[], position: number): number {
	if (position <= points[0].x) {
		return points[0].y;
	}
	const last = points[points.length - 1];
	if (position >= last.x) {
		return last.y;
	}

	const tangents = tangentsFor(points);
	const low = segmentIndexFor(points, position);
	const high = low + 1;
	const width = points[high].x - points[low].x;
	const t = (position - points[low].x) / width;
	const squared = t * t;
	const cubed = squared * t;

	return (
		(2 * cubed - 3 * squared + 1) * points[low].y +
		(cubed - 2 * squared + t) * width * tangents[low] +
		(-2 * cubed + 3 * squared) * points[high].y +
		(cubed - squared) * width * tangents[high]
	);
}

function pathCommand(step: number): string {
	if (step === 0) {
		return 'M';
	}
	return 'L';
}

/// The curve as an SVG path in a unit box, y already flipped for the screen.
export function curvePath(points: CurvePoint[], samples: number): string {
	const steps: string[] = [];
	for (let step = 0; step <= samples; step += 1) {
		const position = step / samples;
		const value = clampUnit(curveValueAt(points, position));
		steps.push(`${pathCommand(step)} ${position} ${1 - value}`);
	}
	return steps.join(' ');
}
