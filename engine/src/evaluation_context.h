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

    /// One entry per declared input port, null where nothing is connected.
    std::vector<const NodeOutput*> inputs;
    std::vector<NodeOutput>* outputs = nullptr;

    std::vector<double> parameterNumbers;
    std::vector<std::string> parameterTexts;

    bool isFinalBuild = false;
    /// How many threads this node may spend on its own lanes. Affects speed
    /// only: lane boundaries come from the node, never from this number.
    int threadBudget = 1;
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
