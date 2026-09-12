#include "terrain/satmap_catalogue.h"

#include <algorithm>
#include <cstdlib>
#include <exception>
#include <map>
#include <mutex>
#include <string_view>

namespace terrain {
namespace {

#ifdef _WIN32
constexpr char pathDelimiter = ';';
#else
constexpr char pathDelimiter = ':';
#endif

std::vector<std::filesystem::path> splitPathList(std::string_view text) {
    std::vector<std::filesystem::path> directories;
    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t end = std::min(text.find(pathDelimiter, start), text.size());
        if (end > start) {
            directories.emplace_back(text.substr(start, end - start));
        }
        start = end + 1;
    }
    return directories;
}

bool isStripFile(const std::filesystem::directory_entry& entry) {
    if (!entry.is_regular_file()) {
        return false;
    }
    std::string extension = entry.path().extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char character) { return std::tolower(character); });
    return extension == ".png";
}

std::vector<std::filesystem::path> stripsIn(const std::filesystem::path& libraryDirectory) {
    std::vector<std::filesystem::path> strips;
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(libraryDirectory, error)) {
        if (isStripFile(entry)) {
            strips.push_back(entry.path());
        }
    }
    std::sort(strips.begin(), strips.end());
    return strips;
}

SatMapLibrary& libraryNamed(std::vector<SatMapLibrary>& libraries, const std::string& name) {
    for (SatMapLibrary& library : libraries) {
        if (library.name == name) {
            return library;
        }
    }
    libraries.push_back(SatMapLibrary{name, {}});
    return libraries.back();
}

void scanAssetDirectory(const std::filesystem::path& directory,
                        std::vector<SatMapLibrary>& libraries) {
    std::vector<std::filesystem::path> libraryDirectories;
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(directory / "satmaps", error)) {
        if (entry.is_directory()) {
            libraryDirectories.push_back(entry.path());
        }
    }
    std::sort(libraryDirectories.begin(), libraryDirectories.end());

    for (const std::filesystem::path& libraryDirectory : libraryDirectories) {
        std::vector<std::filesystem::path> strips = stripsIn(libraryDirectory);
        if (strips.empty()) {
            continue;
        }
        SatMapLibrary& library = libraryNamed(libraries, libraryDirectory.filename().string());
        library.strips.insert(library.strips.end(), strips.begin(), strips.end());
    }
}

}  // namespace

struct SatMapCatalogue::StripCache {
    std::mutex lock;
    std::map<std::filesystem::path, std::shared_ptr<const ColorStrip>> loaded;
};

SatMapCatalogue::SatMapCatalogue() : cache_(std::make_shared<StripCache>()) {}

std::vector<std::filesystem::path> SatMapCatalogue::assetDirectories() {
    const char* configured = std::getenv("TERRAIN_ASSET_PATH");
    if (configured != nullptr && configured[0] != '\0') {
        return splitPathList(configured);
    }
#ifdef TERRAIN_SOURCE_ASSET_DIRECTORY
    return {std::filesystem::path(TERRAIN_SOURCE_ASSET_DIRECTORY)};
#else
    return {};
#endif
}

SatMapCatalogue SatMapCatalogue::scan(const std::vector<std::filesystem::path>& directories) {
    SatMapCatalogue catalogue;
    for (const std::filesystem::path& directory : directories) {
        scanAssetDirectory(directory, catalogue.libraries_);
    }
    return catalogue;
}

const SatMapCatalogue& SatMapCatalogue::shared() {
    static const SatMapCatalogue catalogue = scan(assetDirectories());
    return catalogue;
}

std::shared_ptr<const ColorStrip> SatMapCatalogue::strip(int library, int item) const {
    if (library < 0 || library >= static_cast<int>(libraries_.size())) {
        return nullptr;
    }
    const std::vector<std::filesystem::path>& strips = libraries_[library].strips;
    if (strips.empty()) {
        return nullptr;
    }
    const int wrapped = std::max(item, 0) % static_cast<int>(strips.size());
    return stripAt(strips[wrapped]);
}

std::shared_ptr<const ColorStrip> SatMapCatalogue::stripAt(
    const std::filesystem::path& path) const {
    const std::lock_guard<std::mutex> guard(cache_->lock);
    const auto found = cache_->loaded.find(path);
    if (found != cache_->loaded.end()) {
        return found->second;
    }
    std::shared_ptr<const ColorStrip> strip;
    try {
        strip = std::make_shared<const ColorStrip>(ColorStrip::readPng(path));
    } catch (const std::exception&) {
        strip = nullptr;
    }
    cache_->loaded[path] = strip;
    return strip;
}

}  // namespace terrain
