#include "evaluation_context.h"

#include <algorithm>
#include <atomic>
#include <thread>

namespace terrain {
namespace {

ContextState& stateOf(TerrainEvaluationContext* context) {
    return *reinterpret_cast<ContextState*>(context);
}

int32_t contextWidth(TerrainEvaluationContext* context) {
    return stateOf(context).resolution;
}

int32_t contextHeight(TerrainEvaluationContext* context) {
    return stateOf(context).resolution;
}

double contextCellSize(TerrainEvaluationContext* context) {
    return stateOf(context).cellSizeMeters;
}

const char* contextCoordinateSystem(TerrainEvaluationContext* context) {
    const GeoReference* reference = stateOf(context).georeference;
    if (reference == nullptr) {
        return "";
    }
    return reference->coordinateSystem.c_str();
}

double contextEastingMin(TerrainEvaluationContext* context) {
    const GeoReference* reference = stateOf(context).georeference;
    if (reference == nullptr) {
        return 0.0;
    }
    return reference->eastingMinMeters;
}

double contextNorthingMax(TerrainEvaluationContext* context) {
    const GeoReference* reference = stateOf(context).georeference;
    if (reference == nullptr) {
        return 0.0;
    }
    return reference->northingMaxMeters;
}

const NodeOutput* inputAt(TerrainEvaluationContext* context, int32_t portIndex) {
    const ContextState& state = stateOf(context);
    if (portIndex < 0 || portIndex >= static_cast<int32_t>(state.inputs.size())) {
        return nullptr;
    }
    return state.inputs[static_cast<std::size_t>(portIndex)];
}

NodeOutput* outputAt(TerrainEvaluationContext* context, int32_t portIndex) {
    ContextState& state = stateOf(context);
    if (state.outputs == nullptr || portIndex < 0
        || portIndex >= static_cast<int32_t>(state.outputs->size())) {
        return nullptr;
    }
    return &(*state.outputs)[static_cast<std::size_t>(portIndex)];
}

const float* contextInputField(TerrainEvaluationContext* context, int32_t portIndex) {
    const NodeOutput* input = inputAt(context, portIndex);
    if (input == nullptr || input->field == nullptr) {
        return nullptr;
    }
    return input->field->data();
}

int32_t contextIsInputConnected(TerrainEvaluationContext* context, int32_t portIndex) {
    if (inputAt(context, portIndex) == nullptr) {
        return 0;
    }
    return 1;
}

float* contextOutputField(TerrainEvaluationContext* context, int32_t portIndex) {
    NodeOutput* output = outputAt(context, portIndex);
    if (output == nullptr || output->field == nullptr) {
        return nullptr;
    }
    return output->field->data();
}

const float* contextInputColor(TerrainEvaluationContext* context, int32_t portIndex) {
    const NodeOutput* input = inputAt(context, portIndex);
    if (input == nullptr || input->color == nullptr) {
        return nullptr;
    }
    return input->color->data();
}

float* contextOutputColor(TerrainEvaluationContext* context, int32_t portIndex) {
    NodeOutput* output = outputAt(context, portIndex);
    if (output == nullptr || output->color == nullptr) {
        return nullptr;
    }
    return output->color->data();
}

int32_t contextInputPointCount(TerrainEvaluationContext* context, int32_t portIndex) {
    const NodeOutput* input = inputAt(context, portIndex);
    if (input == nullptr || input->points == nullptr) {
        return 0;
    }
    return static_cast<int32_t>(input->points->size());
}

const TerrainPoint* contextInputPoints(TerrainEvaluationContext* context, int32_t portIndex) {
    const NodeOutput* input = inputAt(context, portIndex);
    if (input == nullptr || input->points == nullptr || input->points->isEmpty()) {
        return nullptr;
    }
    return input->points->points().data();
}

void contextEmitPoint(TerrainEvaluationContext* context, int32_t portIndex,
                      const TerrainPoint* point) {
    NodeOutput* output = outputAt(context, portIndex);
    if (output == nullptr || output->points == nullptr || point == nullptr) {
        return;
    }
    output->points->add(*point);
}

double contextParameterNumber(TerrainEvaluationContext* context, int32_t parameterIndex) {
    const ContextState& state = stateOf(context);
    if (parameterIndex < 0
        || parameterIndex >= static_cast<int32_t>(state.parameterNumbers.size())) {
        return 0.0;
    }
    return state.parameterNumbers[static_cast<std::size_t>(parameterIndex)];
}

const char* contextParameterText(TerrainEvaluationContext* context, int32_t parameterIndex) {
    const ContextState& state = stateOf(context);
    if (parameterIndex < 0
        || parameterIndex >= static_cast<int32_t>(state.parameterTexts.size())) {
        return "";
    }
    return state.parameterTexts[static_cast<std::size_t>(parameterIndex)].c_str();
}

std::uint64_t contextRandomSeed(TerrainEvaluationContext* context) {
    return stateOf(context).nodeSeed;
}

void contextReportProgress(TerrainEvaluationContext* context, float fraction) {
    const ContextState& state = stateOf(context);
    if (state.onProgress) {
        state.onProgress(fraction);
    }
}

int32_t contextIsCancelled(TerrainEvaluationContext* context) {
    const CancellationToken* token = stateOf(context).cancellation;
    if (token != nullptr && token->isCancelled()) {
        return 1;
    }
    return 0;
}

int32_t contextIsFinalBuild(TerrainEvaluationContext* context) {
    if (stateOf(context).isFinalBuild) {
        return 1;
    }
    return 0;
}

void contextRunLanes(TerrainEvaluationContext* context, int32_t laneCount, TerrainLaneTask task,
                     void* userData) {
    if (task == nullptr || laneCount <= 0) {
        return;
    }
    const int workers = std::min(stateOf(context).threadBudget, laneCount);
    if (workers <= 1) {
        for (int32_t lane = 0; lane < laneCount; ++lane) {
            task(userData, lane, laneCount);
        }
        return;
    }

    std::atomic<int32_t> nextLane{0};
    std::vector<std::jthread> threads;
    threads.reserve(static_cast<std::size_t>(workers));
    for (int worker = 0; worker < workers; ++worker) {
        threads.emplace_back([&nextLane, laneCount, task, userData] {
            for (int32_t lane = nextLane.fetch_add(1); lane < laneCount;
                 lane = nextLane.fetch_add(1)) {
                task(userData, lane, laneCount);
            }
        });
    }
}

void contextSetError(TerrainEvaluationContext* context, const char* message) {
    ContextState& state = stateOf(context);
    state.hasError = true;
    if (message != nullptr) {
        state.error = message;
    }
}

const TerrainContextApi api = {contextWidth,
                               contextHeight,
                               contextCellSize,
                               contextCoordinateSystem,
                               contextEastingMin,
                               contextNorthingMax,
                               contextIsInputConnected,
                               contextInputField,
                               contextOutputField,
                               contextParameterNumber,
                               contextParameterText,
                               contextRandomSeed,
                               contextReportProgress,
                               contextIsCancelled,
                               contextIsFinalBuild,
                               contextSetError,
                               contextRunLanes,
                               contextInputColor,
                               contextOutputColor,
                               contextInputPointCount,
                               contextInputPoints,
                               contextEmitPoint};

}  // namespace

const TerrainContextApi& terrainContextApi() {
    return api;
}

}  // namespace terrain
