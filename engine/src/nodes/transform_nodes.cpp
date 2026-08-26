#include <algorithm>
#include <cmath>
#include <vector>

#include "node_support.h"
#include "shape_support.h"

namespace terrain::nodes {
namespace {

const TerrainPortDescriptor transformInputs[] = {
    {"In", "The field to rearrange", TerrainPortType_Field, TerrainFieldSemantics_Heightfield, 0}};

const TerrainPortDescriptor maskedInputs[] = {
    {"In", "The field to rework", TerrainPortType_Field, TerrainFieldSemantics_Heightfield, 0},
    {"Mask", "Scales the effect per cell", TerrainPortType_Field, TerrainFieldSemantics_Mask, 1}};

const TerrainPortDescriptor transformOutputs[] = {
    {"Out", "The reworked field", TerrainPortType_Field, TerrainFieldSemantics_Heightfield, 0}};

/* ----------------------------------------------------------------- Flip -- */

const TerrainEnumOption flipAxes[] = {
    {"horizontal", "Horizontal", 0}, {"vertical", "Vertical", 1}, {"both", "Both", 2}};

const TerrainParameterDescriptor flipParameters[] = {
    {"axis", "Axis", "Horizontal mirrors east against west, vertical north against south", "",
     TerrainParameterType_Enum, 0.0, 0.0, 2.0, "", flipAxes, 3}};

int mirroredIndex(int index, int size, bool mirrored) {
    if (mirrored) {
        return size - 1 - index;
    }
    return index;
}

void evaluateFlip(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const int size = api->width(context);
    const int axis = static_cast<int>(api->parameterNumber(context, 0));
    const float* input = api->inputField(context, 0);
    float* output = api->outputField(context, 0);
    const bool mirrorColumns = axis == 0 || axis == 2;
    const bool mirrorRows = axis == 1 || axis == 2;

    forEachRowRange(api, context, size, [&](int firstRow, int lastRow) {
        for (int row = firstRow; row < lastRow; ++row) {
            const float* sourceLine =
                input + static_cast<std::size_t>(mirroredIndex(row, size, mirrorRows)) * size;
            float* line = output + static_cast<std::size_t>(row) * size;
            for (int column = 0; column < size; ++column) {
                line[column] = sourceLine[mirroredIndex(column, size, mirrorColumns)];
            }
        }
    });
}

const TerrainNodeDescriptor flipDescriptor = {"Flip",
                                              "Filter",
                                              "Mirrors a field about the middle of the world",
                                              1,
                                              transformInputs,
                                              1,
                                              transformOutputs,
                                              1,
                                              flipParameters,
                                              1,
                                              &evaluateFlip};

/* --------------------------------------------------------------- Repeat -- */

const TerrainParameterDescriptor repeatParameters[] = {
    {"repeatCount", "Repeats", "How many copies of the field span the world along each axis", "",
     TerrainParameterType_Int, 2.0, 1.0, 64.0, "", nullptr, 0},
    {"mirror", "Mirror", "Flips every other copy so the tiles meet without a seam", "",
     TerrainParameterType_Bool, 1.0, 0.0, 1.0, "", nullptr, 0}};

int tiledIndex(int index, int size, int repeatCount, bool mirror) {
    const long long scaled = static_cast<long long>(index) * repeatCount;
    const int within = static_cast<int>(scaled % size);
    const bool oddTile = (scaled / size) % 2 == 1;
    if (mirror && oddTile) {
        return size - 1 - within;
    }
    return within;
}

void evaluateRepeat(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const int size = api->width(context);
    const int repeatCount = std::max(static_cast<int>(api->parameterNumber(context, 0)), 1);
    const bool mirror = api->parameterNumber(context, 1) != 0.0;
    const float* input = api->inputField(context, 0);
    float* output = api->outputField(context, 0);

    forEachRowRange(api, context, size, [&](int firstRow, int lastRow) {
        for (int row = firstRow; row < lastRow; ++row) {
            const int sourceRow = tiledIndex(row, size, repeatCount, mirror);
            const float* sourceLine = input + static_cast<std::size_t>(sourceRow) * size;
            float* line = output + static_cast<std::size_t>(row) * size;
            for (int column = 0; column < size; ++column) {
                line[column] = sourceLine[tiledIndex(column, size, repeatCount, mirror)];
            }
        }
    });
}

const TerrainNodeDescriptor repeatDescriptor = {
    "Repeat",
    "Filter",
    "Tiles a field across the world, optionally mirroring so the seams close",
    1,
    transformInputs,
    1,
    transformOutputs,
    1,
    repeatParameters,
    2,
    &evaluateRepeat};

/* -------------------------------------------------------------- Pixelate -- */

const TerrainParameterDescriptor pixelateParameters[] = {
    {"blockSize", "Block size", "Ground distance across one flat block", "m",
     TerrainParameterType_Float, 200.0, 0.0, 100000.0, "", nullptr, 0}};

/// Block averages, gathered in one fixed pass so the sums never depend on how
/// the work was divided.
std::vector<double> blockAverages(const float* input, int size, int blockCells, int blockCount) {
    std::vector<double> totals(static_cast<std::size_t>(blockCount) * blockCount, 0.0);
    std::vector<double> counts(totals.size(), 0.0);
    for (int row = 0; row < size; ++row) {
        const std::size_t blockRow = static_cast<std::size_t>(row / blockCells);
        for (int column = 0; column < size; ++column) {
            const std::size_t block = blockRow * blockCount + column / blockCells;
            totals[block] += input[static_cast<std::size_t>(row) * size + column];
            counts[block] += 1.0;
        }
    }
    for (std::size_t block = 0; block < totals.size(); ++block) {
        totals[block] /= std::max(counts[block], 1.0);
    }
    return totals;
}

void evaluatePixelate(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const int size = api->width(context);
    const std::size_t count = cellCount(api, context);
    const float* input = api->inputField(context, 0);
    const int blockCells = std::max(
        static_cast<int>(std::lround(api->parameterNumber(context, 0)
                                     / api->cellSizeMeters(context))),
        1);
    const int blockCount = (size + blockCells - 1) / blockCells;
    const std::vector<double> averages = blockAverages(input, size, blockCells, blockCount);

    std::vector<float> blocky(count);
    for (int row = 0; row < size; ++row) {
        const std::size_t blockRow = static_cast<std::size_t>(row / blockCells);
        for (int column = 0; column < size; ++column) {
            blocky[static_cast<std::size_t>(row) * size + column] =
                static_cast<float>(averages[blockRow * blockCount + column / blockCells]);
        }
    }
    blendThroughMask(input, blocky.data(), api->inputField(context, 1),
                     api->outputField(context, 0), count);
}

const TerrainNodeDescriptor pixelateDescriptor = {
    "Pixelate",
    "Filter",
    "Averages a field into square blocks of a fixed ground size",
    1,
    maskedInputs,
    2,
    transformOutputs,
    1,
    pixelateParameters,
    1,
    &evaluatePixelate};

/* ------------------------------------------------------------------ Edge -- */

const TerrainParameterDescriptor edgeParameters[] = {
    {"width", "Width", "How far in from the border the fade reaches", "m",
     TerrainParameterType_Float, 500.0, 0.0, 100000.0, "", nullptr, 0},
    {"edgeValue", "Edge value", "What the field is faded to at the border", "m",
     TerrainParameterType_Float, 0.0, -12000.0, 12000.0, "", nullptr, 0}};

/// Ground distance from a cell centre to the nearest edge of the world square.
double distanceToBorder(const WorldFrame& frame, int row, int column) {
    const double extent = frame.extentMeters();
    const double easting = frame.eastingAt(column);
    const double northing = frame.northingAt(row);
    return std::min(std::min(easting, extent - easting), std::min(northing, extent - northing));
}

void evaluateEdge(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const WorldFrame frame = worldFrameFor(api, context);
    const std::size_t count = cellCount(api, context);
    const float* input = api->inputField(context, 0);
    const double widthMeters = api->parameterNumber(context, 0);
    const float edgeValue = static_cast<float>(api->parameterNumber(context, 1));

    std::vector<float> faded(count);
    forEachRowRange(api, context, frame.size, [&](int firstRow, int lastRow) {
        for (int row = firstRow; row < lastRow; ++row) {
            for (int column = 0; column < frame.size; ++column) {
                const std::size_t index = static_cast<std::size_t>(row) * frame.size + column;
                const float nearBorder =
                    insideWeight(distanceToBorder(frame, row, column), widthMeters);
                faded[index] = edgeValue + (input[index] - edgeValue) * (1.0f - nearBorder);
            }
        }
    });
    blendThroughMask(input, faded.data(), api->inputField(context, 1),
                     api->outputField(context, 0), count);
}

const TerrainNodeDescriptor edgeDescriptor = {
    "Edge",
    "Filter",
    "Fades a field to a fixed value towards the border, so tiles meet cleanly",
    1,
    maskedInputs,
    2,
    transformOutputs,
    1,
    edgeParameters,
    2,
    &evaluateEdge};

}  // namespace

void registerTransformNodes(const TerrainRegistryApi& api, TerrainRegistry* registry) {
    api.registerNode(registry, &flipDescriptor);
    api.registerNode(registry, &repeatDescriptor);
    api.registerNode(registry, &pixelateDescriptor);
    api.registerNode(registry, &edgeDescriptor);
}

}  // namespace terrain::nodes
