#include "evaluation_context.h"

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

const float* contextInputField(TerrainEvaluationContext* context, int32_t portIndex) {
    const ContextState& state = stateOf(context);
    if (portIndex < 0 || portIndex >= static_cast<int32_t>(state.inputs.size())) {
        return nullptr;
    }
    return state.inputs[static_cast<std::size_t>(portIndex)];
}

int32_t contextIsInputConnected(TerrainEvaluationContext* context, int32_t portIndex) {
    if (contextInputField(context, portIndex) == nullptr) {
        return 0;
    }
    return 1;
}

float* contextOutputField(TerrainEvaluationContext* context, int32_t portIndex) {
    ContextState& state = stateOf(context);
    if (state.outputs == nullptr || portIndex < 0
        || portIndex >= static_cast<int32_t>(state.outputs->size())) {
        return nullptr;
    }
    return (*state.outputs)[static_cast<std::size_t>(portIndex)]->data();
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
                               contextSetError};

}  // namespace

const TerrainContextApi& terrainContextApi() {
    return api;
}

}  // namespace terrain
