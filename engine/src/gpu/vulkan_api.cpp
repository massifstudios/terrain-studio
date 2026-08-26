#include "vulkan_api.h"

#include <dlfcn.h>

namespace terrain::gpu {
namespace {

/// The loader's own soname on each platform. The versioned name is tried
/// first because it is the one a driver package is guaranteed to install; the
/// unversioned symlink belongs to a development package that may be absent.
const char* const loaderLibraryNames[] = {
#if defined(__APPLE__)
    "libvulkan.1.dylib", "libMoltenVK.dylib", "libvulkan.dylib"
#elif defined(_WIN32)
    "vulkan-1.dll"
#else
    "libvulkan.so.1", "libvulkan.so"
#endif
};

void* openLoaderLibrary() {
    for (const char* name : loaderLibraryNames) {
        void* library = dlopen(name, RTLD_NOW | RTLD_LOCAL);
        if (library != nullptr) {
            return library;
        }
    }
    return nullptr;
}

bool resolveLoaderFunctions(VulkanApi& api) {
#define TERRAIN_VULKAN_RESOLVE(name) \
    api.name = reinterpret_cast<PFN_##name>(api.vkGetInstanceProcAddr(nullptr, #name));
    TERRAIN_VULKAN_LOADER_FUNCTIONS(TERRAIN_VULKAN_RESOLVE)
#undef TERRAIN_VULKAN_RESOLVE
    return api.vkCreateInstance != nullptr;
}

/// Built once, on first use, and never torn down: the loader outlives every
/// device and unloading it while a driver thread is still alive is unsafe.
const VulkanApi* buildVulkanApi() {
    void* library = openLoaderLibrary();
    if (library == nullptr) {
        return nullptr;
    }

    static VulkanApi api;
    api.vkGetInstanceProcAddr =
        reinterpret_cast<PFN_vkGetInstanceProcAddr>(dlsym(library, "vkGetInstanceProcAddr"));
    if (api.vkGetInstanceProcAddr == nullptr) {
        return nullptr;
    }
    if (!resolveLoaderFunctions(api)) {
        return nullptr;
    }
    return &api;
}

}  // namespace

const VulkanApi* loadVulkanApi() {
    static const VulkanApi* api = buildVulkanApi();
    return api;
}

void loadVulkanInstanceFunctions(VulkanApi& api, VkInstance instance) {
#define TERRAIN_VULKAN_RESOLVE(name) \
    api.name = reinterpret_cast<PFN_##name>(api.vkGetInstanceProcAddr(instance, #name));
    TERRAIN_VULKAN_INSTANCE_FUNCTIONS(TERRAIN_VULKAN_RESOLVE)
#undef TERRAIN_VULKAN_RESOLVE
}

void loadVulkanDeviceFunctions(const VulkanApi& api, VkDevice device, VulkanDeviceApi& deviceApi) {
#define TERRAIN_VULKAN_RESOLVE(name) \
    deviceApi.name = reinterpret_cast<PFN_##name>(api.vkGetDeviceProcAddr(device, #name));
    TERRAIN_VULKAN_DEVICE_FUNCTIONS(TERRAIN_VULKAN_RESOLVE)
#undef TERRAIN_VULKAN_RESOLVE
}

}  // namespace terrain::gpu
