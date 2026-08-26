#pragma once

#include <string_view>
#include <vector>

namespace terrain {

/**
 * A transfer curve: an editable mapping from [0, 1] to [0, 1] that a node
 * applies to a normalised field. Stored as text ("x,y;x,y;...") because that
 * is what crosses the plugin ABI, `.massifgraph`, and the DSL unchanged.
 *
 * Interpolation is monotone cubic (Fritsch-Carlson), which follows the points
 * smoothly without the overshoot a natural spline would add — an overshoot
 * here would invent altitudes nobody placed.
 */
class Curve {
public:
    struct Point {
        float x = 0.0f;
        float y = 0.0f;
    };

    /// Malformed text yields the identity curve rather than an error: a curve
    /// is an artistic control, and refusing to draw is worse than drawing
    /// nothing.
    static Curve parse(std::string_view text);

    float valueAt(float position) const;

    const std::vector<Point>& points() const { return points_; }

private:
    std::vector<Point> points_;
    std::vector<float> tangents_;

    void computeTangents();
};

}  // namespace terrain
