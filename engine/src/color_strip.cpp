#include "terrain/color_strip.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "terrain/formats/png_image.h"

namespace terrain {
namespace {

constexpr int channelCount = 3;

std::vector<float> averageOfRows(const std::vector<float>& samples, int width, int rowCount) {
    std::vector<float> average(static_cast<std::size_t>(width) * channelCount, 0.0f);
    for (int row = 0; row < rowCount; ++row) {
        const std::size_t offset = static_cast<std::size_t>(row) * width * channelCount;
        for (std::size_t index = 0; index < average.size(); ++index) {
            average[index] += samples[offset + index];
        }
    }
    for (float& value : average) {
        value /= static_cast<float>(rowCount);
    }
    return average;
}

/// Grey and RGBA files are accepted as well: a strip is a strip whatever
/// channel layout the image happened to be saved with.
float channelOf(const formats::PngImage& image, std::size_t pixel, int channel) {
    const float fullScale = static_cast<float>((1 << image.bitDepth) - 1);
    if (image.channels < 3) {
        return static_cast<float>(image.samples[pixel * image.channels]) / fullScale;
    }
    return static_cast<float>(image.samples[pixel * image.channels + channel]) / fullScale;
}

}  // namespace

ColorStrip::ColorStrip(int width, int rowCount, std::vector<float> samples)
    : width_(width), rowCount_(rowCount), samples_(std::move(samples)) {
    if (width_ <= 0 || rowCount_ <= 0
        || samples_.size() != static_cast<std::size_t>(width_) * rowCount_ * channelCount) {
        throw std::invalid_argument("a colour strip needs width * rows * 3 samples");
    }
    average_ = averageOfRows(samples_, width_, rowCount_);
}

ColorStrip ColorStrip::readPng(const std::filesystem::path& path) {
    formats::PngImage image;
    try {
        image = formats::readPng(path);
    } catch (const std::exception& error) {
        throw std::runtime_error("could not read colour strip " + path.string() + ": "
                                 + error.what());
    }
    if (image.width <= 0 || image.height <= 0) {
        throw std::runtime_error("colour strip " + path.string() + " holds no pixels");
    }

    std::vector<float> samples;
    samples.reserve(static_cast<std::size_t>(image.width) * image.height * channelCount);
    const std::size_t pixelCount = static_cast<std::size_t>(image.width) * image.height;
    for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
        for (int channel = 0; channel < channelCount; ++channel) {
            samples.push_back(channelOf(image, pixel, channel));
        }
    }
    return ColorStrip(image.width, image.height, std::move(samples));
}

RgbColor ColorStrip::columnColor(std::span<const float> row, int column) const {
    const std::size_t offset = static_cast<std::size_t>(column) * channelCount;
    return RgbColor{row[offset], row[offset + 1], row[offset + 2]};
}

RgbColor ColorStrip::sampleRow(std::span<const float> row, float position) const {
    const float scaled = std::clamp(position, 0.0f, 1.0f) * static_cast<float>(width_ - 1);
    const int low = static_cast<int>(std::floor(scaled));
    const int high = std::min(low + 1, width_ - 1);
    const float weight = scaled - static_cast<float>(low);
    const RgbColor left = columnColor(row, low);
    const RgbColor right = columnColor(row, high);
    return RgbColor{left.red + (right.red - left.red) * weight,
                    left.green + (right.green - left.green) * weight,
                    left.blue + (right.blue - left.blue) * weight};
}

RgbColor ColorStrip::sample(float position, int row) const {
    const int clampedRow = std::clamp(row, 0, rowCount_ - 1);
    const std::size_t rowLength = static_cast<std::size_t>(width_) * channelCount;
    return sampleRow(std::span<const float>(samples_).subspan(clampedRow * rowLength, rowLength),
                     position);
}

RgbColor ColorStrip::sampleAverage(float position) const {
    return sampleRow(average_, position);
}

}  // namespace terrain
