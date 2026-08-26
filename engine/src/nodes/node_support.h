#pragma once

#include <cstddef>

#include "terrain/registry.h"
#include "terrain_node_sdk.h"

namespace terrain::nodes {

/// Registration entry points, one per node category. Each goes through the
/// same C API a plugin uses.
void registerInputNodes(const TerrainRegistryApi& api, TerrainRegistry* registry);
void registerFilterNodes(const TerrainRegistryApi& api, TerrainRegistry* registry);
void registerCombineNodes(const TerrainRegistryApi& api, TerrainRegistry* registry);
void registerOutputNodes(const TerrainRegistryApi& api, TerrainRegistry* registry);

inline std::size_t cellCount(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    return static_cast<std::size_t>(api->width(context))
           * static_cast<std::size_t>(api->height(context));
}

/// Scales a node's effect per cell: where the mask is 0 the original survives,
/// where it is 1 the filtered value does. An absent mask means full effect.
void blendThroughMask(const float* original, const float* filtered, const float* mask,
                      float* destination, std::size_t count);

}  // namespace terrain::nodes
