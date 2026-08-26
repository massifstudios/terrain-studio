#include "node_support.h"
#include "terrain/registry.h"

namespace terrain {

void registerBuiltinNodes(NodeRegistry& registry) {
    // Built-ins go through the plugin C entry point, so the ABI is exercised
    // by the engine itself and cannot rot unnoticed.
    const TerrainRegistryApi api = registry.api();
    TerrainRegistry* handle = registry.handle();

    nodes::registerInputNodes(api, handle);
    nodes::registerGenerateNodes(api, handle);
    nodes::registerFilterNodes(api, handle);
    nodes::registerTransformNodes(api, handle);
    nodes::registerAdjustNodes(api, handle);
    nodes::registerErosionNodes(api, handle);
    nodes::registerMaskNodes(api, handle);
    nodes::registerCombineNodes(api, handle);
    nodes::registerTextureNodes(api, handle);
    nodes::registerVegetationNodes(api, handle);
    nodes::registerOutputNodes(api, handle);
}

}  // namespace terrain
