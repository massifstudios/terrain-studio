#include <algorithm>
#include <cstddef>
#include <vector>

#include "node_support.h"
#include "terrain/color_field.h"
#include "terrain/color_ramp.h"
#include "terrain/histogram.h"

namespace terrain::nodes {
namespace {

/* --------------------------------------------------------------- SatMap -- */

constexpr int satMapRampsPerLibrary = 4;

/**
 * A family of gradients sampled from how one kind of ground reads in
 * satellite imagery, low ground on the left. Muted on purpose: a colour map is
 * a base for lighting and overlays, and saturated stops burn through both.
 */
struct SatMapLibrary {
    const char* name;
    const char* ramps[satMapRampsPerLibrary];
};

const SatMapLibrary satMapLibraries[] = {
    {"rock",
     {"0:#2b2724;0.2:#4a423b;0.45:#6e6259;0.7:#8f857b;0.88:#aaa39a;1:#c9c4bd",
      "0:#3a322c;0.25:#5c4f44;0.5:#7d6d5e;0.72:#9b8b78;0.9:#b8ab9a;1:#d3cabd",
      "0:#23262a;0.3:#3f444a;0.55:#5f656b;0.75:#7f858a;0.9:#a1a6a9;1:#bfc3c5",
      "0:#3d2f28;0.2:#5f4a3d;0.45:#7f6a55;0.65:#8e8069;0.85:#a39a86;1:#bab4a3"}},
    {"green",
     {"0:#1f2e1a;0.25:#33482a;0.5:#4f6b3a;0.7:#6f864a;0.85:#8f9c5c;1:#b1ad78",
      "0:#14231a;0.3:#22392a;0.55:#375540;0.75:#5a7150;0.9:#7f8a64;1:#a3a082",
      "0:#2a3a1e;0.2:#3e5426;0.45:#5e7330;0.65:#7e8d43;0.85:#9f9f5e;1:#c1b57e",
      "0:#30401c;0.3:#4c5f2b;0.55:#6e7d3e;0.75:#8e9256;0.9:#aca277;1:#c8b899"}},
    {"sand",
     {"0:#8b7355;0.25:#a68a63;0.5:#c2a476;0.7:#d5ba8c;0.88:#e4cea5;1:#efdfc0",
      "0:#7a5b3c;0.3:#9c7a52;0.55:#bb9a6e;0.75:#d2b48c;0.9:#e2caa8;1:#ecdcc2",
      "0:#6f6a5d;0.25:#8e8672;0.5:#aca38a;0.7:#c4bba0;0.88:#d8d0b7;1:#e8e2cf",
      "0:#9c4f2e;0.25:#b76a3d;0.5:#cd8752;0.7:#dba46e;0.88:#e6bd8d;1:#efd3ac"}},
    {"snow",
     {"0:#4b5560;0.3:#707a84;0.55:#9aa3ab;0.75:#c3c9ce;0.9:#e2e6e9;1:#f7f8f9",
      "0:#3a3f47;0.25:#5f6a73;0.5:#8a969e;0.7:#b3bec4;0.85:#d6dde1;1:#f2f5f7",
      "0:#565c58;0.3:#7d847e;0.55:#a4aaa4;0.75:#c6cac6;0.9:#e0e3e0;1:#f5f6f5",
      "0:#405a6a;0.3:#6b8593;0.55:#98acb7;0.75:#c0ced6;0.9:#e0e8ec;1:#f6f9fa"}},
    {"blue",
     {"0:#0b1f33;0.3:#153a5a;0.55:#256485;0.75:#4a8fae;0.9:#7ab6cd;1:#b5dbe8",
      "0:#102a2c;0.3:#1d4a4c;0.55:#2f6e6d;0.75:#4f918c;0.9:#7fb2ab;1:#b3d1cb",
      "0:#1b2733;0.3:#2f4557;0.55:#4a6a80;0.75:#6f8fa3;0.9:#9bb3c2;1:#c9d8e1",
      "0:#07141f;0.35:#0f2c44;0.6:#1d4d70;0.8:#3d7599;0.92:#6ea1bf;1:#a9cee0"}},
    {"color",
     {"0:#2c4a3a;0.2:#557b4a;0.4:#8c9a5a;0.55:#a98a5c;0.7:#8a6f57;0.85:#a39c94;1:#f2f2f0",
      "0:#1e3f5a;0.15:#3b7a6a;0.3:#6c9a5a;0.5:#b3a860;0.68:#a07b4e;0.85:#8a7a72;1:#ffffff",
      "0:#3b2f2f;0.2:#6a4b3a;0.4:#a2724a;0.6:#c9a466;0.8:#d8c79b;1:#f0ead9",
      "0:#1a2a1a;0.25:#2f4f2f;0.45:#5f7a3a;0.6:#9a8f4a;0.75:#7f6a52;0.9:#b0aaa2;1:#ffffff"}}};

constexpr int satMapLibraryCount = static_cast<int>(sizeof(satMapLibraries) / sizeof(SatMapLibrary));

const TerrainEnumOption satMapLibraryOptions[] = {
    {"rock", "Rock", 0}, {"green", "Green", 1}, {"sand", "Sand", 2},
    {"snow", "Snow", 3}, {"blue", "Blue", 4},   {"color", "Color", 5}};

enum class SatMapSpread { Range = 0, Equalized = 1 };

const TerrainEnumOption satMapSpreadOptions[] = {{"range", "Range", 0},
                                                 {"equalized", "Equalized", 1}};

const TerrainPortDescriptor satMapInputs[] = {
    {"In", "The field to colour by", TerrainPortType_Field, TerrainFieldSemantics_Heightfield,
     0}};

const TerrainPortDescriptor satMapOutputs[] = {
    {"Out", "The colour map", TerrainPortType_Color, TerrainFieldSemantics_Heightfield, 0}};

const TerrainParameterDescriptor satMapParameters[] = {
    {"library", "Library", "Which kind of ground the gradients are sampled from", "",
     TerrainParameterType_Enum, 0.0, 0.0, satMapLibraryCount - 1, "", satMapLibraryOptions,
     satMapLibraryCount},
    {"index", "Map", "Which gradient of the library is used; counts round past the last", "",
     TerrainParameterType_Int, 0.0, 0.0, 63.0, "", nullptr, 0},
    {"reverse", "Reverse", "Reads the gradient high ground first", "",
     TerrainParameterType_Bool, 0.0, 0.0, 1.0, "", nullptr, 0},
    {"spread", "Spread",
     "Range lays the gradient over the field's own range; equalized gives every colour the "
     "same share of ground",
     "", TerrainParameterType_Enum, 0.0, 0.0, 1.0, "", satMapSpreadOptions, 2}};

const char* satMapRampText(int library, int index) {
    const int clampedLibrary = std::clamp(library, 0, satMapLibraryCount - 1);
    const int wrappedIndex = std::max(index, 0) % satMapRampsPerLibrary;
    return satMapLibraries[clampedLibrary].ramps[wrappedIndex];
}

/// Where each cell sits in [0, 1] before the gradient is read, per the spread.
void rampPositions(const float* input, std::size_t count, SatMapSpread spread,
                   float* positions) {
    if (spread == SatMapSpread::Equalized) {
        const Histogram histogram(input, count);
        for (std::size_t index = 0; index < count; ++index) {
            positions[index] = histogram.quantileOfValue(input[index]);
        }
        return;
    }
    const FieldRange range = fieldRange(input, count);
    for (std::size_t index = 0; index < count; ++index) {
        positions[index] = range.normalise(input[index]);
    }
}

void evaluateSatMap(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const std::size_t count = cellCount(api, context);
    const float* input = api->inputField(context, 0);
    float* output = api->outputColor(context, 0);
    const ColorRamp ramp = ColorRamp::parse(
        satMapRampText(static_cast<int>(api->parameterNumber(context, 0)),
                       static_cast<int>(api->parameterNumber(context, 1))));
    const bool reversed = api->parameterNumber(context, 2) != 0.0;
    const auto spread = static_cast<SatMapSpread>(static_cast<int>(api->parameterNumber(context, 3)));

    std::vector<float> positions(count);
    rampPositions(input, count, spread, positions.data());

    for (std::size_t index = 0; index < count; ++index) {
        float position = positions[index];
        if (reversed) {
            position = 1.0f - position;
        }
        const RgbColor color = ramp.sample(position);
        float* cell = output + index * ColorField::channelCount;
        cell[0] = color.red;
        cell[1] = color.green;
        cell[2] = color.blue;
    }
}

const TerrainNodeDescriptor satMapDescriptor = {
    "SatMap",
    "Texture",
    "Colours a field through a library of satellite-sampled gradients",
    1,
    satMapInputs,
    1,
    satMapOutputs,
    1,
    satMapParameters,
    4,
    &evaluateSatMap};

}  // namespace

void registerSatMapNodes(const TerrainRegistryApi& api, TerrainRegistry* registry) {
    api.registerNode(registry, &satMapDescriptor);
}

}  // namespace terrain::nodes
