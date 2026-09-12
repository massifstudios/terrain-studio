#pragma once

#include <cstddef>
#include <functional>

#include "terrain/registry.h"
#include "terrain_node_sdk.h"

namespace terrain::nodes {

/// Registration entry points, one per node category. Each goes through the
/// same C API a plugin uses.
void registerInputNodes(const TerrainRegistryApi& api, TerrainRegistry* registry);
void registerGenerateNodes(const TerrainRegistryApi& api, TerrainRegistry* registry);
void registerLandformNodes(const TerrainRegistryApi& api, TerrainRegistry* registry);
void registerFilterNodes(const TerrainRegistryApi& api, TerrainRegistry* registry);
void registerTransformNodes(const TerrainRegistryApi& api, TerrainRegistry* registry);
void registerAdjustNodes(const TerrainRegistryApi& api, TerrainRegistry* registry);
void registerErosionNodes(const TerrainRegistryApi& api, TerrainRegistry* registry);
void registerFluvialNodes(const TerrainRegistryApi& api, TerrainRegistry* registry);
void registerMaskNodes(const TerrainRegistryApi& api, TerrainRegistry* registry);
void registerCombineNodes(const TerrainRegistryApi& api, TerrainRegistry* registry);
void registerTextureNodes(const TerrainRegistryApi& api, TerrainRegistry* registry);
void registerSatMapNodes(const TerrainRegistryApi& api, TerrainRegistry* registry);
void registerColorNodes(const TerrainRegistryApi& api, TerrainRegistry* registry);
void registerVegetationNodes(const TerrainRegistryApi& api, TerrainRegistry* registry);
void registerOutputNodes(const TerrainRegistryApi& api, TerrainRegistry* registry);

inline std::size_t cellCount(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    return static_cast<std::size_t>(api->width(context))
           * static_cast<std::size_t>(api->height(context));
}

/// Scales a node's effect per cell: where the mask is 0 the original survives,
/// where it is 1 the filtered value does. An absent mask means full effect.
void blendThroughMask(const float* original, const float* filtered, const float* mask,
                      float* destination, std::size_t count);

/* --------------------------------------------------------- row splitting -- */

/**
 * Runs `body(firstRow, lastRow)` over the whole grid in parallel. The split is
 * a function of the row count alone, so a lane always covers the same rows no
 * matter how many threads the machine has.
 */
void forEachRowRange(const TerrainContextApi* api, TerrainEvaluationContext* context, int rowCount,
                     const std::function<void(int firstRow, int lastRow)>& body);

/* ------------------------------------------------------------ field maths -- */

struct FieldRange {
    float minimum = 0.0f;
    float maximum = 0.0f;

    float span() const;
    /// Where `value` sits in the range, as [0, 1]. A flat field maps to 0.
    float normalise(float value) const;
    float denormalise(float unit) const;
};

FieldRange fieldRange(const float* values, std::size_t count);

/// Ground gradient by central differences, in meters risen per meter walked.
/// Row 0 is north, so `northward` is positive when the ground rises northwards.
struct SurfaceGradient {
    float eastward = 0.0f;
    float northward = 0.0f;
};

SurfaceGradient sampleGradient(const float* field, int size, int row, int column,
                               double cellSizeMeters);

float slopeDegrees(const SurfaceGradient& gradient);

/// Positive over a rise, negative in a hollow: the discrete Laplacian scaled
/// by cell size, so the value means the same thing at any resolution.
float sampleCurvature(const float* field, int size, int row, int column, double cellSizeMeters);

/// 1 inside [low, high], 0 further than `falloff` outside it, smooth between.
float bandWeight(float value, float low, float high, float falloff);

/// Hermite smoothstep, the same easing `bandWeight` uses.
float smoothStep(float edge);

}  // namespace terrain::nodes
