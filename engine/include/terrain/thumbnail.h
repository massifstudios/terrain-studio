#pragma once

#include <cstdint>
#include <vector>

#include "terrain/color_field.h"
#include "terrain/heightfield.h"
#include "terrain/point_set.h"

namespace terrain {

/// A small square RGBA image the graph editor blits onto a node body. The
/// engine shades it so no part of the UI has to look at heights.
struct ThumbnailImage {
    int size = 0;
    std::vector<std::uint8_t> rgba;
};

/**
 * Heightfields get the same hillshade an exported preview does. Masks get a
 * perceptual ramp, because a mask has no relief to light — only a magnitude,
 * and a ramp that is uniform in lightness is the only kind that reads
 * honestly.
 */
ThumbnailImage renderThumbnail(const Heightfield& field, int size);

/// Colour maps are shown as themselves, area-averaged down.
ThumbnailImage renderThumbnail(const ColorField& colors, int size);

/// A point set has no image of its own, so it gets a plan view: one dot per
/// instance over an empty ground. `worldSizeMeters` is what maps a position to
/// a pixel.
ThumbnailImage renderThumbnail(const PointSet& points, int size, double worldSizeMeters);

}  // namespace terrain
