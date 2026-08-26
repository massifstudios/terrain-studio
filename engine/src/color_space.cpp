#include "terrain/color_space.h"

#include <algorithm>
#include <cmath>

namespace terrain {
namespace {

/// One channel of the hue wheel: a trapezoid that holds at `high` for a sixth
/// of the turn, falls to `low`, holds, and rises again.
float hueChannel(float low, float high, float position) {
    const float wrapped = position - std::floor(position);
    if (wrapped < 1.0f / 6.0f) {
        return low + (high - low) * 6.0f * wrapped;
    }
    if (wrapped < 1.0f / 2.0f) {
        return high;
    }
    if (wrapped < 2.0f / 3.0f) {
        return low + (high - low) * (2.0f / 3.0f - wrapped) * 6.0f;
    }
    return low;
}

float hueDegrees(const RgbColor& color, float high, float span) {
    float sixths = 4.0f + (color.red - color.green) / span;
    if (high == color.red) {
        sixths = (color.green - color.blue) / span;
    } else if (high == color.green) {
        sixths = 2.0f + (color.blue - color.red) / span;
    }
    const float degrees = sixths * 60.0f;
    return degrees - 360.0f * std::floor(degrees / 360.0f);
}

}  // namespace

HslColor toHsl(const RgbColor& color) {
    const float high = std::max({color.red, color.green, color.blue});
    const float low = std::min({color.red, color.green, color.blue});

    HslColor result;
    result.lightness = (high + low) * 0.5f;
    const float span = high - low;
    if (span <= 0.0f) {
        return result;
    }

    result.saturation = span / (2.0f - high - low);
    if (result.lightness < 0.5f) {
        result.saturation = span / (high + low);
    }
    result.hue = hueDegrees(color, high, span);
    return result;
}

RgbColor toRgb(const HslColor& color) {
    const float saturation = std::clamp(color.saturation, 0.0f, 1.0f);
    const float lightness = std::clamp(color.lightness, 0.0f, 1.0f);
    if (saturation <= 0.0f) {
        return RgbColor{lightness, lightness, lightness};
    }

    float high = lightness + saturation - lightness * saturation;
    if (lightness < 0.5f) {
        high = lightness * (1.0f + saturation);
    }
    const float low = 2.0f * lightness - high;
    const float turns = color.hue / 360.0f;
    return RgbColor{hueChannel(low, high, turns + 1.0f / 3.0f), hueChannel(low, high, turns),
                    hueChannel(low, high, turns - 1.0f / 3.0f)};
}

float luminance(const RgbColor& color) {
    return 0.2126f * color.red + 0.7152f * color.green + 0.0722f * color.blue;
}

}  // namespace terrain
