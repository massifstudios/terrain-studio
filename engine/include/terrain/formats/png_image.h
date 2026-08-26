#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

namespace terrain::formats {

/**
 * Minimal PNG codec: non-interlaced grayscale, RGB and RGBA at 8 or 16 bits
 * per sample. Enough for heightfield interop, splat maps, colour maps and the
 * hillshade preview; palette and interlace are refused rather than guessed at.
 */
struct PngImage {
    int width = 0;
    int height = 0;
    int channels = 0;
    int bitDepth = 0;
    /// Channel-interleaved samples. 8-bit images keep their 0..255 range.
    std::vector<std::uint16_t> samples;
};

PngImage readPng(const std::filesystem::path& path);

void writePngGray16(const std::filesystem::path& path, int width, int height,
                    std::span<const std::uint16_t> samples);

void writePngRgb8(const std::filesystem::path& path, int width, int height,
                  std::span<const std::uint8_t> pixels);

void writePngRgba8(const std::filesystem::path& path, int width, int height,
                   std::span<const std::uint8_t> pixels);

}  // namespace terrain::formats
