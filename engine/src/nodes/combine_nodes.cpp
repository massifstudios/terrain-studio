#include <algorithm>

#include "node_support.h"

namespace terrain::nodes {
namespace {

const TerrainPortDescriptor pairInputs[] = {
    {"A", "First field", TerrainPortType_Field, TerrainFieldSemantics_Heightfield, 0},
    {"B", "Second field", TerrainPortType_Field, TerrainFieldSemantics_Heightfield, 0}};

const TerrainPortDescriptor pairOutputs[] = {
    {"Out", "The combined field", TerrainPortType_Field, TerrainFieldSemantics_Heightfield, 0}};

/// Every cell-by-cell combiner is the same loop around a different operation,
/// so the operation is all each node below actually supplies.
template <float (*Operation)(float, float)>
void evaluatePair(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const float* first = api->inputField(context, 0);
    const float* second = api->inputField(context, 1);
    float* output = api->outputField(context, 0);
    const std::size_t count = cellCount(api, context);

    for (std::size_t index = 0; index < count; ++index) {
        output[index] = Operation(first[index], second[index]);
    }
}

float addValues(float first, float second) {
    return first + second;
}

float subtractValues(float first, float second) {
    return first - second;
}

float multiplyValues(float first, float second) {
    return first * second;
}

float lowerValue(float first, float second) {
    return std::min(first, second);
}

float higherValue(float first, float second) {
    return std::max(first, second);
}

TerrainNodeDescriptor pairDescriptor(const char* typeName, const char* description,
                                     TerrainNodeEvaluate evaluate) {
    return TerrainNodeDescriptor{typeName, "Combine", description, 1,       pairInputs, 2,
                                 pairOutputs, 1,       nullptr,     0,      evaluate};
}

/* ----------------------------------------------------------------- Lerp -- */

const TerrainPortDescriptor lerpInputs[] = {
    {"A", "The field at an amount of 0", TerrainPortType_Field,
     TerrainFieldSemantics_Heightfield, 0},
    {"B", "The field at an amount of 1", TerrainPortType_Field,
     TerrainFieldSemantics_Heightfield, 0},
    {"Mask", "When connected, this replaces the amount, cell by cell",
     TerrainPortType_Field, TerrainFieldSemantics_Mask, 1}};

const TerrainParameterDescriptor lerpParameters[] = {
    {"amount", "Amount", "How far from A towards B, when no mask is connected", "",
     TerrainParameterType_Float, 0.5, 0.0, 1.0, "", nullptr, 0}};

void evaluateLerp(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const float* first = api->inputField(context, 0);
    const float* second = api->inputField(context, 1);
    const float* mask = api->inputField(context, 2);
    float* output = api->outputField(context, 0);
    const std::size_t count = cellCount(api, context);
    const float amount = static_cast<float>(api->parameterNumber(context, 0));

    for (std::size_t index = 0; index < count; ++index) {
        float weight = amount;
        if (mask != nullptr) {
            weight = std::clamp(mask[index], 0.0f, 1.0f);
        }
        output[index] = first[index] + (second[index] - first[index]) * weight;
    }
}

const TerrainNodeDescriptor lerpDescriptor = {
    "Lerp",
    "Combine",
    "Blends between two fields, evenly or through a mask",
    1,
    lerpInputs,
    3,
    pairOutputs,
    1,
    lerpParameters,
    1,
    &evaluateLerp};

const TerrainNodeDescriptor addDescriptor =
    pairDescriptor("Add", "Adds two fields together", &evaluatePair<&addValues>);
const TerrainNodeDescriptor subtractDescriptor =
    pairDescriptor("Subtract", "Takes B away from A", &evaluatePair<&subtractValues>);
const TerrainNodeDescriptor multiplyDescriptor = pairDescriptor(
    "Multiply", "Multiplies two fields, the usual way to apply a mask",
    &evaluatePair<&multiplyValues>);
const TerrainNodeDescriptor minimumDescriptor =
    pairDescriptor("Min", "Keeps whichever field is lower in each cell",
                   &evaluatePair<&lowerValue>);
const TerrainNodeDescriptor maximumDescriptor =
    pairDescriptor("Max", "Keeps whichever field is higher in each cell",
                   &evaluatePair<&higherValue>);

}  // namespace

void registerCombineNodes(const TerrainRegistryApi& api, TerrainRegistry* registry) {
    api.registerNode(registry, &addDescriptor);
    api.registerNode(registry, &subtractDescriptor);
    api.registerNode(registry, &multiplyDescriptor);
    api.registerNode(registry, &minimumDescriptor);
    api.registerNode(registry, &maximumDescriptor);
    api.registerNode(registry, &lerpDescriptor);
}

}  // namespace terrain::nodes
