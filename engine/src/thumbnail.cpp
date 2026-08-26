#include "terrain/thumbnail.h"

#include <algorithm>
#include <array>
#include <cmath>

#include "terrain/formats/hillshade.h"
#include "terrain/resample.h"

namespace terrain {
namespace {

struct RampStop {
    double red;
    double green;
    double blue;
};

/// Viridis, sampled at nine stops. Perceptually uniform in lightness, so a
/// mask reads as a magnitude rather than as a picture. The 2D viewport's
/// mask drape carries the same stops in GLSL; change one and change both.
constexpr std::array<RampStop, 9> maskRamp = {{{0.267, 0.005, 0.329},
                                               {0.283, 0.141, 0.458},
                                               {0.254, 0.265, 0.530},
                                               {0.207, 0.372, 0.553},
                                               {0.164, 0.471, 0.558},
                                               {0.128, 0.567, 0.551},
                                               {0.135, 0.659, 0.518},
                                               {0.267, 0.749, 0.441},
                                               {0.993, 0.906, 0.144}}};

RampStop sampleRamp(double position) {
    const double scaled = std::clamp(position, 0.0, 1.0) * (maskRamp.size() - 1);
    const std::size_t low = static_cast<std::size_t>(scaled);
    const std::size_t high = std::min(low + 1, maskRamp.size() - 1);
    const double fraction = scaled - low;

    return RampStop{maskRamp[low].red + (maskRamp[high].red - maskRamp[low].red) * fraction,
                    maskRamp[low].green + (maskRamp[high].green - maskRamp[low].green) * fraction,
                    maskRamp[low].blue + (maskRamp[high].blue - maskRamp[low].blue) * fraction};
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
        const RampStop colour = sampleRamp((small.data()[index] - lowest) / relief);
        std::uint8_t* pixel = image.rgba.data() + index * 4;
        pixel[0] = toByte(colour.red);
        pixel[1] = toByte(colour.green);
        pixel[2] = toByte(colour.blue);
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
