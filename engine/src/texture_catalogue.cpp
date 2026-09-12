#include "terrain/texture_catalogue.h"

#include <algorithm>
#include <cctype>
#include <set>

#include "terrain/satmap_catalogue.h"

namespace terrain {
namespace {

bool isTextureFile(const std::filesystem::directory_entry& entry) {
    if (!entry.is_regular_file()) {
        return false;
    }
    std::string extension = entry.path().extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char character) { return std::tolower(character); });
    return extension == ".jpg" || extension == ".jpeg" || extension == ".png";
}

void collectTextureNames(const std::filesystem::path& directory, std::set<std::string>& names) {
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(directory / "textures", error)) {
        if (isTextureFile(entry)) {
            names.insert(entry.path().filename().string());
        }
    }
}

}  // namespace

TextureCatalogue TextureCatalogue::scan(const std::vector<std::filesystem::path>& directories) {
    std::set<std::string> names;
    for (const std::filesystem::path& directory : directories) {
        collectTextureNames(directory, names);
    }
    TextureCatalogue catalogue;
    catalogue.names_.assign(names.begin(), names.end());
    return catalogue;
}

const TextureCatalogue& TextureCatalogue::shared() {
    static const TextureCatalogue catalogue = scan(SatMapCatalogue::assetDirectories());
    return catalogue;
}

}  // namespace terrain
