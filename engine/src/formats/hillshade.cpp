#include "terrain/formats/hillshade.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

#include "terrain/formats/png_image.h"

namespace terrain::formats {
namespace {

struct SampledGrid {
    int width = 0;
    int height = 0;
    double spacingMeters = 1.0;
    std::vector<double> values;

    double at(int row, int column) const {
        return values[static_cast<std::size_t>(row) * width + column];
    }
};

struct SunVector {
    double east = 0.0;
    double north = 0.0;
    double up = 0.0;
};

int strideFor(int height, int previewSize) {
    return std::max(1, height / previewSize);
}

SampledGrid subsample(const Heightfield& field, int stride) {
    SampledGrid grid;
    grid.width = (field.width() + stride - 1) / stride;
    grid.height = (field.height() + stride - 1) / stride;
    grid.spacingMeters = field.cellSizeMeters() * stride;
    grid.values.resize(static_cast<std::size_t>(grid.width) * grid.height);

    for (int row = 0; row < grid.height; ++row) {
        for (int column = 0; column < grid.width; ++column) {
            grid.values[static_cast<std::size_t>(row) * grid.width + column] =
                field.at(row * stride, column * stride);
        }
    }
    return grid;
}

/// Central differences inside, one-sided at the edges: numpy's `gradient`
/// with the default `edge_order=1`.
double centralDifference(double before, double after, double spacing) {
    return (after - before) / (2.0 * spacing);
}

double rowGradient(const SampledGrid& grid, int row, int column) {
    if (grid.height < 2) {
        return 0.0;
    }
    if (row == 0) {
        return (grid.at(1, column) - grid.at(0, column)) / grid.spacingMeters;
    }
    if (row == grid.height - 1) {
        return (grid.at(row, column) - grid.at(row - 1, column)) / grid.spacingMeters;
    }
    return centralDifference(grid.at(row - 1, column), grid.at(row + 1, column),
                             grid.spacingMeters);
}

double columnGradient(const SampledGrid& grid, int row, int column) {
    if (grid.width < 2) {
        return 0.0;
    }
    if (column == 0) {
        return (grid.at(row, 1) - grid.at(row, 0)) / grid.spacingMeters;
    }
    if (column == grid.width - 1) {
        return (grid.at(row, column) - grid.at(row, column - 1)) / grid.spacingMeters;
    }
    return centralDifference(grid.at(row, column - 1), grid.at(row, column + 1),
                             grid.spacingMeters);
}

SunVector sunVectorFrom(const HillshadeOptions& options) {
    const double azimuth = options.sunAzimuthDegrees * std::numbers::pi / 180.0;
    const double altitude = options.sunAltitudeDegrees * std::numbers::pi / 180.0;
    return SunVector{std::sin(azimuth) * std::cos(altitude),
                     std::cos(azimuth) * std::cos(altitude), std::sin(altitude)};
}

double shadeAt(const SampledGrid& grid, int row, int column, const SunVector& sun) {
    const double gradientEast = columnGradient(grid, row, column);
    // Row 0 is north, so the row derivative runs opposite to northing.
    const double gradientNorth = -rowGradient(grid, row, column);
    const double normalLength =
        std::sqrt(gradientEast * gradientEast + gradientNorth * gradientNorth + 1.0);
    const double lit = -gradientEast * sun.east - gradientNorth * sun.north + sun.up;
    return std::clamp(lit / normalLength, 0.0, 1.0);
}

std::uint8_t rampByte(double base, double span, double band, double shade) {
    const double channel = (base + span * band) * shade * 255.0;
    return static_cast<std::uint8_t>(std::clamp(channel, 0.0, 255.0));
}

}  // namespace

std::filesystem::path previewPathFor(const std::filesystem::path& demPath) {
    std::filesystem::path preview = demPath;
    preview.replace_filename(demPath.stem().string() + "_preview.png");
    return preview;
}

void writeHillshadePreview(const std::filesystem::path& path, const Heightfield& field,
                           const HillshadeOptions& options) {
    const SampledGrid grid = subsample(field, strideFor(field.height(), options.previewSize));
    const auto [lowest, highest] = std::minmax_element(grid.values.begin(), grid.values.end());
    const double relief = std::max(*highest - *lowest, 1.0);
    const SunVector sun = sunVectorFrom(options);

    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(grid.width) * grid.height * 3);
    for (int row = 0; row < grid.height; ++row) {
        for (int column = 0; column < grid.width; ++column) {
            const double band = std::clamp((grid.at(row, column) - *lowest) / relief, 0.0, 1.0);
            const double shade = shadeAt(grid, row, column, sun);
            std::uint8_t* pixel =
                pixels.data() + (static_cast<std::size_t>(row) * grid.width + column) * 3;
            pixel[0] = rampByte(0.35, 0.65, band, shade);
            pixel[1] = rampByte(0.40, 0.55, band, shade);
            pixel[2] = rampByte(0.35, 0.45, band, shade);
        }
    }

    writePngRgb8(path, grid.width, grid.height, pixels);
}

}  // namespace terrain::formats
