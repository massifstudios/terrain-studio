#pragma once

#include <filesystem>

#include "terrain/heightfield.h"

namespace terrain::formats {

/// Extra shape that only some formats need: a PNG carries no altitudes, and a
/// raw float32 file carries no dimensions.
struct ElevationImportOptions {
    double minimumAltitudeMeters = 0.0;
    double maximumAltitudeMeters = 1000.0;
    int rawWidth = 0;
    int rawHeight = 0;
};

/// Reads whichever elevation format the extension names. This is the only
/// place that decides what a file is; nodes and the graph never look.
Heightfield readElevationModel(const std::filesystem::path& path,
                               const ElevationImportOptions& options);

}  // namespace terrain::formats
