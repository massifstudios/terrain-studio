#pragma once

#include <cstddef>
#include <vector>

namespace terrain {

/**
 * The distribution of a field's values, as a cumulative curve over a fixed
 * number of bins.
 *
 * The bin count is a constant rather than a function of the data, so the same
 * field always yields the same curve and two fields can be read against each
 * other bin for bin. Counting is integer and the division to shares happens
 * once at the end, so no partial sum depends on the order the cells were read.
 */
class Histogram {
public:
    static constexpr int binCount = 4096;

    Histogram(const float* values, std::size_t count);

    float minimum() const { return minimum_; }
    float maximum() const { return maximum_; }
    /// True when every cell holds the same value, which leaves the curve flat
    /// and every remapping through it meaningless.
    bool isFlat() const;

    /// The value below which `fraction` of the field lies, in field units.
    float valueAtQuantile(float fraction) const;

    /// The share of the field at or below `value`, as [0, 1]. This is the
    /// equalising transfer function: applying it spreads the field evenly.
    float quantileOfValue(float value) const;

private:
    float valueAtBinEdge(std::size_t edge) const;

    float minimum_ = 0.0f;
    float maximum_ = 0.0f;
    /// binCount + 1 shares, rising from 0 at the low edge to 1 at the high one.
    std::vector<float> cumulative_;
};

}  // namespace terrain
