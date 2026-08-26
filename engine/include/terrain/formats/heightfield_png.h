#pragma once

#include <filesystem>

#include "terrain/heightfield.h"

namespace terrain::formats {

/// A 16-bit PNG carries no altitudes, so the importer is told the band the
/// full sample range stands for.
struct HeightPngBand {
    double cellSizeMeters = 1.0;
    double minimumAltitudeMeters = 0.0;
    double maximumAltitudeMeters = 1.0;
};

Heightfield readHeightfieldPng(const std::filesystem::path& path, const HeightPngBand& band);

/// Quantises over the field's own altitude band, as `.massifdem` does. Masks
/// are written the same way, which for values in [0, 1] is the identity band.
void writeHeightfieldPng16(const std::filesystem::path& path, const Heightfield& field);

}  // namespace terrain::formats
