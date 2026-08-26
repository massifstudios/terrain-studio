#pragma once

#include <string_view>
#include <vector>

namespace terrain {

struct RgbColor {
    float red = 0.0f;
    float green = 0.0f;
    float blue = 0.0f;
};

/**
 * An editable gradient: an ordered set of coloured stops over [0, 1].
 *
 * Stored as text, `"0:#1e2d24;0.5:#8d7b5f;1:#ffffff"`, because that is what
 * crosses the plugin ABI, `.massifgraph`, and the DSL unchanged. Positions are
 * in [0, 1] and colours are six-digit hex.
 */
class ColorRamp {
public:
    struct Stop {
        float position = 0.0f;
        RgbColor color;
    };

    /// Unusable text yields a black-to-white ramp rather than an error: a ramp
    /// is an artistic control, and refusing to draw helps nobody.
    static ColorRamp parse(std::string_view text);

    RgbColor sample(float position) const;

    const std::vector<Stop>& stops() const { return stops_; }

private:
    std::vector<Stop> stops_;
};

}  // namespace terrain
