#include <algorithm>
#include <cmath>
#include <vector>

#include "node_support.h"
#include "terrain/curve.h"

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

/* ---------------------------------------------------------------- Clamp -- */

const TerrainPortDescriptor filterInputs[] = {
    {"In", "The field to reshape", TerrainPortType_Field, TerrainFieldSemantics_Heightfield, 0},
    {"Mask", "Scales the effect per cell", TerrainPortType_Field, TerrainFieldSemantics_Mask, 1}};

const TerrainPortDescriptor filterOutputs[] = {
    {"Out", "The reshaped field", TerrainPortType_Field, TerrainFieldSemantics_Heightfield, 0}};

const TerrainParameterDescriptor clampParameters[] = {
    {"minimumValue", "Floor", "Nothing is allowed below this", "m", TerrainParameterType_Float,
     0.0, -12000.0, 12000.0, "", nullptr, 0},
    {"maximumValue", "Ceiling", "Nothing is allowed above this", "m", TerrainParameterType_Float,
     3000.0, -12000.0, 12000.0, "", nullptr, 0}};

void evaluateClamp(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const std::size_t count = cellCount(api, context);
    const float* input = api->inputField(context, 0);
    const float floorValue = static_cast<float>(api->parameterNumber(context, 0));
    const float ceilingValue = static_cast<float>(api->parameterNumber(context, 1));

    std::vector<float> clamped(count);
    for (std::size_t index = 0; index < count; ++index) {
        clamped[index] = std::clamp(input[index], std::min(floorValue, ceilingValue),
                                    std::max(floorValue, ceilingValue));
    }
    blendThroughMask(input, clamped.data(), api->inputField(context, 1),
                     api->outputField(context, 0), count);
}

const TerrainNodeDescriptor clampDescriptor = {"Clamp",
                                               "Filter",
                                               "Holds a field between a floor and a ceiling",
                                               1,
                                               filterInputs,
                                               2,
                                               filterOutputs,
                                               1,
                                               clampParameters,
                                               2,
                                               &evaluateClamp};

/* -------------------------------------------------------------- Terrace -- */

const TerrainParameterDescriptor terraceParameters[] = {
    {"stepCount", "Steps", "How many terraces span the field's own height range", "",
     TerrainParameterType_Int, 12.0, 2.0, 128.0, "", nullptr, 0},
    {"sharpness", "Sharpness", "0 leaves the slope untouched, 1 makes every tread dead flat", "",
     TerrainParameterType_Float, 0.6, 0.0, 1.0, "", nullptr, 0}};

/// Flattens the tread and steepens the riser without moving the step edges,
/// so raising sharpness never shifts a terrace up or down the hill.
float shapeTread(float fraction, float sharpness) {
    const float softness = std::max(1.0f - std::clamp(sharpness, 0.0f, 1.0f), 1.0e-3f);
    return std::pow(std::clamp(fraction, 0.0f, 1.0f), 1.0f / softness);
}

void evaluateTerrace(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const std::size_t count = cellCount(api, context);
    const float* input = api->inputField(context, 0);
    const float stepCount = static_cast<float>(api->parameterNumber(context, 0));
    const float sharpness = static_cast<float>(api->parameterNumber(context, 1));
    const FieldRange range = fieldRange(input, count);

    std::vector<float> stepped(count);
    for (std::size_t index = 0; index < count; ++index) {
        const float scaled = range.normalise(input[index]) * stepCount;
        const float tread = std::floor(scaled);
        const float shaped = shapeTread(scaled - tread, sharpness);
        stepped[index] = range.denormalise((tread + shaped) / stepCount);
    }
    blendThroughMask(input, stepped.data(), api->inputField(context, 1),
                     api->outputField(context, 0), count);
}

const TerrainNodeDescriptor terraceDescriptor = {
    "Terrace",
    "Filter",
    "Cuts a field into level steps, as bedding planes do",
    1,
    filterInputs,
    2,
    filterOutputs,
    1,
    terraceParameters,
    2,
    &evaluateTerrace};

/* ---------------------------------------------------------------- Curve -- */

const TerrainParameterDescriptor curveParameters[] = {
    {"curve", "Curve", "Remaps the field's own range, low on the left", "",
     TerrainParameterType_Curve, 0.0, 0.0, 1.0, "0,0;1,1", nullptr, 0}};

void evaluateCurve(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const std::size_t count = cellCount(api, context);
    const float* input = api->inputField(context, 0);
    const Curve curve = Curve::parse(api->parameterText(context, 0));
    const FieldRange range = fieldRange(input, count);

    std::vector<float> remapped(count);
    for (std::size_t index = 0; index < count; ++index) {
        remapped[index] = range.denormalise(curve.valueAt(range.normalise(input[index])));
    }
    blendThroughMask(input, remapped.data(), api->inputField(context, 1),
                     api->outputField(context, 0), count);
}

const TerrainNodeDescriptor curveDescriptor = {
    "Curve",
    "Filter",
    "Remaps a field through a drawn transfer curve",
    1,
    filterInputs,
    2,
    filterOutputs,
    1,
    curveParameters,
    1,
    &evaluateCurve};

/* ----------------------------------------------------------------- Warp -- */

const TerrainPortDescriptor warpInputs[] = {
    {"In", "The field to distort", TerrainPortType_Field, TerrainFieldSemantics_Heightfield, 0},
    {"Warp", "The field whose shape decides where each cell is read from",
     TerrainPortType_Field, TerrainFieldSemantics_Heightfield, 0},
    {"Mask", "Scales the effect per cell", TerrainPortType_Field, TerrainFieldSemantics_Mask, 1}};

const TerrainParameterDescriptor warpParameters[] = {
    {"amount", "Amount", "How far a cell may be pulled sideways", "m",
     TerrainParameterType_Float, 200.0, 0.0, 20000.0, "", nullptr, 0}};

float sampleBilinearClamped(const float* field, int size, float row, float column) {
    const float clampedRow = std::clamp(row, 0.0f, static_cast<float>(size - 1));
    const float clampedColumn = std::clamp(column, 0.0f, static_cast<float>(size - 1));
    const int lowRow = static_cast<int>(clampedRow);
    const int lowColumn = static_cast<int>(clampedColumn);
    const int highRow = std::min(lowRow + 1, size - 1);
    const int highColumn = std::min(lowColumn + 1, size - 1);
    const float rowFraction = clampedRow - lowRow;
    const float columnFraction = clampedColumn - lowColumn;

    const float* top = field + static_cast<std::size_t>(lowRow) * size;
    const float* bottom = field + static_cast<std::size_t>(highRow) * size;
    const float upper = top[lowColumn] + (top[highColumn] - top[lowColumn]) * columnFraction;
    const float lower =
        bottom[lowColumn] + (bottom[highColumn] - bottom[lowColumn]) * columnFraction;
    return upper + (lower - upper) * rowFraction;
}

/**
 * The two displacement components both come from the warp field, read a third
 * of the grid apart. One field cannot supply two independent offsets, and a
 * fixed large shift decorrelates them far better than using the field twice
 * would, without needing a second input the artist has to wire up.
 */
void evaluateWarp(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const int size = api->width(context);
    const std::size_t count = cellCount(api, context);
    const float* input = api->inputField(context, 0);
    const float* warp = api->inputField(context, 1);
    const float amountCells =
        static_cast<float>(api->parameterNumber(context, 0) / api->cellSizeMeters(context));
    const FieldRange range = fieldRange(warp, count);
    const int shift = size / 3;

    std::vector<float> warped(count);
    forEachRowRange(api, context, size, [&](int firstRow, int lastRow) {
        for (int row = firstRow; row < lastRow; ++row) {
            const int shiftedRow = (row + shift) % size;
            for (int column = 0; column < size; ++column) {
                const std::size_t index = static_cast<std::size_t>(row) * size + column;
                const std::size_t shiftedIndex =
                    static_cast<std::size_t>(shiftedRow) * size + (column + shift) % size;
                const float alongColumns = range.normalise(warp[index]) * 2.0f - 1.0f;
                const float alongRows = range.normalise(warp[shiftedIndex]) * 2.0f - 1.0f;
                warped[index] = sampleBilinearClamped(input, size, row + alongRows * amountCells,
                                                      column + alongColumns * amountCells);
            }
        }
    });
    blendThroughMask(input, warped.data(), api->inputField(context, 2),
                     api->outputField(context, 0), count);
}

const TerrainNodeDescriptor warpDescriptor = {
    "Warp",
    "Filter",
    "Pulls a field sideways by the shape of another, breaking up straight lines",
    1,
    warpInputs,
    3,
    filterOutputs,
    1,
    warpParameters,
    1,
    &evaluateWarp};

}  // namespace

void registerFilterNodes(const TerrainRegistryApi& api, TerrainRegistry* registry) {
    api.registerNode(registry, &blurDescriptor);
    api.registerNode(registry, &clampDescriptor);
    api.registerNode(registry, &terraceDescriptor);
    api.registerNode(registry, &curveDescriptor);
    api.registerNode(registry, &warpDescriptor);
}

}  // namespace terrain::nodes
