#include "terrain/formats/heightfield_png.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

#include "terrain/formats/png_image.h"

namespace terrain::formats {
namespace {

constexpr double quantisationLevels = 65535.0;

double fullScaleFor(int bitDepth) {
    if (bitDepth == 16) {
        return 65535.0;
    }
    return 255.0;
}

}  // namespace

Heightfield readHeightfieldPng(const std::filesystem::path& path, const HeightPngBand& band) {
    const PngImage image = readPng(path);
    if (image.channels != 1 && image.channels != 2) {
        throw std::runtime_error("Heightfield PNG import needs a grayscale image");
    }

    const double fullScale = fullScaleFor(image.bitDepth);
    const double relief = band.maximumAltitudeMeters - band.minimumAltitudeMeters;

    Heightfield field(image.width, image.height);
    field.setCellSizeMeters(band.cellSizeMeters);
    std::span<float> destination = field.samples();
    for (std::size_t index = 0; index < destination.size(); ++index) {
        const double normalised = image.samples[index * image.channels] / fullScale;
        destination[index] =
            static_cast<float>(band.minimumAltitudeMeters + normalised * relief);
    }
    return field;
}

void writeHeightfieldPng16(const std::filesystem::path& path, const Heightfield& field) {
    if (field.isEmpty()) {
        throw std::runtime_error("Cannot write an empty heightfield PNG");
    }

    const double minimum = field.minimum();
    const double relief = std::max(static_cast<double>(field.maximum()) - minimum, 1.0e-6);

    std::vector<std::uint16_t> samples(field.sampleCount());
    const std::span<const float> source = field.samples();
    for (std::size_t index = 0; index < source.size(); ++index) {
        const double level = std::nearbyint((source[index] - minimum) / relief * quantisationLevels);
        samples[index] = static_cast<std::uint16_t>(std::clamp(level, 0.0, quantisationLevels));
    }

    writePngGray16(path, field.width(), field.height(), samples);
}

}  // namespace terrain::formats
