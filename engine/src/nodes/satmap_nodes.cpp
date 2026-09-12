#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "color_lookup_support.h"
#include "node_support.h"
#include "terrain/color_field.h"
#include "terrain/satmap_catalogue.h"

namespace terrain::nodes {
namespace {

/* --------------------------------------------------------------- SatMap -- */

/// How far the read position is scattered, per roughness step. The steps
/// double so each is a visible change from the last.
constexpr float roughnessJitter[] = {0.0f, 0.01f, 0.02f, 0.04f, 0.08f};

const TerrainEnumOption satMapRoughOptions[] = {{"none", "None", 0},
                                                {"low", "Low", 1},
                                                {"medium", "Medium", 2},
                                                {"high", "High", 3},
                                                {"ultra", "Ultra", 4}};

const TerrainPortDescriptor satMapInputs[] = {
    {"In", "The field to colour by: a heightfield, or a mask such as a texture base",
     TerrainPortType_Field, TerrainFieldSemantics_Heightfield, 0}};

const TerrainPortDescriptor satMapOutputs[] = {
    {"Out", "The colour map", TerrainPortType_Color, TerrainFieldSemantics_Heightfield, 0}};

/// The library option list is built from what is on disk, so the palette
/// shows the libraries that exist rather than a list typed in here.
struct SatMapRegistration {
    std::vector<std::string> optionNames;
    std::vector<TerrainEnumOption> libraryOptions;
    std::vector<TerrainParameterDescriptor> parameters;
    TerrainNodeDescriptor descriptor;
};

std::string labelFor(const std::string& name) {
    std::string label = name;
    if (!label.empty()) {
        label[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(label[0])));
    }
    return label;
}

std::vector<TerrainEnumOption> libraryOptionsFor(const SatMapCatalogue& catalogue,
                                                 std::vector<std::string>& names) {
    std::vector<TerrainEnumOption> options;
    const std::vector<SatMapLibrary>& libraries = catalogue.libraries();
    names.reserve(libraries.size() * 2);
    for (std::size_t index = 0; index < libraries.size(); ++index) {
        names.push_back(libraries[index].name);
        names.push_back(labelFor(libraries[index].name));
        options.push_back(TerrainEnumOption{names[names.size() - 2].c_str(),
                                            names.back().c_str(), static_cast<int32_t>(index)});
    }
    if (options.empty()) {
        options.push_back(TerrainEnumOption{"none", "No libraries found", 0});
    }
    return options;
}

std::vector<TerrainParameterDescriptor> satMapParametersFor(
    const std::vector<TerrainEnumOption>& libraryOptions) {
    const double lastLibrary = static_cast<double>(libraryOptions.size() - 1);
    return {
        {"library", "Library", "Which family of satellite strips to read from", "",
         TerrainParameterType_Enum, 0.0, 0.0, lastLibrary, "", libraryOptions.data(),
         static_cast<int32_t>(libraryOptions.size())},
        {"item", "Map", "Which strip of the library; counts round past the last", "",
         TerrainParameterType_Int, 0.0, 0.0, 255.0, "", nullptr, 0},
        {"rangeStart", "Clip start", "Where along the strip the lowest ground reads", "",
         TerrainParameterType_Float, 0.0, 0.0, 1.0, "", nullptr, 0},
        {"rangeEnd", "Clip end", "Where along the strip the highest ground reads", "",
         TerrainParameterType_Float, 1.0, 0.0, 1.0, "", nullptr, 0},
        {"bias", "Bias", "Negative leans the strip's low end over more ground, positive the high end",
         "", TerrainParameterType_Float, 0.0, -1.0, 1.0, "", nullptr, 0},
        {"enhance", "Enhance",
         "None reads the field as [0, 1]; autolevel stretches its range over the strip; "
         "equalize gives every colour the same share of ground",
         "", TerrainParameterType_Enum, 1.0, 0.0, 2.0, "", lookupEnhanceOptions, 3},
        {"reverse", "Reverse", "Reads the strip high ground first", "",
         TerrainParameterType_Bool, 0.0, 0.0, 1.0, "", nullptr, 0},
        {"rough", "Roughness",
         "Scatters where each cell reads the strip and which shade it takes, for grain", "",
         TerrainParameterType_Enum, 0.0, 0.0, 4.0, "", satMapRoughOptions, 5},
        {"hueShift", "Hue", "Turns every colour around the hue wheel", "deg",
         TerrainParameterType_Float, 0.0, -180.0, 180.0, "", nullptr, 0},
        {"saturation", "Saturation", "Drains the colour towards grey or pushes it to full", "",
         TerrainParameterType_Float, 0.0, -1.0, 1.0, "", nullptr, 0},
        {"lightness", "Lightness", "Darkens towards black or lifts towards white", "",
         TerrainParameterType_Float, 0.0, -1.0, 1.0, "", nullptr, 0},
        {"seed", "Seed", "Varies the roughness scatter", "", TerrainParameterType_Seed, 0.0,
         0.0, 4294967295.0, "", nullptr, 0},
        {"strip", "Own strip",
         "A PNG of your own to read instead of the library: one column per position, any "
         "number of rows of shades",
         "", TerrainParameterType_Path, 0.0, 0.0, 0.0, "", nullptr, 0}};
}

struct SatMapSettings {
    LookupSettings lookup;
    ColorGrade grade;
    int roughness = 0;
    std::uint64_t seed = 0;
};

SatMapSettings satMapSettingsFor(const TerrainContextApi* api,
                                 TerrainEvaluationContext* context) {
    SatMapSettings settings;
    settings.lookup.rangeStart = static_cast<float>(api->parameterNumber(context, 2));
    settings.lookup.rangeEnd = static_cast<float>(api->parameterNumber(context, 3));
    settings.lookup.bias = static_cast<float>(api->parameterNumber(context, 4));
    settings.lookup.enhance =
        static_cast<LookupEnhance>(static_cast<int>(api->parameterNumber(context, 5)));
    settings.lookup.reverse = api->parameterNumber(context, 6) != 0.0;
    settings.roughness =
        std::clamp(static_cast<int>(api->parameterNumber(context, 7)), 0, 4);
    settings.grade.hueShiftDegrees = static_cast<float>(api->parameterNumber(context, 8));
    settings.grade.saturationShift = static_cast<float>(api->parameterNumber(context, 9));
    settings.grade.lightnessShift = static_cast<float>(api->parameterNumber(context, 10));
    settings.seed = api->randomSeed(context)
                    ^ static_cast<std::uint64_t>(api->parameterNumber(context, 11));
    return settings;
}

std::shared_ptr<const ColorStrip> stripFor(const TerrainContextApi* api,
                                           TerrainEvaluationContext* context) {
    const SatMapCatalogue& catalogue = SatMapCatalogue::shared();
    const char* ownStrip = api->parameterText(context, 12);
    if (ownStrip != nullptr && ownStrip[0] != '\0') {
        std::shared_ptr<const ColorStrip> strip = catalogue.stripAt(ownStrip);
        if (strip == nullptr) {
            api->setError(context, (std::string("Could not read the strip ") + ownStrip).c_str());
        }
        return strip;
    }

    const int library = static_cast<int>(api->parameterNumber(context, 0));
    const int item = static_cast<int>(api->parameterNumber(context, 1));
    std::shared_ptr<const ColorStrip> strip = catalogue.strip(library, item);
    if (strip == nullptr) {
        api->setError(context,
                      "No satellite strip found for this library. Set TERRAIN_ASSET_PATH to a "
                      "directory holding satmaps/<library>/*.png");
    }
    return strip;
}

RgbColor readStrip(const ColorStrip& strip, const SatMapSettings& settings, float position,
                   std::size_t index) {
    if (settings.roughness == 0) {
        return strip.sampleAverage(position);
    }
    const float jitter = (cellNoise(settings.seed, index, 1) * 2.0f - 1.0f)
                         * roughnessJitter[settings.roughness];
    const int row = static_cast<int>(cellNoise(settings.seed, index, 2)
                                     * static_cast<float>(strip.rowCount()));
    return strip.sample(position + jitter, row);
}

void evaluateSatMap(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const std::shared_ptr<const ColorStrip> strip = stripFor(api, context);
    if (strip == nullptr) {
        return;
    }
    const std::size_t count = cellCount(api, context);
    const float* input = api->inputField(context, 0);
    float* output = api->outputColor(context, 0);
    const SatMapSettings settings = satMapSettingsFor(api, context);

    std::vector<float> positions(count);
    lookupPositions(input, count, settings.lookup, positions.data());

    for (std::size_t index = 0; index < count; ++index) {
        const RgbColor color =
            gradeColor(readStrip(*strip, settings, positions[index], index), settings.grade);
        float* cell = output + index * ColorField::channelCount;
        cell[0] = color.red;
        cell[1] = color.green;
        cell[2] = color.blue;
    }
}

SatMapRegistration& satMapRegistration() {
    static SatMapRegistration registration = [] {
        SatMapRegistration built;
        built.libraryOptions = libraryOptionsFor(SatMapCatalogue::shared(), built.optionNames);
        built.parameters = satMapParametersFor(built.libraryOptions);
        built.descriptor = TerrainNodeDescriptor{
            "SatMap",
            "Texture",
            "Colours a field through strips sampled from real satellite imagery",
            2,
            satMapInputs,
            1,
            satMapOutputs,
            1,
            built.parameters.data(),
            static_cast<int32_t>(built.parameters.size()),
            &evaluateSatMap};
        return built;
    }();
    return registration;
}

}  // namespace

void registerSatMapNodes(const TerrainRegistryApi& api, TerrainRegistry* registry) {
    api.registerNode(registry, &satMapRegistration().descriptor);
}

}  // namespace terrain::nodes
