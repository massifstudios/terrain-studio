#pragma once

#include <filesystem>

#include "terrain/heightfield.h"

namespace terrain::formats {

/// Matches `massif_dem.py`'s `write_preview` so a DEM written here looks the
/// same in the file browser as one produced by the game's own tools.
struct HillshadeOptions {
    int previewSize = 1200;
    double sunAzimuthDegrees = 315.0;
    double sunAltitudeDegrees = 45.0;
};

/// `foo.massifdem` -> `foo_preview.png`, the convention the game's tools use.
std::filesystem::path previewPathFor(const std::filesystem::path& demPath);

void writeHillshadePreview(const std::filesystem::path& path, const Heightfield& field,
                           const HillshadeOptions& options = {});

}  // namespace terrain::formats
