#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace terrain {

/**
 * The image textures a texture parameter can name, found by scanning
 * `<asset directory>/textures/*.jpg|png` in every asset directory, the same
 * directories the satmap catalogue reads. Names are file names, in name
 * order, and a name found in several directories is listed once.
 *
 * The engine only ever needs the names: they become the options of every
 * texture parameter. The images themselves are read by the viewport.
 */
class TextureCatalogue {
public:
    static TextureCatalogue scan(const std::vector<std::filesystem::path>& directories);

    /// The catalogue over the asset directories, scanned once per process.
    static const TextureCatalogue& shared();

    const std::vector<std::string>& names() const { return names_; }

private:
    std::vector<std::string> names_;
};

}  // namespace terrain
