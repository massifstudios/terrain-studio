#pragma once

#include <filesystem>
#include <string>
#include <string_view>

#include "terrain/heightfield.h"

namespace terrain::formats {

/**
 * `.massifdem` reader and writer. The byte layout is owned by the game Massif
 * and defined by `Tools/massif_dem.py`: "MASSIFDM" magic, version 1, a
 * 192-byte header, then uint16 samples quantised over the window's own
 * altitude band, row-major, row 0 north. It is never changed from this side.
 */

inline constexpr std::size_t massifDemHeaderBytes = 192;
inline constexpr std::uint32_t massifDemVersion = 1;

/// A DEM plus the provenance label the header carries.
struct DemDocument {
    Heightfield field;
    std::string label;
};

DemDocument readMassifDem(const std::filesystem::path& path);

void writeMassifDem(const std::filesystem::path& path, const Heightfield& field,
                    std::string_view label);

}  // namespace terrain::formats
