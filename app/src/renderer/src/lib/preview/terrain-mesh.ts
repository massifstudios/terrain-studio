import { BufferAttribute, PlaneGeometry } from 'three';
import type { FieldResult } from '$shared/engine-protocol';

/**
 * Geometry for the 3D view. The grid is displaced by the engine's own samples;
 * nothing here derives a height.
 *
 * `PlaneGeometry` lays vertices out row-major from its top-left corner, which
 * after the rotation to the ground plane is the north-west corner — the same
 * order the engine stores.
 */
export function spanMetersOf(field: FieldResult): number {
	return (field.width - 1) * field.cellSizeMeters;
}

export function buildTerrainGeometry(field: FieldResult): PlaneGeometry {
	const span = spanMetersOf(field);
	const geometry = new PlaneGeometry(span, span, field.width - 1, field.height - 1);
	geometry.rotateX(-Math.PI / 2);

	const position = geometry.attributes.position as BufferAttribute;
	for (let index = 0; index < field.samples.length; index += 1) {
		position.setY(index, field.samples[index]);
	}
	position.needsUpdate = true;
	geometry.computeVertexNormals();
	geometry.computeBoundingSphere();
	return geometry;
}

/// Ground position to grid cell. Row 0 is north, which is -Z.
export function cellAtGroundPosition(
	field: FieldResult,
	eastMeters: number,
	southMeters: number
): { row: number; column: number } {
	const span = spanMetersOf(field);
	return {
		row: (southMeters + span / 2) / field.cellSizeMeters,
		column: (eastMeters + span / 2) / field.cellSizeMeters
	};
}
