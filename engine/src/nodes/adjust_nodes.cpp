#include <algorithm>
#include <cmath>
#include <vector>

#include "node_support.h"
#include "terrain/histogram.h"

namespace terrain::nodes {
namespace {

const TerrainPortDescriptor adjustInputs[] = {
    {"In", "The field to rescale", TerrainPortType_Field, TerrainFieldSemantics_Heightfield, 0},
    {"Mask", "Scales the effect per cell", TerrainPortType_Field, TerrainFieldSemantics_Mask, 1}};

const TerrainPortDescriptor adjustOutputs[] = {
    {"Out", "The rescaled field", TerrainPortType_Field, TerrainFieldSemantics_Heightfield, 0}};

/// Applies a node's own strength dial before the mask does its work, so the
/// two compose the way an artist expects: amount sets how far, mask sets where.
void blendByAmount(const float* original, float* adjusted, float amount, std::size_t count) {
    const float strength = std::clamp(amount, 0.0f, 1.0f);
    for (std::size_t index = 0; index < count; ++index) {
        adjusted[index] = original[index] + (adjusted[index] - original[index]) * strength;
    }
}

/* ------------------------------------------------------------ Autolevel -- */

const TerrainParameterDescriptor autolevelParameters[] = {
    {"lowPercentile", "Low percentile", "The share of the field pushed down onto the floor", "%",
     TerrainParameterType_Float, 0.0, 0.0, 49.0, "", nullptr, 0},
    {"highPercentile", "High percentile", "The share of the field pulled up onto the ceiling", "%",
     TerrainParameterType_Float, 100.0, 51.0, 100.0, "", nullptr, 0},
    {"targetLow", "Floor", "Where the low percentile ends up", "m", TerrainParameterType_Float,
     0.0, -12000.0, 12000.0, "", nullptr, 0},
    {"targetHigh", "Ceiling", "Where the high percentile ends up", "m",
     TerrainParameterType_Float, 1000.0, -12000.0, 12000.0, "", nullptr, 0}};

/**
 * Stretches a field onto an explicit floor and ceiling, ignoring the tails the
 * percentiles cut off. Reading the ends from percentiles rather than from the
 * extremes stops one stray cell — a spike left by an import, say — from
 * deciding the scale of everything else.
 */
void evaluateAutolevel(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const std::size_t count = cellCount(api, context);
    const float* input = api->inputField(context, 0);
    const Histogram histogram(input, count);
    const float low =
        histogram.valueAtQuantile(static_cast<float>(api->parameterNumber(context, 0)) * 0.01f);
    const float high =
        histogram.valueAtQuantile(static_cast<float>(api->parameterNumber(context, 1)) * 0.01f);
    const float targetLow = static_cast<float>(api->parameterNumber(context, 2));
    const float targetHigh = static_cast<float>(api->parameterNumber(context, 3));

    std::vector<float> levelled(count, targetLow);
    if (high > low) {
        for (std::size_t index = 0; index < count; ++index) {
            const float unit = std::clamp((input[index] - low) / (high - low), 0.0f, 1.0f);
            levelled[index] = targetLow + (targetHigh - targetLow) * unit;
        }
    }
    blendThroughMask(input, levelled.data(), api->inputField(context, 1),
                     api->outputField(context, 0), count);
}

const TerrainNodeDescriptor autolevelDescriptor = {
    "Autolevel",
    "Filter",
    "Stretches a field onto a chosen floor and ceiling, ignoring its tails",
    1,
    adjustInputs,
    2,
    adjustOutputs,
    1,
    autolevelParameters,
    4,
    &evaluateAutolevel};

/* ------------------------------------------------------------- Equalize -- */

const TerrainParameterDescriptor equalizeParameters[] = {
    {"amount", "Amount", "How far towards an even spread of heights the field is taken", "",
     TerrainParameterType_Float, 1.0, 0.0, 1.0, "", nullptr, 0}};

/**
 * Spreads the field's heights evenly across its own range, which pulls detail
 * out of whichever band the cells were crowded into. The range itself does not
 * move, so a field in meters stays in the meters it was in.
 */
void evaluateEqualize(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const std::size_t count = cellCount(api, context);
    const float* input = api->inputField(context, 0);
    const Histogram histogram(input, count);
    const float amount = static_cast<float>(api->parameterNumber(context, 0));

    std::vector<float> spread(count);
    const float span = histogram.maximum() - histogram.minimum();
    for (std::size_t index = 0; index < count; ++index) {
        spread[index] = histogram.minimum() + histogram.quantileOfValue(input[index]) * span;
    }
    blendByAmount(input, spread.data(), amount, count);
    blendThroughMask(input, spread.data(), api->inputField(context, 1),
                     api->outputField(context, 0), count);
}

const TerrainNodeDescriptor equalizeDescriptor = {
    "Equalize",
    "Filter",
    "Spreads a field's heights evenly across its own range",
    1,
    adjustInputs,
    2,
    adjustOutputs,
    1,
    equalizeParameters,
    1,
    &evaluateEqualize};

/* ---------------------------------------------------------------- Match -- */

const TerrainPortDescriptor matchInputs[] = {
    {"In", "The field to reshape", TerrainPortType_Field, TerrainFieldSemantics_Heightfield, 0},
    {"Reference", "The field whose spread of heights is copied", TerrainPortType_Field,
     TerrainFieldSemantics_Heightfield, 0},
    {"Mask", "Scales the effect per cell", TerrainPortType_Field, TerrainFieldSemantics_Mask, 1}};

const TerrainParameterDescriptor matchParameters[] = {
    {"amount", "Amount", "How far towards the reference's spread the field is taken", "",
     TerrainParameterType_Float, 1.0, 0.0, 1.0, "", nullptr, 0}};

/**
 * Sends every cell through its own rank in the field and back out at the same
 * rank in the reference. Ranks, rather than values, are what carries over: an
 * import and a generated terrain end up with the same distribution of heights
 * without either one's shape being replaced.
 */
void evaluateMatch(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const std::size_t count = cellCount(api, context);
    const float* input = api->inputField(context, 0);
    const Histogram source(input, count);
    const Histogram reference(api->inputField(context, 1), count);
    const float amount = static_cast<float>(api->parameterNumber(context, 0));

    std::vector<float> matched(count);
    for (std::size_t index = 0; index < count; ++index) {
        matched[index] = reference.valueAtQuantile(source.quantileOfValue(input[index]));
    }
    blendByAmount(input, matched.data(), amount, count);
    blendThroughMask(input, matched.data(), api->inputField(context, 2),
                     api->outputField(context, 0), count);
}

const TerrainNodeDescriptor matchDescriptor = {
    "Match",
    "Filter",
    "Reshapes a field's spread of heights to match a reference",
    1,
    matchInputs,
    3,
    adjustOutputs,
    1,
    matchParameters,
    1,
    &evaluateMatch};

/* ------------------------------------------------------------- SoftClip -- */

const TerrainEnumOption softClipSides[] = {{"above", "Above", 0}, {"below", "Below", 1}};

const TerrainParameterDescriptor softClipParameters[] = {
    {"threshold", "Threshold", "Where the field starts being held back", "m",
     TerrainParameterType_Float, 1000.0, -12000.0, 12000.0, "", nullptr, 0},
    {"softness", "Softness", "How far past the threshold the field may still travel", "m",
     TerrainParameterType_Float, 200.0, 0.0, 12000.0, "", nullptr, 0},
    {"side", "Side", "Which side of the threshold is compressed", "",
     TerrainParameterType_Enum, 0.0, 0.0, 1.0, "", softClipSides, 2}};

/**
 * An exponential shoulder rather than a corner: the field approaches
 * `threshold + softness` and never crosses it, and its slope is continuous
 * where a clamp would leave a crease that every later erosion pass finds.
 */
float softClippedAbove(float value, float threshold, float softness) {
    const float excess = value - threshold;
    if (excess <= 0.0f) {
        return value;
    }
    if (softness <= 0.0f) {
        return threshold;
    }
    return threshold + softness * (1.0f - std::exp(-excess / softness));
}

/// The same shoulder read upside down, so one formula serves both sides.
float softClippedBelow(float value, float threshold, float softness) {
    return -softClippedAbove(-value, -threshold, softness);
}

void applySoftClip(const float* input, float* clipped, std::size_t count, float threshold,
                   float softness, bool holdBelow) {
    for (std::size_t index = 0; index < count; ++index) {
        if (holdBelow) {
            clipped[index] = softClippedBelow(input[index], threshold, softness);
            continue;
        }
        clipped[index] = softClippedAbove(input[index], threshold, softness);
    }
}

void evaluateSoftClip(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const std::size_t count = cellCount(api, context);
    const float* input = api->inputField(context, 0);
    const float threshold = static_cast<float>(api->parameterNumber(context, 0));
    const float softness = static_cast<float>(api->parameterNumber(context, 1));
    const bool holdBelow = api->parameterNumber(context, 2) != 0.0;

    std::vector<float> clipped(count);
    applySoftClip(input, clipped.data(), count, threshold, softness, holdBelow);
    blendThroughMask(input, clipped.data(), api->inputField(context, 1),
                     api->outputField(context, 0), count);
}

const TerrainNodeDescriptor softClipDescriptor = {
    "SoftClip",
    "Filter",
    "Eases a field to a limit instead of cutting it against one",
    1,
    adjustInputs,
    2,
    adjustOutputs,
    1,
    softClipParameters,
    3,
    &evaluateSoftClip};

}  // namespace

void registerAdjustNodes(const TerrainRegistryApi& api, TerrainRegistry* registry) {
    api.registerNode(registry, &autolevelDescriptor);
    api.registerNode(registry, &equalizeDescriptor);
    api.registerNode(registry, &matchDescriptor);
    api.registerNode(registry, &softClipDescriptor);
}

}  // namespace terrain::nodes
