#include "terrain/curve.h"

#include <algorithm>
#include <charconv>
#include <cmath>

namespace terrain {
namespace {

Curve::Point identityStart() {
    return Curve::Point{0.0f, 0.0f};
}

Curve::Point identityEnd() {
    return Curve::Point{1.0f, 1.0f};
}

/// One "x,y" pair. Returns false on anything it cannot read whole.
bool parsePoint(std::string_view text, Curve::Point& point) {
    const std::size_t comma = text.find(',');
    if (comma == std::string_view::npos) {
        return false;
    }
    const std::string_view left = text.substr(0, comma);
    const std::string_view right = text.substr(comma + 1);

    const auto readX = std::from_chars(left.data(), left.data() + left.size(), point.x);
    if (readX.ec != std::errc{}) {
        return false;
    }
    const auto readY = std::from_chars(right.data(), right.data() + right.size(), point.y);
    return readY.ec == std::errc{};
}

std::vector<Curve::Point> parsePoints(std::string_view text) {
    std::vector<Curve::Point> points;
    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t separator = std::min(text.find(';', start), text.size());
        Curve::Point point;
        if (parsePoint(text.substr(start, separator - start), point)) {
            points.push_back(Curve::Point{std::clamp(point.x, 0.0f, 1.0f),
                                          std::clamp(point.y, 0.0f, 1.0f)});
        }
        start = separator + 1;
    }
    return points;
}

/// Fritsch-Carlson: clamp the slopes so no segment can turn back on itself.
float limitedTangent(float tangent, float leftSlope, float rightSlope) {
    if (leftSlope * rightSlope <= 0.0f) {
        return 0.0f;
    }
    const float limit = 3.0f * std::min(std::abs(leftSlope), std::abs(rightSlope));
    return std::clamp(tangent, -limit, limit);
}

}  // namespace

Curve Curve::parse(std::string_view text) {
    Curve curve;
    curve.points_ = parsePoints(text);

    std::stable_sort(curve.points_.begin(), curve.points_.end(),
                     [](const Point& left, const Point& right) { return left.x < right.x; });
    // Two distinct points at the same x would make a segment of zero width.
    const auto duplicate = std::unique(
        curve.points_.begin(), curve.points_.end(),
        [](const Point& left, const Point& right) { return left.x == right.x; });
    curve.points_.erase(duplicate, curve.points_.end());

    if (curve.points_.size() < 2) {
        curve.points_ = {identityStart(), identityEnd()};
    }
    curve.computeTangents();
    return curve;
}

void Curve::computeTangents() {
    const std::size_t count = points_.size();
    std::vector<float> slopes(count - 1);
    for (std::size_t index = 0; index + 1 < count; ++index) {
        slopes[index] =
            (points_[index + 1].y - points_[index].y) / (points_[index + 1].x - points_[index].x);
    }

    tangents_.assign(count, 0.0f);
    tangents_.front() = slopes.front();
    tangents_.back() = slopes.back();
    for (std::size_t index = 1; index + 1 < count; ++index) {
        const float average = (slopes[index - 1] + slopes[index]) * 0.5f;
        tangents_[index] = limitedTangent(average, slopes[index - 1], slopes[index]);
    }
}

float Curve::valueAt(float position) const {
    if (position <= points_.front().x) {
        return points_.front().y;
    }
    if (position >= points_.back().x) {
        return points_.back().y;
    }

    const auto above = std::upper_bound(
        points_.begin(), points_.end(), position,
        [](float value, const Point& point) { return value < point.x; });
    const std::size_t high = static_cast<std::size_t>(above - points_.begin());
    const std::size_t low = high - 1;

    const float width = points_[high].x - points_[low].x;
    const float t = (position - points_[low].x) / width;
    const float t2 = t * t;
    const float t3 = t2 * t;

    return (2.0f * t3 - 3.0f * t2 + 1.0f) * points_[low].y
           + (t3 - 2.0f * t2 + t) * width * tangents_[low]
           + (-2.0f * t3 + 3.0f * t2) * points_[high].y
           + (t3 - t2) * width * tangents_[high];
}

}  // namespace terrain
