#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

#include "terrain/heightfield.h"

namespace terrain::formats {

/// Matches `massif_dem.py`'s `write_preview` so a DEM written here looks the
/// same in the file browser as one produced by the game's own tools.
struct HillshadeOptions {
    int previewSize = 1200;
    double sunAzimuthDegrees = 315.0;
    double sunAltitudeDegrees = 45.0;
};

/// An 8-bit RGB image, row-major.
struct ShadedImage {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> rgb;
};

/// The shaded image the preview writer puts on disk, without writing it.
ShadedImage renderHillshade(const Heightfield& field, const HillshadeOptions& options = {});

/// `foo.massifdem` -> `foo_preview.png`, the convention the game's tools use.
std::filesystem::path previewPathFor(const std::filesystem::path& demPath);

void writeHillshadePreview(const std::filesystem::path& path, const Heightfield& field,
                           const HillshadeOptions& options = {});

}  // namespace terrain::formats
