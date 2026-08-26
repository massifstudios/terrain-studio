/*
 * A plugin from a future the engine does not know. It exists so the suite can
 * prove that an ABI mismatch is refused with a message rather than crashing or
 * being skipped in silence, and so it registers nothing at all.
 */

#include "terrain_node_sdk.h"

#if defined(_WIN32)
#define TERRAIN_PLUGIN_EXPORT __declspec(dllexport)
#else
#define TERRAIN_PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

TERRAIN_PLUGIN_EXPORT uint32_t terrainPluginAbiVersion(void) {
    return TERRAIN_SDK_ABI_VERSION + 1;
}

TERRAIN_PLUGIN_EXPORT int32_t terrainRegisterPlugin(const TerrainRegistryApi* api,
                                                    TerrainRegistry* registry) {
    (void)api;
    (void)registry;
    return 0;
}
