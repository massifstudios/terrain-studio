#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "terrain/color_strip.h"

namespace terrain {

/// One family of strips: the name of the directory they were found in, and
/// the files inside it in name order.
struct SatMapLibrary {
    std::string name;
    std::vector<std::filesystem::path> strips;
};

/**
 * The satellite colour strips the engine can reach, found by scanning
 * `<asset directory>/satmaps/<library>/*.png` in every asset directory.
 *
 * The asset directories come from `TERRAIN_ASSET_PATH`, split on the
 * platform's path delimiter, and fall back to the source tree's own
 * `engine/assets` so a development build and the test suite find the strips
 * with nothing configured. The process that starts the engine decides the
 * directories, exactly as it does for plugins.
 *
 * Strips are read the first time they are asked for and kept for the life of
 * the catalogue. Loading is under a lock because several nodes can evaluate
 * at once, and the loaded strip is immutable and shared.
 */
class SatMapCatalogue {
public:
    SatMapCatalogue();

    static std::vector<std::filesystem::path> assetDirectories();

    /// Scans the given asset directories. Libraries with the same name in
    /// several directories are merged, in directory order.
    static SatMapCatalogue scan(const std::vector<std::filesystem::path>& directories);

    /// The catalogue over `assetDirectories()`, scanned once per process.
    static const SatMapCatalogue& shared();

    const std::vector<SatMapLibrary>& libraries() const { return libraries_; }

    /// The strip at `item` of `library`, counting round past the last one.
    /// Null when the library index is out of range or the library is empty,
    /// or when the file could not be read.
    std::shared_ptr<const ColorStrip> strip(int library, int item) const;

    /// A strip by file, for one an artist brought in themselves. Null when
    /// the file cannot be read.
    std::shared_ptr<const ColorStrip> stripAt(const std::filesystem::path& path) const;

private:
    struct StripCache;

    std::vector<SatMapLibrary> libraries_;
    /// Behind a pointer so the catalogue stays movable around its lock.
    std::shared_ptr<StripCache> cache_;
};

}  // namespace terrain
