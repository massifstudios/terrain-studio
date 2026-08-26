import type { FieldResult } from '$shared/engine-protocol';

/**
 * The cell under the pointer, in grid coordinates so both views mirror the
 * same thing exactly. Row 0 is north, matching the engine.
 */
export interface CursorCell {
	row: number;
	column: number;
	altitudeMeters: number;
}

/// Shared by the 3D and 2D views: what is being previewed, how it is lit, and
/// where the pointer is.
class PreviewState {
	field = $state<FieldResult | null>(null);
	sunAzimuthDegrees = $state(315);
	sunAltitudeDegrees = $state(45);
	showTopDownView = $state(true);
	cursor = $state<CursorCell | null>(null);
	errors = $state<string[]>([]);

	get worldSizeMeters(): number {
		if (!this.field) {
			return 0;
		}
		return this.field.width * this.field.cellSizeMeters;
	}

	/// A lookup into the grid the engine produced, never a computed height.
	altitudeAt(row: number, column: number): number | null {
		const field = this.field;
		if (!field) {
			return null;
		}
		if (row < 0 || column < 0 || row >= field.height || column >= field.width) {
			return null;
		}
		return field.samples[row * field.width + column];
	}

	setCursorCell(row: number, column: number): void {
		const altitude = this.altitudeAt(Math.round(row), Math.round(column));
		if (altitude === null) {
			this.cursor = null;
			return;
		}
		this.cursor = { row: Math.round(row), column: Math.round(column), altitudeMeters: altitude };
	}

	clearCursor(): void {
		this.cursor = null;
	}
}

export const previewState = new PreviewState();
