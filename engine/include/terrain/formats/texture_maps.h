#pragma once

#include <array>
#include <filesystem>

#include "terrain/color_field.h"
#include "terrain/heightfield.h"

namespace terrain::formats {

/**
 * Up to four masks packed into the channels of an 8-bit RGBA PNG, the layout
 * Unreal and Unity landscape layers expect. A missing layer writes as zero.
 */
void writeSplatPng(const std::filesystem::path& path,
                   const std::array<const Heightfield*, 4>& layers);

/// An 8-bit RGB PNG. Channels are written exactly as the graph holds them; no
/// transfer function is applied on the way out.
void writeColorPng(const std::filesystem::path& path, const ColorField& colors);

}  // namespace terrain::formats
