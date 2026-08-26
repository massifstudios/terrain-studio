#pragma once

#include <filesystem>

#include "terrain/heightfield.h"

namespace terrain::formats {

/// Headerless little-endian float32 samples, row-major, row 0 north. The
/// escape hatch for tools that export nothing else, so the caller supplies
/// the shape the file cannot carry.
struct RawFloat32Layout {
    int width = 0;
    int height = 0;
    double cellSizeMeters = 1.0;
};

Heightfield readRawFloat32(const std::filesystem::path& path, const RawFloat32Layout& layout);

}  // namespace terrain::formats
