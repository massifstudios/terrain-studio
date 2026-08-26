#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "node_support.h"

namespace terrain::nodes {
namespace {

/* ----------------------------------------------------------- GrowthMask -- */

const TerrainPortDescriptor growthInputs[] = {
    {"In", "The surface things have to grow on", TerrainPortType_Field,
     TerrainFieldSemantics_Heightfield, 0},
    {"Moisture", "Where water gathers, usually a flow mask", TerrainPortType_Field,
     TerrainFieldSemantics_Mask, 1}};

const TerrainPortDescriptor growthOutputs[] = {
    {"Out", "How well things grow, from 0 to 1", TerrainPortType_Field,
     TerrainFieldSemantics_Mask, 0}};

const TerrainParameterDescriptor growthParameters[] = {
    {"minimumAltitude", "Lowest", "Nothing grows below this", "m", TerrainParameterType_Float,
     600.0, -12000.0, 12000.0, "", nullptr, 0},
    {"maximumAltitude", "Treeline", "Nothing grows above this", "m", TerrainParameterType_Float,
     2100.0, -12000.0, 12000.0, "", nullptr, 0},
    {"altitudeFalloff", "Altitude falloff", "How gradually the band fades out at both ends", "m",
     TerrainParameterType_Float, 250.0, 0.0, 5000.0, "", nullptr, 0},
    {"maximumSlope", "Steepest", "Nothing takes root on ground steeper than this", "deg",
     TerrainParameterType_Float, 35.0, 0.0, 90.0, "", nullptr, 0},
    {"slopeFalloff", "Slope falloff", "How gradually the slope limit bites", "deg",
     TerrainParameterType_Float, 10.0, 0.0, 90.0, "", nullptr, 0},
    {"moisture", "Moisture influence", "How much the moisture input decides where things grow",
     "", TerrainParameterType_Float, 0.4, 0.0, 1.0, "", nullptr, 0}};

/// Moisture only ever scales the result down, and only as far as the influence
/// dial allows, so leaving the input unconnected changes nothing.
float moistureWeight(const float* moisture, std::size_t index, float influence) {
    if (moisture == nullptr) {
        return 1.0f;
    }
    return 1.0f - influence + influence * std::clamp(moisture[index], 0.0f, 1.0f);
}

/**
 * The five-node subgraph everyone builds by hand — an altitude band, a slope
 * limit and a moisture proxy multiplied together — as one node, because that
 * subgraph is the same every time.
 */
void evaluateGrowthMask(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const int size = api->width(context);
    const double cellSize = api->cellSizeMeters(context);
    const float* input = api->inputField(context, 0);
    const float* moisture = api->inputField(context, 1);
    float* output = api->outputField(context, 0);

    const float lowest = static_cast<float>(api->parameterNumber(context, 0));
    const float highest = static_cast<float>(api->parameterNumber(context, 1));
    const float altitudeFalloff = static_cast<float>(api->parameterNumber(context, 2));
    const float steepest = static_cast<float>(api->parameterNumber(context, 3));
    const float slopeFalloff = static_cast<float>(api->parameterNumber(context, 4));
    const float influence = static_cast<float>(api->parameterNumber(context, 5));

    forEachRowRange(api, context, size, [&](int firstRow, int lastRow) {
        for (int row = firstRow; row < lastRow; ++row) {
            for (int column = 0; column < size; ++column) {
                const std::size_t index = static_cast<std::size_t>(row) * size + column;
                const float altitude =
                    bandWeight(input[index], std::min(lowest, highest),
                               std::max(lowest, highest), altitudeFalloff);
                const float slope = bandWeight(
                    slopeDegrees(sampleGradient(input, size, row, column, cellSize)), 0.0f,
                    steepest, slopeFalloff);
                output[index] = altitude * slope * moistureWeight(moisture, index, influence);
            }
        }
    });
}

const TerrainNodeDescriptor growthMaskDescriptor = {
    "GrowthMask",
    "Vegetation",
    "Where things will grow: an altitude band, a slope limit and moisture, together",
    1,
    growthInputs,
    2,
    growthOutputs,
    1,
    growthParameters,
    6,
    &evaluateGrowthMask};

/* -------------------------------------------------------------- Scatter -- */

const TerrainPortDescriptor scatterInputs[] = {
    {"Density", "How likely an instance is, cell by cell", TerrainPortType_Field,
     TerrainFieldSemantics_Mask, 0},
    {"Surface", "The surface instances sit on, for their altitude", TerrainPortType_Field,
     TerrainFieldSemantics_Heightfield, 1}};

const TerrainPortDescriptor scatterOutputs[] = {
    {"Out", "The scattered instances", TerrainPortType_Points,
     TerrainFieldSemantics_Heightfield, 0}};

const TerrainParameterDescriptor scatterParameters[] = {
    {"spacing", "Spacing", "The closest two instances are allowed to stand", "m",
     TerrainParameterType_Float, 25.0, 0.5, 5000.0, "", nullptr, 0},
    {"seed", "Seed", "Varies this node alone, on top of the graph seed", "",
     TerrainParameterType_Seed, 0.0, 0.0, 4294967295.0, "", nullptr, 0},
    {"threshold", "Threshold", "Density below this is treated as bare ground", "",
     TerrainParameterType_Float, 0.15, 0.0, 1.0, "", nullptr, 0},
    {"scaleMinimum", "Smallest", "The smallest an instance may be scaled", "",
     TerrainParameterType_Float, 0.8, 0.05, 10.0, "", nullptr, 0},
    {"scaleMaximum", "Largest", "The largest an instance may be scaled", "",
     TerrainParameterType_Float, 1.25, 0.05, 10.0, "", nullptr, 0},
    {"maximumCount", "Limit", "Stops once this many instances have been placed", "",
     TerrainParameterType_Int, 200000.0, 1.0, 20000000.0, "", nullptr, 0}};

struct ScatterSettings {
    int size = 0;
    double cellSizeMeters = 1.0;
    double worldSizeMeters = 1.0;
    float spacingMeters = 25.0f;
    float threshold = 0.15f;
    float scaleMinimum = 0.8f;
    float scaleMaximum = 1.25f;
    std::int64_t maximumCount = 0;
    std::uint64_t seed = 0;
};

/// A candidate per background cell, jittered inside it. Cells this size hold
/// at most one accepted point, so a rejection test only has to look at the
/// neighbours within two cells.
struct ScatterGrid {
    int columns = 0;
    double cellMeters = 1.0;
    std::vector<std::int64_t> occupant;

    std::size_t indexAt(int row, int column) const {
        return static_cast<std::size_t>(row) * columns + column;
    }
};

ScatterGrid makeScatterGrid(const ScatterSettings& settings) {
    ScatterGrid grid;
    grid.cellMeters = settings.spacingMeters / std::numbers::sqrt2_v<double>;
    grid.columns = std::max(static_cast<int>(settings.worldSizeMeters / grid.cellMeters), 1);
    grid.cellMeters = settings.worldSizeMeters / grid.columns;
    grid.occupant.assign(static_cast<std::size_t>(grid.columns) * grid.columns, -1);
    return grid;
}

/// Nearest-cell lookup at a world position. Row 0 is north, so northing counts
/// down. Returns 0 for a field that is not connected.
float sampleNearest(const float* field, const ScatterSettings& settings, double eastingMeters,
                    double northingMeters) {
    if (field == nullptr) {
        return 0.0f;
    }
    const int column = std::clamp(static_cast<int>(eastingMeters / settings.cellSizeMeters), 0,
                                  settings.size - 1);
    const int row = std::clamp(
        static_cast<int>((settings.worldSizeMeters - northingMeters) / settings.cellSizeMeters),
        0, settings.size - 1);
    return field[static_cast<std::size_t>(row) * settings.size + column];
}

bool standsClearOf(const ScatterGrid& grid, const std::vector<TerrainPoint>& placed, int row,
                   int column, double eastingMeters, double northingMeters, float spacing) {
    for (int nearRow = std::max(row - 2, 0); nearRow <= std::min(row + 2, grid.columns - 1);
         ++nearRow) {
        for (int nearColumn = std::max(column - 2, 0);
             nearColumn <= std::min(column + 2, grid.columns - 1); ++nearColumn) {
            const std::int64_t occupant = grid.occupant[grid.indexAt(nearRow, nearColumn)];
            if (occupant < 0) {
                continue;
            }
            const double eastGap = placed[occupant].eastingMeters - eastingMeters;
            const double northGap = placed[occupant].northingMeters - northingMeters;
            if (eastGap * eastGap + northGap * northGap < spacing * spacing) {
                return false;
            }
        }
    }
    return true;
}

/// The fields a scatter reads. Density decides where, surface decides how high.
struct ScatterSources {
    const float* density = nullptr;
    const float* surface = nullptr;
};

/// The one candidate a background cell offers, before any spacing test.
TerrainPoint candidateIn(int row, int column, const ScatterSettings& settings,
                         const ScatterGrid& grid, const ScatterSources& sources,
                         bool& isWanted) {
    // The id is the cell mixed with the seed: unique, stable, and unchanged by
    // whether the neighbouring cell placed anything.
    const std::uint64_t id =
        terrainSeedForLane(settings.seed, static_cast<std::uint64_t>(grid.indexAt(row, column)));
    std::uint64_t state = id;

    TerrainPoint point;
    const double easting = (column + terrainRandomUnitFloat(&state)) * grid.cellMeters;
    const double northing =
        settings.worldSizeMeters - (row + terrainRandomUnitFloat(&state)) * grid.cellMeters;
    const float wanted = sampleNearest(sources.density, settings, easting, northing);

    point.eastingMeters = static_cast<float>(easting);
    point.northingMeters = static_cast<float>(northing);
    point.altitudeMeters = sampleNearest(sources.surface, settings, easting, northing);
    isWanted = wanted >= settings.threshold && terrainRandomUnitFloat(&state) <= wanted;
    point.rotationDegrees = terrainRandomUnitFloat(&state) * 360.0f;
    point.scale = settings.scaleMinimum
                  + terrainRandomUnitFloat(&state)
                        * (settings.scaleMaximum - settings.scaleMinimum);
    point.id = id;
    return point;
}

void scatterRow(int row, const ScatterSettings& settings, const ScatterSources& sources,
                ScatterGrid& grid, std::vector<TerrainPoint>& placed) {
    for (int column = 0; column < grid.columns; ++column) {
        if (static_cast<std::int64_t>(placed.size()) >= settings.maximumCount) {
            return;
        }
        bool isWanted = false;
        const TerrainPoint point = candidateIn(row, column, settings, grid, sources, isWanted);
        if (!isWanted) {
            continue;
        }
        if (!standsClearOf(grid, placed, row, column, point.eastingMeters, point.northingMeters,
                           settings.spacingMeters)) {
            continue;
        }
        grid.occupant[grid.indexAt(row, column)] = static_cast<std::int64_t>(placed.size());
        placed.push_back(point);
    }
}

/**
 * Jittered-grid dart throwing: one candidate per background cell, in a fixed
 * order, accepted if the density allows it and nothing already stands within
 * the spacing.
 *
 * Sequential on purpose. Whether a point survives depends on which points were
 * accepted before it, so there is no division of this work that would give the
 * same answer — and an answer that changed with the thread count would not be
 * a scatter anyone could rebuild.
 */
void evaluateScatter(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    ScatterSettings settings;
    settings.size = api->width(context);
    settings.cellSizeMeters = api->cellSizeMeters(context);
    settings.worldSizeMeters = settings.size * settings.cellSizeMeters;
    settings.spacingMeters = static_cast<float>(api->parameterNumber(context, 0));
    settings.threshold = static_cast<float>(api->parameterNumber(context, 2));
    settings.scaleMinimum = static_cast<float>(api->parameterNumber(context, 3));
    settings.scaleMaximum = static_cast<float>(api->parameterNumber(context, 4));
    settings.maximumCount = static_cast<std::int64_t>(api->parameterNumber(context, 5));
    settings.seed = terrainSeedForLane(
        api->randomSeed(context),
        static_cast<std::uint64_t>(api->parameterNumber(context, 1)));

    const ScatterSources sources{api->inputField(context, 0), api->inputField(context, 1)};
    ScatterGrid grid = makeScatterGrid(settings);
    std::vector<TerrainPoint> placed;

    for (int row = 0; row < grid.columns; ++row) {
        scatterRow(row, settings, sources, grid, placed);
        api->reportProgress(context, static_cast<float>(row + 1) / grid.columns);
    }
    for (const TerrainPoint& point : placed) {
        api->emitPoint(context, 0, &point);
    }
}

const TerrainNodeDescriptor scatterDescriptor = {
    "Scatter",
    "Vegetation",
    "Places instances over the ground, no two closer than the spacing",
    1,
    scatterInputs,
    2,
    scatterOutputs,
    1,
    scatterParameters,
    6,
    &evaluateScatter};

}  // namespace

void registerVegetationNodes(const TerrainRegistryApi& api, TerrainRegistry* registry) {
    api.registerNode(registry, &growthMaskDescriptor);
    api.registerNode(registry, &scatterDescriptor);
}

}  // namespace terrain::nodes
