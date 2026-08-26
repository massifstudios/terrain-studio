/**
 * The editor's half of a colour ramp. The engine owns what a ramp does to a
 * field (`engine/src/color_ramp.cpp`); this owns the same text form so the
 * control can show the gradient the artist is about to get.
 *
 * The text is `"0:#1e2d24;0.5:#8d7b5f;1:#ffffff"` — positions in [0, 1], six
 * digit hex — and is what crosses the plugin ABI and `.massifgraph`.
 */

export interface RampStop {
	position: number;
	color: string;
}

const blackToWhite: RampStop[] = [
	{ position: 0, color: '#000000' },
	{ position: 1, color: '#ffffff' }
];

const hexPattern = /^#[0-9a-f]{6}$/i;

function clampUnit(value: number): number {
	return Math.min(Math.max(value, 0), 1);
}

function readStop(text: string): RampStop | null {
	const colon = text.indexOf(':');
	if (colon < 0) {
		return null;
	}
	const position = Number(text.slice(0, colon));
	const color = text.slice(colon + 1);
	if (!Number.isFinite(position) || !hexPattern.test(color)) {
		return null;
	}
	return { position: clampUnit(position), color: color.toLowerCase() };
}

/// Unusable text yields a black-to-white ramp rather than an error, matching
/// the engine: a ramp is an artistic control, and refusing to draw helps nobody.
export function parseRamp(text: string): RampStop[] {
	const parsed = text
		.split(';')
		.map(readStop)
		.filter((stop): stop is RampStop => stop !== null)
		.sort((left, right) => left.position - right.position);

	if (parsed.length === 0) {
		return blackToWhite.map((stop) => ({ ...stop }));
	}
	return parsed;
}

function formatPosition(position: number): string {
	return Number(position.toFixed(4)).toString();
}

export function serializeRamp(stops: RampStop[]): string {
	return stops
		.slice()
		.sort((left, right) => left.position - right.position)
		.map((stop) => `${formatPosition(stop.position)}:${stop.color}`)
		.join(';');
}

/// The ramp as a CSS gradient, so the strip in the property panel is drawn by
/// the browser rather than sampled a pixel at a time.
export function rampGradientCss(stops: RampStop[]): string {
	const steps = stops.map((stop) => `${stop.color} ${stop.position * 100}%`);
	return `linear-gradient(to right, ${steps.join(', ')})`;
}
