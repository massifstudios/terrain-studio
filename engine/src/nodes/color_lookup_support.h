#pragma once

#include <cstddef>
#include <cstdint>

#include "terrain/color_ramp.h"
#include "terrain_node_sdk.h"

namespace terrain::nodes {

/**
 * What every colour lookup shares: how a field becomes a position along a
 * gradient, and how the colour that comes back is graded. A gradient of typed
 * stops and a satellite strip differ only in what is read at the position.
 */

/// How the field is spread over the gradient before it is read.
enum class LookupEnhance {
    /// The field is read as it is, clamped to [0, 1]. For masks.
    None = 0,
    /// The field's own range is stretched over the gradient.
    Autolevel = 1,
    /// Every colour of the gradient gets the same share of ground.
    Equalize = 2
};

extern const TerrainEnumOption lookupEnhanceOptions[3];

struct LookupSettings {
    LookupEnhance enhance = LookupEnhance::Autolevel;
    bool reverse = false;
    /// Negative leans the gradient's low end over more ground, positive the
    /// high end. Zero reads it as it is.
    float bias = 0.0f;
    /// The segment of the gradient in use, so a strip can be clipped to the
    /// part that fits the ground.
    float rangeStart = 0.0f;
    float rangeEnd = 1.0f;
};

/// Where each cell reads the gradient, as [0, 1], after enhancement, reversal,
/// bias and clipping.
void lookupPositions(const float* input, std::size_t count, const LookupSettings& settings,
                     float* positions);

struct ColorGrade {
    float hueShiftDegrees = 0.0f;
    /// -1 drains every colour to grey, +1 pushes it to full saturation.
    float saturationShift = 0.0f;
    /// -1 takes every colour to black, +1 to white.
    float lightnessShift = 0.0f;

    bool isIdentity() const;
};

RgbColor gradeColor(const RgbColor& color, const ColorGrade& grade);

/// Uniform in [0, 1) for one cell, from a seed. The same cell and seed give
/// the same value on every machine.
float cellNoise(std::uint64_t seed, std::size_t index, std::uint64_t salt);

}  // namespace terrain::nodes
