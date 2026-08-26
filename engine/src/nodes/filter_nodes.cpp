#include <algorithm>
#include <cmath>
#include <vector>

#include "node_support.h"

namespace terrain::nodes {
namespace {

/// One separable box pass along rows, with clamped edges. Three of these
/// approximate a Gaussian closely enough for terrain and cost far less.
void boxPassRows(const float* source, float* destination, int width, int height, int radius) {
    const float scale = 1.0f / (2 * radius + 1);
    for (int row = 0; row < height; ++row) {
        const float* input = source + static_cast<std::size_t>(row) * width;
        float* output = destination + static_cast<std::size_t>(row) * width;
        float total = input[0] * static_cast<float>(radius + 1);
        for (int column = 1; column <= radius; ++column) {
            total += input[std::min(column, width - 1)];
        }
        for (int column = 0; column < width; ++column) {
            total += input[std::min(column + radius, width - 1)];
            total -= input[std::max(column - radius - 1, 0)];
            output[column] = total * scale;
        }
    }
}

void transpose(const float* source, float* destination, int width, int height) {
    for (int row = 0; row < height; ++row) {
        for (int column = 0; column < width; ++column) {
            destination[static_cast<std::size_t>(column) * height + row] =
                source[static_cast<std::size_t>(row) * width + column];
        }
    }
}

void blurSquare(const float* source, float* destination, int size, int radius, int passes) {
    const std::size_t count = static_cast<std::size_t>(size) * size;
    std::vector<float> front(source, source + count);
    std::vector<float> back(count);

    for (int pass = 0; pass < passes; ++pass) {
        boxPassRows(front.data(), back.data(), size, size, radius);
        transpose(back.data(), front.data(), size, size);
        boxPassRows(front.data(), back.data(), size, size, radius);
        transpose(back.data(), front.data(), size, size);
    }
    std::copy(front.begin(), front.end(), destination);
}

const TerrainPortDescriptor blurInputs[] = {
    {"In", "The field to soften", TerrainPortType_Field, TerrainFieldSemantics_Heightfield, 0},
    {"Mask", "Scales the effect per cell", TerrainPortType_Field, TerrainFieldSemantics_Mask, 1}};

const TerrainPortDescriptor blurOutputs[] = {
    {"Out", "The softened field", TerrainPortType_Field, TerrainFieldSemantics_Heightfield, 0}};

const TerrainParameterDescriptor blurParameters[] = {
    {"radius", "Radius", "How far the smoothing reaches, in ground distance", "m",
     TerrainParameterType_Float, 50.0, 0.0, 20000.0, "", nullptr, 0}};

void evaluateBlur(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const int size = api->width(context);
    const std::size_t count = cellCount(api, context);
    const float* input = api->inputField(context, 0);
    float* output = api->outputField(context, 0);

    const double radiusMeters = api->parameterNumber(context, 0);
    const int radiusCells =
        static_cast<int>(std::lround(radiusMeters / api->cellSizeMeters(context)));
    if (radiusCells < 1) {
        std::copy(input, input + count, output);
        return;
    }

    std::vector<float> blurred(count);
    blurSquare(input, blurred.data(), size, radiusCells, 3);
    blendThroughMask(input, blurred.data(), api->inputField(context, 1), output, count);
}

const TerrainNodeDescriptor blurDescriptor = {"Blur",
                                              "Filter",
                                              "Softens a field over a distance in meters",
                                              1,
                                              blurInputs,
                                              2,
                                              blurOutputs,
                                              1,
                                              blurParameters,
                                              1,
                                              &evaluateBlur};

}  // namespace

void registerFilterNodes(const TerrainRegistryApi& api, TerrainRegistry* registry) {
    api.registerNode(registry, &blurDescriptor);
}

}  // namespace terrain::nodes
