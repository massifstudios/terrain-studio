#include "terrain/thumbnail.h"

#include <algorithm>
#include <cmath>

#include "terrain/formats/hillshade.h"
#include "terrain/resample.h"

namespace terrain {
namespace {

/// A mask is quantised to flat grey steps: no hue, so it can never be read as
/// a shaded surface, and banded lightness so a threshold shows its own edge.
/// The 2D viewport's mask drape uses the same step count and range in GLSL;
/// change one and change both.
constexpr int maskStepCount = 8;
constexpr double darkestMaskStep = 0.08;
constexpr double lightestMaskStep = 0.94;

double maskStep(double position) {
    const double clamped = std::clamp(position, 0.0, 1.0);
    const double index =
        std::min(std::floor(clamped * maskStepCount), static_cast<double>(maskStepCount - 1));
    const double band = index / (maskStepCount - 1);
    return darkestMaskStep + (lightestMaskStep - darkestMaskStep) * band;
}

std::uint8_t toByte(double channel) {
    return static_cast<std::uint8_t>(std::clamp(channel * 255.0, 0.0, 255.0));
}

ThumbnailImage renderMaskThumbnail(const Heightfield& small, int size) {
    const float lowest = small.minimum();
    const float relief = std::max(small.maximum() - lowest, 1.0e-6f);

    ThumbnailImage image{size, std::vector<std::uint8_t>(
                                   static_cast<std::size_t>(size) * size * 4, 255)};
    for (std::size_t index = 0; index < small.sampleCount(); ++index) {
        const std::uint8_t grey = toByte(maskStep((small.data()[index] - lowest) / relief));
        std::uint8_t* pixel = image.rgba.data() + index * 4;
        pixel[0] = grey;
        pixel[1] = grey;
        pixel[2] = grey;
    }
    return image;
}

ThumbnailImage renderSurfaceThumbnail(const Heightfield& small, int size) {
    formats::HillshadeOptions options;
    options.previewSize = size;
    const formats::ShadedImage shaded = formats::renderHillshade(small, options);

    ThumbnailImage image{size, std::vector<std::uint8_t>(
                                   static_cast<std::size_t>(size) * size * 4, 255)};
    for (std::size_t index = 0; index < image.rgba.size() / 4; ++index) {
        std::copy_n(shaded.rgb.data() + index * 3, 3, image.rgba.data() + index * 4);
    }
    return image;
}

}  // namespace

ThumbnailImage renderThumbnail(const Heightfield& field, int size) {
    const Heightfield small = downsampleAverage(field, size);
    if (small.semantics() == FieldSemantics::Mask) {
        return renderMaskThumbnail(small, size);
    }
    return renderSurfaceThumbnail(small, size);
}

ThumbnailImage renderThumbnail(const ColorField& colors, int size) {
    ThumbnailImage image{size, std::vector<std::uint8_t>(
                                   static_cast<std::size_t>(size) * size * 4, 255)};
    const double step = static_cast<double>(colors.width()) / size;

    for (int row = 0; row < size; ++row) {
        for (int column = 0; column < size; ++column) {
            const int sourceRow = std::min(static_cast<int>(row * step), colors.height() - 1);
            const int sourceColumn = std::min(static_cast<int>(column * step), colors.width() - 1);
            const float* source = colors.at(sourceRow, sourceColumn);
            std::uint8_t* pixel =
                image.rgba.data() + (static_cast<std::size_t>(row) * size + column) * 4;
            for (int channel = 0; channel < ColorField::channelCount; ++channel) {
                pixel[channel] = toByte(source[channel]);
            }
        }
    }
    return image;
}

ThumbnailImage renderThumbnail(const PointSet& points, int size, double worldSizeMeters) {
    ThumbnailImage image{size, std::vector<std::uint8_t>(
                                   static_cast<std::size_t>(size) * size * 4, 255)};
    for (std::size_t index = 0; index < image.rgba.size(); index += 4) {
        image.rgba[index] = 24;
        image.rgba[index + 1] = 26;
        image.rgba[index + 2] = 30;
    }

    const double scale = size / std::max(worldSizeMeters, 1.0);
    for (const TerrainPoint& point : points.points()) {
        // Row 0 is north, so northing counts down the image.
        const int row = static_cast<int>((worldSizeMeters - point.northingMeters) * scale);
        const int column = static_cast<int>(point.eastingMeters * scale);
        if (row < 0 || row >= size || column < 0 || column >= size) {
            continue;
        }
        std::uint8_t* pixel =
            image.rgba.data() + (static_cast<std::size_t>(row) * size + column) * 4;
        pixel[0] = 134;
        pixel[1] = 205;
        pixel[2] = 116;
    }
    return image;
}

}  // namespace terrain
