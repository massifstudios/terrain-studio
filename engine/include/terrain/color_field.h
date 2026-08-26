#pragma once

#include <cstddef>
#include <span>
#include <vector>

namespace terrain {

/**
 * Row-major RGB grid, three floats per cell, on the same square as every
 * heightfield in the graph. Row 0 is north.
 *
 * Channels are in [0, 1] and no gamma is applied anywhere: a colour written
 * as #808080 in a ramp comes out of the writer as 128, 128, 128. Guessing at
 * a transfer function would make an exported colour map disagree with the
 * swatch the artist picked.
 */
class ColorField {
public:
    static constexpr int channelCount = 3;

    ColorField() = default;
    ColorField(int width, int height);

    int width() const { return width_; }
    int height() const { return height_; }
    bool isEmpty() const { return samples_.empty(); }
    std::size_t cellCount() const { return samples_.size() / channelCount; }

    float* at(int row, int column);
    const float* at(int row, int column) const;

    float* data() { return samples_.data(); }
    const float* data() const { return samples_.data(); }

    std::span<float> samples() { return samples_; }
    std::span<const float> samples() const { return samples_; }

private:
    int width_ = 0;
    int height_ = 0;
    std::vector<float> samples_;
};

}  // namespace terrain
