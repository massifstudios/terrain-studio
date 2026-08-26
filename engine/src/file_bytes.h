#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace terrain::io {

/// Reads a whole file into memory. Every format here is small enough for that
/// to be the right trade: a 10240² DEM is 200 MB, and the engine holds the
/// decoded grid anyway.
inline std::vector<std::uint8_t> readAllBytes(const std::filesystem::path& path,
                                              std::string_view what) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Cannot open " + std::string(what) + ": " + path.string());
    }
    std::vector<std::uint8_t> bytes;
    bytes.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    return bytes;
}

inline void writeAllBytes(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes,
                          std::string_view what) {
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Cannot write " + std::string(what) + ": " + path.string());
    }
    file.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
}

}  // namespace terrain::io
