/**
 * How a drag turns into a number.
 *
 * A parameter's range is whatever the engine declared, from a fraction to tens
 * of thousands, and the control is the same hundred pixels wide either way.
 * Pixels are therefore worth a quantum drawn from the range's own order of
 * magnitude, multiplied by how fast the pointer is moving: a slow drag places a
 * value exactly, a flick crosses the range.
 */

const slowDragPixels = 2;
const accelerationPerPixel = 0.3;
const accelerationCeiling = 10;
const pixelsToCrossAtFullSpeed = 400;
const largestDecimals = 6;

/// A thousandth of the range's own order of magnitude: 0.001 for a range of
/// one, 10 for a range of a hundred thousand.
export function scrubQuantum(minimum: number, maximum: number, step?: number): number {
	if (step !== undefined && step > 0) {
		return step;
	}
	const span = spanOf(minimum, maximum);
	const magnitude = Math.pow(10, Math.floor(Math.log10(span)));
	return magnitude / 1000;
}

export function spanOf(minimum: number, maximum: number): number {
	return Math.max(maximum - minimum, Number.EPSILON);
}

export function scrubDecimals(quantum: number): number {
	const places = -Math.floor(Math.log10(quantum));
	return Math.min(Math.max(places, 0), largestDecimals);
}

/// A narrow range must not accelerate into uselessness: even at full speed,
/// crossing it takes a few hundred pixels of travel.
export function scrubAccelerationLimit(span: number, quantum: number): number {
	const needed = span / (quantum * pixelsToCrossAtFullSpeed);
	return Math.min(accelerationCeiling, Math.max(1, needed));
}

export function scrubAcceleration(pixels: number, limit: number): number {
	const speed = Math.abs(pixels);
	if (speed <= slowDragPixels) {
		return 1;
	}
	return Math.min(1 + (speed - slowDragPixels) * accelerationPerPixel, limit);
}

export function quantizeValue(
	candidate: number,
	quantum: number,
	minimum: number,
	maximum: number
): number {
	const held = Math.min(Math.max(candidate, minimum), maximum);
	const snapped = Math.round(held / quantum) * quantum;
	return Number(snapped.toFixed(scrubDecimals(quantum)));
}
