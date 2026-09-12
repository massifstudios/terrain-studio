#include <algorithm>
#include <vector>

#include "color_lookup_support.h"
#include "node_support.h"
#include "terrain/color_field.h"
#include "terrain/color_ramp.h"

namespace terrain::nodes {
namespace {

/* ------------------------------------------------------------ ColorRamp -- */

const TerrainPortDescriptor colorRampInputs[] = {
    {"In", "The field to colour by", TerrainPortType_Field, TerrainFieldSemantics_Heightfield,
     0}};

const TerrainPortDescriptor colorOutputs[] = {
    {"Out", "The colour map", TerrainPortType_Color, TerrainFieldSemantics_Heightfield, 0}};

const TerrainParameterDescriptor colorRampParameters[] = {
    {"ramp", "Ramp", "Colours along the gradient, low ground on the left", "",
     TerrainParameterType_ColorRamp, 0.0, 0.0, 1.0,
     "0:#3f5545;0.35:#7c8459;0.6:#8d7b5f;0.82:#9a9791;1:#ffffff", nullptr, 0},
    {"enhance", "Enhance",
     "None reads the field as [0, 1]; autolevel stretches its range over the gradient; "
     "equalize gives every colour the same share of ground",
     "", TerrainParameterType_Enum, 1.0, 0.0, 2.0, "", lookupEnhanceOptions, 3},
    {"reverse", "Reverse", "Reads the gradient high ground first", "",
     TerrainParameterType_Bool, 0.0, 0.0, 1.0, "", nullptr, 0},
    {"bias", "Bias", "Negative leans the gradient's low end over more ground, positive the high end",
     "", TerrainParameterType_Float, 0.0, -1.0, 1.0, "", nullptr, 0},
    {"hueShift", "Hue", "Turns every colour around the hue wheel", "deg",
     TerrainParameterType_Float, 0.0, -180.0, 180.0, "", nullptr, 0},
    {"saturation", "Saturation", "Drains the colour towards grey or pushes it to full", "",
     TerrainParameterType_Float, 0.0, -1.0, 1.0, "", nullptr, 0},
    {"lightness", "Lightness", "Darkens towards black or lifts towards white", "",
     TerrainParameterType_Float, 0.0, -1.0, 1.0, "", nullptr, 0}};

void writeColor(float* destination, const RgbColor& color) {
    destination[0] = color.red;
    destination[1] = color.green;
    destination[2] = color.blue;
}

LookupSettings colorRampLookupFor(const TerrainContextApi* api,
                                  TerrainEvaluationContext* context) {
    LookupSettings settings;
    settings.enhance =
        static_cast<LookupEnhance>(static_cast<int>(api->parameterNumber(context, 1)));
    settings.reverse = api->parameterNumber(context, 2) != 0.0;
    settings.bias = static_cast<float>(api->parameterNumber(context, 3));
    return settings;
}

ColorGrade colorRampGradeFor(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    ColorGrade grade;
    grade.hueShiftDegrees = static_cast<float>(api->parameterNumber(context, 4));
    grade.saturationShift = static_cast<float>(api->parameterNumber(context, 5));
    grade.lightnessShift = static_cast<float>(api->parameterNumber(context, 6));
    return grade;
}

void evaluateColorRamp(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const std::size_t count = cellCount(api, context);
    const float* input = api->inputField(context, 0);
    float* output = api->outputColor(context, 0);
    const ColorRamp ramp = ColorRamp::parse(api->parameterText(context, 0));
    const ColorGrade grade = colorRampGradeFor(api, context);

    std::vector<float> positions(count);
    lookupPositions(input, count, colorRampLookupFor(api, context), positions.data());

    for (std::size_t index = 0; index < count; ++index) {
        writeColor(output + index * ColorField::channelCount,
                   gradeColor(ramp.sample(positions[index]), grade));
    }
}

const TerrainNodeDescriptor colorRampDescriptor = {
    "ColorRamp",
    "Texture",
    "Turns a field into a colour map through a gradient you pick",
    2,
    colorRampInputs,
    1,
    colorOutputs,
    1,
    colorRampParameters,
    7,
    &evaluateColorRamp};

/* ------------------------------------------------------------- MixColor -- */

const TerrainPortDescriptor mixColorInputs[] = {
    {"A", "The colour map at an amount of 0", TerrainPortType_Color,
     TerrainFieldSemantics_Heightfield, 0},
    {"B", "The colour map at an amount of 1", TerrainPortType_Color,
     TerrainFieldSemantics_Heightfield, 0},
    {"Mask", "When connected, this replaces the amount, cell by cell", TerrainPortType_Field,
     TerrainFieldSemantics_Mask, 1}};

const TerrainParameterDescriptor mixColorParameters[] = {
    {"amount", "Amount", "How far from A towards B, when no mask is connected", "",
     TerrainParameterType_Float, 0.5, 0.0, 1.0, "", nullptr, 0}};

void evaluateMixColor(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const std::size_t count = cellCount(api, context);
    const float* first = api->inputColor(context, 0);
    const float* second = api->inputColor(context, 1);
    const float* mask = api->inputField(context, 2);
    float* output = api->outputColor(context, 0);
    const float amount = static_cast<float>(api->parameterNumber(context, 0));

    for (std::size_t index = 0; index < count; ++index) {
        float weight = amount;
        if (mask != nullptr) {
            weight = std::clamp(mask[index], 0.0f, 1.0f);
        }
        for (int channel = 0; channel < ColorField::channelCount; ++channel) {
            const std::size_t sample = index * ColorField::channelCount + channel;
            output[sample] = first[sample] + (second[sample] - first[sample]) * weight;
        }
    }
}

const TerrainNodeDescriptor mixColorDescriptor = {
    "MixColor",
    "Texture",
    "Blends two colour maps, evenly or through a mask",
    1,
    mixColorInputs,
    3,
    colorOutputs,
    1,
    mixColorParameters,
    1,
    &evaluateMixColor};

/* --------------------------------------------------------- SplatWeights -- */

constexpr int splatLayerCount = 4;

const TerrainPortDescriptor splatInputs[] = {
    {"Base", "The layer that shows wherever nothing else claims the ground",
     TerrainPortType_Field, TerrainFieldSemantics_Mask, 1},
    {"Layer 2", "Claims ground in proportion to its weight", TerrainPortType_Field,
     TerrainFieldSemantics_Mask, 1},
    {"Layer 3", "Claims ground in proportion to its weight", TerrainPortType_Field,
     TerrainFieldSemantics_Mask, 1},
    {"Layer 4", "Claims ground in proportion to its weight", TerrainPortType_Field,
     TerrainFieldSemantics_Mask, 1}};

const TerrainPortDescriptor splatOutputs[] = {
    {"Base", "The base layer's share", TerrainPortType_Field, TerrainFieldSemantics_Mask, 0},
    {"Layer 2", "The second layer's share", TerrainPortType_Field, TerrainFieldSemantics_Mask, 0},
    {"Layer 3", "The third layer's share", TerrainPortType_Field, TerrainFieldSemantics_Mask, 0},
    {"Layer 4", "The fourth layer's share", TerrainPortType_Field, TerrainFieldSemantics_Mask,
     0}};

float weightAt(const float* layer, std::size_t index) {
    if (layer == nullptr) {
        return 0.0f;
    }
    return std::max(layer[index], 0.0f);
}

/**
 * Shares out each cell between up to four layers so the four add to exactly
 * one. Where nothing claims a cell it falls to the base layer, because a splat
 * map with a hole in it renders as whatever the consumer happens to have in
 * memory rather than as ground.
 */
void evaluateSplatWeights(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const std::size_t count = cellCount(api, context);
    const float* inputs[splatLayerCount];
    float* outputs[splatLayerCount];
    for (int layer = 0; layer < splatLayerCount; ++layer) {
        inputs[layer] = api->inputField(context, layer);
        outputs[layer] = api->outputField(context, layer);
    }

    for (std::size_t index = 0; index < count; ++index) {
        float total = 0.0f;
        for (int layer = 0; layer < splatLayerCount; ++layer) {
            total += weightAt(inputs[layer], index);
        }
        if (total <= 0.0f) {
            outputs[0][index] = 1.0f;
            for (int layer = 1; layer < splatLayerCount; ++layer) {
                outputs[layer][index] = 0.0f;
            }
            continue;
        }
        for (int layer = 0; layer < splatLayerCount; ++layer) {
            outputs[layer][index] = weightAt(inputs[layer], index) / total;
        }
    }
}

const TerrainNodeDescriptor splatWeightsDescriptor = {
    "SplatWeights",
    "Texture",
    "Shares each cell out between up to four layers so they add to one",
    1,
    splatInputs,
    splatLayerCount,
    splatOutputs,
    splatLayerCount,
    nullptr,
    0,
    &evaluateSplatWeights};

}  // namespace

void registerTextureNodes(const TerrainRegistryApi& api, TerrainRegistry* registry) {
    api.registerNode(registry, &colorRampDescriptor);
    api.registerNode(registry, &mixColorDescriptor);
    api.registerNode(registry, &splatWeightsDescriptor);
}

}  // namespace terrain::nodes
