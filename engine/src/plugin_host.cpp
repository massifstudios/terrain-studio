#include "terrain/plugin_host.h"

#include <algorithm>
#include <system_error>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace terrain {
namespace {

#if defined(_WIN32)

constexpr const char* libraryExtension = ".dll";

void* openLibrary(const std::filesystem::path& path) {
    return reinterpret_cast<void*>(LoadLibraryW(path.c_str()));
}

void* findSymbol(void* handle, const char* name) {
    return reinterpret_cast<void*>(GetProcAddress(reinterpret_cast<HMODULE>(handle), name));
}

std::string openFailure() {
    return "the operating system refused to load it";
}

#elif defined(__APPLE__)

constexpr const char* libraryExtension = ".dylib";

void* openLibrary(const std::filesystem::path& path) {
    return dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
}

void* findSymbol(void* handle, const char* name) {
    return dlsym(handle, name);
}

std::string openFailure() {
    const char* reported = dlerror();
    if (reported == nullptr) {
        return "the operating system refused to load it";
    }
    return reported;
}

#else

constexpr const char* libraryExtension = ".so";

void* openLibrary(const std::filesystem::path& path) {
    return dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
}

void* findSymbol(void* handle, const char* name) {
    return dlsym(handle, name);
}

std::string openFailure() {
    const char* reported = dlerror();
    if (reported == nullptr) {
        return "the operating system refused to load it";
    }
    return reported;
}

#endif

/// Sorted, so a rescan reports the same libraries in the same order however
/// the file system happened to hand them over.
std::vector<std::filesystem::path> libraryFilesIn(const std::filesystem::path& directory) {
    std::error_code failure;
    std::filesystem::directory_iterator entries(directory, failure);
    if (failure) {
        return {};
    }

    std::vector<std::filesystem::path> found;
    for (const std::filesystem::directory_entry& entry : entries) {
        if (entry.is_regular_file() && entry.path().extension() == libraryExtension) {
            found.push_back(entry.path());
        }
    }
    std::sort(found.begin(), found.end());
    return found;
}

std::string versionComplaint(std::uint32_t reported) {
    return "was built against plugin ABI version " + std::to_string(reported) +
           "; this engine speaks version " + std::to_string(TERRAIN_SDK_ABI_VERSION);
}

struct EntryPoints {
    TerrainPluginAbiVersionFn abiVersion = nullptr;
    TerrainRegisterPluginFn registerPlugin = nullptr;
};

EntryPoints entryPointsOf(void* handle) {
    EntryPoints found;
    found.abiVersion =
        reinterpret_cast<TerrainPluginAbiVersionFn>(findSymbol(handle, "terrainPluginAbiVersion"));
    found.registerPlugin =
        reinterpret_cast<TerrainRegisterPluginFn>(findSymbol(handle, "terrainRegisterPlugin"));
    return found;
}

/// Fills in everything past opening the library: which nodes arrived, or why
/// none did. Refusal is always a message, never a crash and never silence.
void registerFrom(void* handle, NodeRegistry& registry, PluginReport& report) {
    const EntryPoints entries = entryPointsOf(handle);
    if (entries.abiVersion == nullptr || entries.registerPlugin == nullptr) {
        report.message = "does not export terrainPluginAbiVersion and terrainRegisterPlugin";
        return;
    }

    report.abiVersion = entries.abiVersion();
    if (report.abiVersion != TERRAIN_SDK_ABI_VERSION) {
        report.message = versionComplaint(report.abiVersion);
        return;
    }

    const std::size_t before = registry.size();
    const TerrainRegistryApi api = registry.api();
    if (entries.registerPlugin(&api, registry.handle()) == 0) {
        report.message = "refused to register its nodes";
        return;
    }

    report.loaded = true;
    report.nodeCount = static_cast<int>(registry.size() - before);
    if (report.nodeCount == 0) {
        report.message = "registered no nodes; its type names may already be taken";
    }
}

}  // namespace

bool PluginHost::isAlreadyLoaded(const std::filesystem::path& path) const {
    const std::string text = path.string();
    return std::any_of(reports_.begin(), reports_.end(),
                       [&text](const PluginReport& report) { return report.path == text; });
}

PluginReport PluginHost::load(const std::filesystem::path& path, NodeRegistry& registry) {
    PluginReport report;
    report.path = path.string();
    report.name = path.stem().string();

    void* handle = openLibrary(path);
    if (handle == nullptr) {
        report.message = "could not be opened: " + openFailure();
        return report;
    }

    registerFrom(handle, registry, report);
    return report;
}

std::vector<PluginReport> PluginHost::loadDirectory(const std::filesystem::path& directory,
                                                    NodeRegistry& registry) {
    std::vector<PluginReport> fresh;
    for (const std::filesystem::path& path : libraryFilesIn(directory)) {
        if (isAlreadyLoaded(path)) {
            continue;
        }
        fresh.push_back(load(path, registry));
    }
    reports_.insert(reports_.end(), fresh.begin(), fresh.end());
    return fresh;
}

}  // namespace terrain
