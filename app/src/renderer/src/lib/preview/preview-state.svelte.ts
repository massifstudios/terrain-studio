import type { ColorResult, FieldResult, PointsResult } from '$shared/engine-protocol';

/**
 * The cell under the pointer, in grid coordinates so both views mirror the
 * same thing exactly. Row 0 is north, matching the engine.
 */
export interface CursorCell {
	row: number;
	column: number;
	altitudeMeters: number;
}

/// Tiled textures over `field`: one weight mask per layer, and the texture
/// each layer shows, in the same order.
export interface MaterialPreview {
	weights: FieldResult[];
	textureNames: string[];
}

/// Shared by the 3D and 2D views: what is being previewed, how it is lit, and
/// where the pointer is.
class PreviewState {
	field = $state<FieldResult | null>(null);
	/// A mask draped over `field`, so it is always read in terrain context.
	overlay = $state<FieldResult | null>(null);
	/// A colour map painted onto `field`, for the same reason.
	painted = $state<ColorResult | null>(null);
	/// Scattered instances standing on `field`, drawn as markers.
	points = $state<PointsResult | null>(null);
	materials = $state<MaterialPreview | null>(null);
	/// Ground distance one texture tile covers. Presentation only: a game tiles
	/// the same weights at whatever scale its own materials use.
	tileSizeMeters = $state(20);
	sunAzimuthDegrees = $state(315);
	sunAltitudeDegrees = $state(45);
	showTopDownView = $state(true);
	cursor = $state<CursorCell | null>(null);

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
