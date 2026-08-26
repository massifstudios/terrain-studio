#pragma once

#include "terrain/heightfield.h"

namespace terrain {

/// Bilinear sample at a fractional grid position, clamped at the edges.
float sampleBilinear(const Heightfield& field, double row, double column);

/// Maps the whole of `source` onto a square grid of the given resolution. The
/// graph evaluates at one resolution, so every import lands on that grid.
Heightfield resampleToSquare(const Heightfield& source, int resolution, double worldSizeMeters);

/// Area average down to a small square, for node thumbnails. Averaging rather
/// than point sampling keeps a thumbnail from aliasing away the relief.
Heightfield downsampleAverage(const Heightfield& source, int size);

}  // namespace terrain
