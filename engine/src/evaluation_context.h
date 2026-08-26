#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "terrain/evaluator.h"
#include "terrain/heightfield.h"
#include "terrain_node_sdk.h"

namespace terrain {

/// Everything one node evaluation may reach, behind the C ABI's function table.
struct ContextState {
    int resolution = 0;
    double cellSizeMeters = 1.0;
    const GeoReference* georeference = nullptr;
    std::uint64_t nodeSeed = 0;

    std::vector<const float*> inputs;
    std::vector<std::shared_ptr<Heightfield>>* outputs = nullptr;

    std::vector<double> parameterNumbers;
    std::vector<std::string> parameterTexts;

    bool isFinalBuild = false;
    CancellationToken* cancellation = nullptr;
    std::function<void(float)> onProgress;

    bool hasError = false;
    std::string error;
};

const TerrainContextApi& terrainContextApi();

inline TerrainEvaluationContext* asEvaluationContext(ContextState& state) {
    return reinterpret_cast<TerrainEvaluationContext*>(&state);
}

}  // namespace terrain
