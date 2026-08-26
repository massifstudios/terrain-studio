#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "terrain/registry.h"

namespace terrain {

/// What became of one shared library the loader looked at. `message` is empty
/// only when the plugin loaded cleanly and registered at least one node.
struct PluginReport {
    std::string path;
    std::string name;
    bool loaded = false;
    std::uint32_t abiVersion = 0;
    int nodeCount = 0;
    std::string message;
};

/**
 * Loads node plugins from directories.
 *
 * A loaded library is never unloaded. The registry holds function pointers
 * into it for as long as the process lives, and there is no moment at which
 * closing one would be safe. A rescan is therefore additive: it picks up
 * libraries that have appeared, and leaves everything already loaded alone.
 */
class PluginHost {
public:
    /// Every shared library directly in `directory`, in file-name order.
    /// Subdirectories are ignored, which is what lets a plugin's own source
    /// live beside its built form. A directory that does not exist is not an
    /// error — an unused plugin folder is the normal case.
    std::vector<PluginReport> loadDirectory(const std::filesystem::path& directory,
                                            NodeRegistry& registry);

    const std::vector<PluginReport>& reports() const { return reports_; }

private:
    PluginReport load(const std::filesystem::path& path, NodeRegistry& registry);
    bool isAlreadyLoaded(const std::filesystem::path& path) const;

    std::vector<PluginReport> reports_;
};

}  // namespace terrain
