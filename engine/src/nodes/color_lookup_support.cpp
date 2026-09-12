#include "color_lookup_support.h"

#include <algorithm>
#include <cmath>

#include "node_support.h"
#include "terrain/color_space.h"
#include "terrain/histogram.h"

namespace terrain::nodes {

const TerrainEnumOption lookupEnhanceOptions[3] = {
    {"none", "None", 0}, {"autolevel", "Autolevel", 1}, {"equalize", "Equalize", 2}};

namespace {

void enhancedPositions(const float* input, std::size_t count, LookupEnhance enhance,
                       float* positions) {
    if (enhance == LookupEnhance::Equalize) {
        const Histogram histogram(input, count);
        for (std::size_t index = 0; index < count; ++index) {
            positions[index] = histogram.quantileOfValue(input[index]);
        }
        return;
    }
    if (enhance == LookupEnhance::Autolevel) {
        const FieldRange range = fieldRange(input, count);
        for (std::size_t index = 0; index < count; ++index) {
            positions[index] = range.normalise(input[index]);
        }
        return;
    }
    for (std::size_t index = 0; index < count; ++index) {
        positions[index] = std::clamp(input[index], 0.0f, 1.0f);
    }
}

/// A power curve through (0, 0) and (1, 1): the exponent doubles for every
/// half turn of the dial, so the lean is even in either direction.
float biasExponent(float bias) {
    return std::exp2(-2.0f * std::clamp(bias, -1.0f, 1.0f));
}

}  // namespace

void lookupPositions(const float* input, std::size_t count, const LookupSettings& settings,
                     float* positions) {
    enhancedPositions(input, count, settings.enhance, positions);

    const float exponent = biasExponent(settings.bias);
    const float start = std::clamp(settings.rangeStart, 0.0f, 1.0f);
    const float end = std::clamp(settings.rangeEnd, 0.0f, 1.0f);
    for (std::size_t index = 0; index < count; ++index) {
        float position = std::clamp(positions[index], 0.0f, 1.0f);
        if (settings.reverse) {
            position = 1.0f - position;
        }
        if (exponent != 1.0f) {
            position = std::pow(position, exponent);
        }
        positions[index] = start + (end - start) * position;
    }
}

bool ColorGrade::isIdentity() const {
    return hueShiftDegrees == 0.0f && saturationShift == 0.0f && lightnessShift == 0.0f;
}

namespace {

/// Moves `value` towards 1 for a positive shift and towards 0 for a negative
/// one, by that share of the way, so neither end can overshoot.
float shiftedUnit(float value, float shift) {
    const float clampedShift = std::clamp(shift, -1.0f, 1.0f);
    if (clampedShift >= 0.0f) {
        return value + (1.0f - value) * clampedShift;
    }
    return value + value * clampedShift;
}

}  // namespace

RgbColor gradeColor(const RgbColor& color, const ColorGrade& grade) {
    if (grade.isIdentity()) {
        return color;
    }
    HslColor hsl = toHsl(color);
    const float turned = hsl.hue + grade.hueShiftDegrees;
    hsl.hue = turned - 360.0f * std::floor(turned / 360.0f);
    hsl.saturation = shiftedUnit(hsl.saturation, grade.saturationShift);
    hsl.lightness = shiftedUnit(hsl.lightness, grade.lightnessShift);
    return toRgb(hsl);
}

float cellNoise(std::uint64_t seed, std::size_t index, std::uint64_t salt) {
    std::uint64_t state = terrainSeedForLane(seed ^ (salt * 0xD6E8FEB86659FD93ull), index);
    return terrainRandomUnitFloat(&state);
}

}  // namespace terrain::nodes
