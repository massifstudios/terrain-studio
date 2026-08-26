#pragma once

#include "terrain/color_ramp.h"

namespace terrain {

/// Hue in degrees over [0, 360), saturation and lightness over [0, 1].
struct HslColor {
    float hue = 0.0f;
    float saturation = 0.0f;
    float lightness = 0.0f;
};

/**
 * Colour is linear everywhere in the engine and no transfer function is
 * applied here either, so a hue rotation moves the same channel values the
 * ramp editor showed. A grey has no hue to speak of; it comes back as 0.
 */
HslColor toHsl(const RgbColor& color);
RgbColor toRgb(const HslColor& color);

/// Rec. 709 luminance: how bright a colour reads, rather than its mean.
float luminance(const RgbColor& color);

}  // namespace terrain
