#pragma once

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace terrain::testing {

inline std::filesystem::path dataDirectory() {
    return std::filesystem::path(TERRAIN_TEST_DATA_DIR);
}

inline std::vector<std::uint8_t> readAllBytes(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(file),
                                     std::istreambuf_iterator<char>());
}

/// A scratch file that removes itself, so a failing test leaves nothing behind.
class ScratchFile {
public:
    explicit ScratchFile(std::string name)
        : path_(std::filesystem::temp_directory_path() / ("terrain_studio_test_" + name)) {
        std::filesystem::remove(path_);
    }

    ~ScratchFile() { std::filesystem::remove(path_); }

    ScratchFile(const ScratchFile&) = delete;
    ScratchFile& operator=(const ScratchFile&) = delete;

    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

}  // namespace terrain::testing
