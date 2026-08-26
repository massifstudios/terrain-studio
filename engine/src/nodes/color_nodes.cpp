#include <algorithm>
#include <cmath>

#include "node_support.h"
#include "terrain/color_field.h"
#include "terrain/color_space.h"

namespace terrain::nodes {
namespace {

const TerrainPortDescriptor colorInputs[] = {
    {"In", "The colour map to adjust", TerrainPortType_Color, TerrainFieldSemantics_Heightfield,
     0},
    {"Mask", "Scales the effect per cell", TerrainPortType_Field, TerrainFieldSemantics_Mask, 1}};

const TerrainPortDescriptor colorOutputs[] = {
    {"Out", "The adjusted colour map", TerrainPortType_Color, TerrainFieldSemantics_Heightfield,
     0}};

RgbColor readColor(const float* source, std::size_t index) {
    const std::size_t sample = index * ColorField::channelCount;
    return RgbColor{source[sample], source[sample + 1], source[sample + 2]};
}

void writeColor(float* destination, std::size_t index, const RgbColor& color) {
    const std::size_t sample = index * ColorField::channelCount;
    destination[sample] = color.red;
    destination[sample + 1] = color.green;
    destination[sample + 2] = color.blue;
}

/// How far the adjusted colour is allowed to show through, cell by cell.
float maskWeightAt(const float* mask, std::size_t index) {
    if (mask == nullptr) {
        return 1.0f;
    }
    return std::clamp(mask[index], 0.0f, 1.0f);
}

RgbColor mixColors(const RgbColor& original, const RgbColor& adjusted, float weight) {
    return RgbColor{original.red + (adjusted.red - original.red) * weight,
                    original.green + (adjusted.green - original.green) * weight,
                    original.blue + (adjusted.blue - original.blue) * weight};
}

/* ---------------------------------------------------------------- Gamma -- */

const TerrainParameterDescriptor gammaParameters[] = {
    {"gamma", "Gamma", "Below 1 lifts the shadows, above 1 deepens them", "",
     TerrainParameterType_Float, 1.0, 0.05, 8.0, "", nullptr, 0}};

void evaluateGamma(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const std::size_t count = cellCount(api, context);
    const float* input = api->inputColor(context, 0);
    const float* mask = api->inputField(context, 1);
    float* output = api->outputColor(context, 0);
    const float exponent = static_cast<float>(api->parameterNumber(context, 0));

    for (std::size_t index = 0; index < count; ++index) {
        const RgbColor original = readColor(input, index);
        const RgbColor adjusted{std::pow(std::max(original.red, 0.0f), exponent),
                                std::pow(std::max(original.green, 0.0f), exponent),
                                std::pow(std::max(original.blue, 0.0f), exponent)};
        writeColor(output, index, mixColors(original, adjusted, maskWeightAt(mask, index)));
    }
}

const TerrainNodeDescriptor gammaDescriptor = {
    "Gamma",
    "Texture",
    "Bends a colour map's tonal response without moving black or white",
    1,
    colorInputs,
    2,
    colorOutputs,
    1,
    gammaParameters,
    1,
    &evaluateGamma};

/* ------------------------------------------------------------------ HSL -- */

const TerrainParameterDescriptor hslParameters[] = {
    {"hueShift", "Hue shift", "How far every colour is turned around the hue wheel", "deg",
     TerrainParameterType_Float, 0.0, -180.0, 180.0, "", nullptr, 0},
    {"saturation", "Saturation", "Multiplies how strongly each colour reads; 0 leaves grey", "",
     TerrainParameterType_Float, 1.0, 0.0, 4.0, "", nullptr, 0},
    {"lightness", "Lightness", "Added to every colour's lightness", "",
     TerrainParameterType_Float, 0.0, -1.0, 1.0, "", nullptr, 0}};

HslColor shiftedHsl(const HslColor& color, float hueShift, float saturationScale,
                    float lightnessOffset) {
    const float turned = color.hue + hueShift;
    HslColor result;
    result.hue = turned - 360.0f * std::floor(turned / 360.0f);
    result.saturation = std::clamp(color.saturation * saturationScale, 0.0f, 1.0f);
    result.lightness = std::clamp(color.lightness + lightnessOffset, 0.0f, 1.0f);
    return result;
}

void evaluateHsl(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const std::size_t count = cellCount(api, context);
    const float* input = api->inputColor(context, 0);
    const float* mask = api->inputField(context, 1);
    float* output = api->outputColor(context, 0);
    const float hueShift = static_cast<float>(api->parameterNumber(context, 0));
    const float saturationScale = static_cast<float>(api->parameterNumber(context, 1));
    const float lightnessOffset = static_cast<float>(api->parameterNumber(context, 2));

    for (std::size_t index = 0; index < count; ++index) {
        const RgbColor original = readColor(input, index);
        const RgbColor adjusted = toRgb(
            shiftedHsl(toHsl(original), hueShift, saturationScale, lightnessOffset));
        writeColor(output, index, mixColors(original, adjusted, maskWeightAt(mask, index)));
    }
}

const TerrainNodeDescriptor hslDescriptor = {
    "HSL",
    "Texture",
    "Turns the hue, strengthens or drains the colour, and lifts or drops the lightness",
    1,
    colorInputs,
    2,
    colorOutputs,
    1,
    hslParameters,
    3,
    &evaluateHsl};

/* ------------------------------------------------------------- RGBSplit -- */

const TerrainPortDescriptor splitInputs[] = {
    {"In", "The colour map to take apart", TerrainPortType_Color,
     TerrainFieldSemantics_Heightfield, 0}};

const TerrainPortDescriptor splitOutputs[] = {
    {"Red", "The red channel", TerrainPortType_Field, TerrainFieldSemantics_Mask, 0},
    {"Green", "The green channel", TerrainPortType_Field, TerrainFieldSemantics_Mask, 0},
    {"Blue", "The blue channel", TerrainPortType_Field, TerrainFieldSemantics_Mask, 0}};

void evaluateRgbSplit(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const std::size_t count = cellCount(api, context);
    const float* input = api->inputColor(context, 0);
    float* channels[ColorField::channelCount];
    for (int channel = 0; channel < ColorField::channelCount; ++channel) {
        channels[channel] = api->outputField(context, channel);
    }

    for (std::size_t index = 0; index < count; ++index) {
        for (int channel = 0; channel < ColorField::channelCount; ++channel) {
            channels[channel][index] = input[index * ColorField::channelCount + channel];
        }
    }
}

const TerrainNodeDescriptor rgbSplitDescriptor = {
    "RGBSplit",
    "Texture",
    "Takes a colour map apart into one field per channel",
    1,
    splitInputs,
    1,
    splitOutputs,
    ColorField::channelCount,
    nullptr,
    0,
    &evaluateRgbSplit};

/* ------------------------------------------------------------- RGBMerge -- */

const TerrainPortDescriptor mergeInputs[] = {
    {"Red", "The red channel; absent reads as 0", TerrainPortType_Field,
     TerrainFieldSemantics_Mask, 1},
    {"Green", "The green channel; absent reads as 0", TerrainPortType_Field,
     TerrainFieldSemantics_Mask, 1},
    {"Blue", "The blue channel; absent reads as 0", TerrainPortType_Field,
     TerrainFieldSemantics_Mask, 1}};

const TerrainPortDescriptor mergeOutputs[] = {
    {"Out", "The assembled colour map", TerrainPortType_Color, TerrainFieldSemantics_Heightfield,
     0}};

float channelValueAt(const float* channel, std::size_t index) {
    if (channel == nullptr) {
        return 0.0f;
    }
    return std::clamp(channel[index], 0.0f, 1.0f);
}

void evaluateRgbMerge(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const std::size_t count = cellCount(api, context);
    const float* channels[ColorField::channelCount];
    for (int channel = 0; channel < ColorField::channelCount; ++channel) {
        channels[channel] = api->inputField(context, channel);
    }
    float* output = api->outputColor(context, 0);

    for (std::size_t index = 0; index < count; ++index) {
        for (int channel = 0; channel < ColorField::channelCount; ++channel) {
            output[index * ColorField::channelCount + channel] =
                channelValueAt(channels[channel], index);
        }
    }
}

const TerrainNodeDescriptor rgbMergeDescriptor = {
    "RGBMerge",
    "Texture",
    "Assembles a colour map from one field per channel",
    1,
    mergeInputs,
    ColorField::channelCount,
    mergeOutputs,
    1,
    nullptr,
    0,
    &evaluateRgbMerge};

/* ------------------------------------------------------- ColorThreshold -- */

const TerrainPortDescriptor thresholdInputs[] = {
    {"In", "The colour map to select from", TerrainPortType_Color,
     TerrainFieldSemantics_Heightfield, 0}};

const TerrainPortDescriptor thresholdOutputs[] = {
    {"Out", "1 where the channel falls inside the band", TerrainPortType_Field,
     TerrainFieldSemantics_Mask, 0}};

const TerrainEnumOption thresholdChannels[] = {{"luminance", "Luminance", 0},
                                               {"red", "Red", 1},
                                               {"green", "Green", 2},
                                               {"blue", "Blue", 3}};

const TerrainParameterDescriptor thresholdParameters[] = {
    {"channel", "Channel", "Which reading of the colour is tested", "",
     TerrainParameterType_Enum, 0.0, 0.0, 3.0, "", thresholdChannels, 4},
    {"low", "Low", "Nothing below this is selected", "", TerrainParameterType_Float, 0.5, 0.0,
     1.0, "", nullptr, 0},
    {"high", "High", "Nothing above this is selected", "", TerrainParameterType_Float, 1.0, 0.0,
     1.0, "", nullptr, 0},
    {"falloff", "Falloff", "How far outside the band the selection fades to nothing", "",
     TerrainParameterType_Float, 0.05, 0.0, 1.0, "", nullptr, 0}};

float channelReading(const RgbColor& color, int channel) {
    if (channel == 1) {
        return color.red;
    }
    if (channel == 2) {
        return color.green;
    }
    if (channel == 3) {
        return color.blue;
    }
    return luminance(color);
}

void evaluateColorThreshold(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const std::size_t count = cellCount(api, context);
    const float* input = api->inputColor(context, 0);
    float* output = api->outputField(context, 0);
    const int channel = static_cast<int>(api->parameterNumber(context, 0));
    const float low = static_cast<float>(api->parameterNumber(context, 1));
    const float high = static_cast<float>(api->parameterNumber(context, 2));
    const float falloff = static_cast<float>(api->parameterNumber(context, 3));

    for (std::size_t index = 0; index < count; ++index) {
        const float reading = channelReading(readColor(input, index), channel);
        output[index] = bandWeight(reading, std::min(low, high), std::max(low, high), falloff);
    }
}

const TerrainNodeDescriptor colorThresholdDescriptor = {
    "ColorThreshold",
    "Texture",
    "Selects the ground where a colour map falls inside a band",
    1,
    thresholdInputs,
    1,
    thresholdOutputs,
    1,
    thresholdParameters,
    4,
    &evaluateColorThreshold};

}  // namespace

void registerColorNodes(const TerrainRegistryApi& api, TerrainRegistry* registry) {
    api.registerNode(registry, &gammaDescriptor);
    api.registerNode(registry, &hslDescriptor);
    api.registerNode(registry, &rgbSplitDescriptor);
    api.registerNode(registry, &rgbMergeDescriptor);
    api.registerNode(registry, &colorThresholdDescriptor);
}

}  // namespace terrain::nodes
