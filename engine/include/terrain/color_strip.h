#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

#include "terrain/color_ramp.h"

namespace terrain {

/**
 * A colour lookup read from an image rather than typed as stops: one column
 * per position along [0, 1], and a handful of rows that each hold a different
 * shade the same ground shows at that position. Row 0 is the darkest shade
 * and the last row the brightest, which is how the satmap builder writes
 * them, so a caller that wants variation picks a row and one that wants the
 * plain colour reads the average.
 *
 * Channels are kept exactly as the file held them, scaled to [0, 1], with no
 * transfer function applied, the same rule every other colour in the engine
 * follows.
 */
class ColorStrip {
public:
    ColorStrip() = default;
    /// Rows of `width` colours each, three floats per colour, row-major.
    ColorStrip(int width, int rowCount, std::vector<float> samples);

    /// Reads a PNG. Throws `std::runtime_error` naming the file when it
    /// cannot be read or holds no pixels.
    static ColorStrip readPng(const std::filesystem::path& path);

    int width() const { return width_; }
    int rowCount() const { return rowCount_; }
    bool isEmpty() const { return samples_.empty(); }

    /// The colour at `position` in [0, 1] along one row, blended between the
    /// two nearest columns. Positions outside the strip read its ends.
    RgbColor sample(float position, int row) const;

    /// The mean of every row at `position`.
    RgbColor sampleAverage(float position) const;

private:
    RgbColor columnColor(std::span<const float> row, int column) const;
    RgbColor sampleRow(std::span<const float> row, float position) const;

    int width_ = 0;
    int rowCount_ = 0;
    std::vector<float> samples_;
    std::vector<float> average_;
};

}  // namespace terrain
