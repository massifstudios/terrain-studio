#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

#include "color_lookup_support.h"
#include "node_support.h"
#include "terrain/color_field.h"
#include "terrain/color_ramp.h"
#include "terrain/texture_catalogue.h"

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
constexpr int splatStackedLayerCount = splatLayerCount - 1;

const TerrainPortDescriptor splatInputs[] = {
    {"Layer 2", "Covers the base wherever its weight says so", TerrainPortType_Field,
     TerrainFieldSemantics_Mask, 1},
    {"Layer 3", "Covers the base and layer 2", TerrainPortType_Field, TerrainFieldSemantics_Mask,
     1},
    {"Layer 4", "The topmost layer, covering everything under it", TerrainPortType_Field,
     TerrainFieldSemantics_Mask, 1}};

const TerrainPortDescriptor splatOutputs[] = {
    {"Base", "The base layer's share: whatever no layer above it covers", TerrainPortType_Field,
     TerrainFieldSemantics_Mask, 0},
    {"Layer 2", "The second layer's share", TerrainPortType_Field, TerrainFieldSemantics_Mask, 0},
    {"Layer 3", "The third layer's share", TerrainPortType_Field, TerrainFieldSemantics_Mask, 0},
    {"Layer 4", "The fourth layer's share", TerrainPortType_Field, TerrainFieldSemantics_Mask,
     0}};

float coverAt(const float* layer, std::size_t index) {
    if (layer == nullptr) {
        return 0.0f;
    }
    return std::clamp(layer[index], 0.0f, 1.0f);
}

/**
 * Stacks the layers like paint: the topmost layer takes what its mask says,
 * each layer under it takes its share of whatever is still showing, and the
 * base takes the rest. The four add to exactly one, so a splat map never has
 * a hole for a consumer to fill with whatever it has in memory. Stacking
 * rather than sharing in proportion keeps a boundary crisp: a cell that snow
 * covers fully is snow, not a blend of snow and the rock its mask also named.
 */
void evaluateSplatWeights(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const std::size_t count = cellCount(api, context);
    const float* inputs[splatStackedLayerCount];
    float* outputs[splatLayerCount];
    for (int layer = 0; layer < splatStackedLayerCount; ++layer) {
        inputs[layer] = api->inputField(context, layer);
    }
    for (int layer = 0; layer < splatLayerCount; ++layer) {
        outputs[layer] = api->outputField(context, layer);
    }

    for (std::size_t index = 0; index < count; ++index) {
        float showing = 1.0f;
        for (int layer = splatStackedLayerCount - 1; layer >= 0; --layer) {
            const float share = coverAt(inputs[layer], index) * showing;
            outputs[layer + 1][index] = share;
            showing -= share;
        }
        outputs[0][index] = showing;
    }
}

/// Each layer names the texture the viewport tiles where it has weight. The
/// option list is read off the asset directories, so it offers the textures
/// that exist rather than a list typed in here.
struct SplatWeightsRegistration {
    std::vector<std::string> optionNames;
    std::vector<TerrainEnumOption> textureOptions;
    std::vector<TerrainParameterDescriptor> parameters;
    TerrainNodeDescriptor descriptor;
};

/// "aerial_rocks_02.jpg" reads as "Aerial rocks 02".
std::string textureLabel(const std::string& fileName) {
    std::string label = fileName.substr(0, fileName.find_last_of('.'));
    std::replace(label.begin(), label.end(), '_', ' ');
    if (!label.empty()) {
        label[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(label[0])));
    }
    return label;
}

std::vector<TerrainEnumOption> textureOptionsFor(const TextureCatalogue& catalogue,
                                                 std::vector<std::string>& names) {
    std::vector<TerrainEnumOption> options;
    const std::vector<std::string>& textures = catalogue.names();
    names.reserve(textures.size() * 2);
    for (std::size_t index = 0; index < textures.size(); ++index) {
        names.push_back(textures[index]);
        names.push_back(textureLabel(textures[index]));
        options.push_back(TerrainEnumOption{names[names.size() - 2].c_str(),
                                            names.back().c_str(), static_cast<int32_t>(index)});
    }
    if (options.empty()) {
        options.push_back(TerrainEnumOption{"none", "No textures found", 0});
    }
    return options;
}

const char* const splatTextureParameterNames[splatLayerCount] = {
    "baseTexture", "layer2Texture", "layer3Texture", "layer4Texture"};
const char* const splatTextureLabels[splatLayerCount] = {"Base texture", "Layer 2 texture",
                                                         "Layer 3 texture", "Layer 4 texture"};

/// Every layer starts on a different texture, so a freshly placed node already
/// shows its layers apart.
std::vector<TerrainParameterDescriptor> splatParametersFor(
    const std::vector<TerrainEnumOption>& textureOptions) {
    const std::size_t lastTexture = textureOptions.size() - 1;
    std::vector<TerrainParameterDescriptor> parameters;
    for (int layer = 0; layer < splatLayerCount; ++layer) {
        const std::size_t preset = std::min(static_cast<std::size_t>(layer), lastTexture);
        parameters.push_back({splatTextureParameterNames[layer], splatTextureLabels[layer],
                              "The texture the viewport tiles where this layer has weight", "",
                              TerrainParameterType_Texture, 0.0, 0.0, 0.0,
                              textureOptions[preset].name, textureOptions.data(),
                              static_cast<int32_t>(textureOptions.size())});
    }
    return parameters;
}

SplatWeightsRegistration& splatWeightsRegistration() {
    static SplatWeightsRegistration registration = [] {
        SplatWeightsRegistration built;
        built.textureOptions = textureOptionsFor(TextureCatalogue::shared(), built.optionNames);
        built.parameters = splatParametersFor(built.textureOptions);
        built.descriptor = TerrainNodeDescriptor{
            "SplatWeights",
            "Texture",
            "Stacks up to three layers over a base so every cell's four shares add to one",
            3,
            splatInputs,
            splatStackedLayerCount,
            splatOutputs,
            splatLayerCount,
            built.parameters.data(),
            static_cast<int32_t>(built.parameters.size()),
            &evaluateSplatWeights};
        return built;
    }();
    return registration;
}

}  // namespace

void registerTextureNodes(const TerrainRegistryApi& api, TerrainRegistry* registry) {
    api.registerNode(registry, &colorRampDescriptor);
    api.registerNode(registry, &mixColorDescriptor);
    api.registerNode(registry, &splatWeightsRegistration().descriptor);
}

}  // namespace terrain::nodes
