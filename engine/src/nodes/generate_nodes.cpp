#include <algorithm>
#include <cmath>

#include "node_support.h"
#include "shape_support.h"
#include "terrain/noise.h"

namespace terrain::nodes {
namespace {

/**
 * Generators sample in world meters measured from the graph's north-west
 * corner, never from the georeference. Anchoring to the georeference would
 * make a whole landscape slide the moment an import window moved, which is
 * not what a seed is for.
 */
struct NoiseFrame {
    int size = 0;
    float featureSizeMeters = 1.0f;
    double cellSizeMeters = 1.0;
    std::uint64_t seed = 0;

    float noiseX(int column) const {
        return static_cast<float>((column + 0.5) * cellSizeMeters) / featureSizeMeters;
    }
    float noiseY(int row) const {
        return static_cast<float>((row + 0.5) * cellSizeMeters) / featureSizeMeters;
    }
};

/// The seed parameter is a variation dial: it mixes with the per-node seed the
/// graph already derived, so rolling it changes one node and nothing else.
NoiseFrame frameFromContext(const TerrainContextApi* api, TerrainEvaluationContext* context,
                            int featureSizeIndex, int seedIndex) {
    NoiseFrame frame;
    frame.size = api->width(context);
    frame.cellSizeMeters = api->cellSizeMeters(context);
    frame.featureSizeMeters =
        std::max(static_cast<float>(api->parameterNumber(context, featureSizeIndex)), 1.0f);
    frame.seed = terrainSeedForLane(
        api->randomSeed(context),
        static_cast<std::uint64_t>(api->parameterNumber(context, seedIndex)));
    return frame;
}

noise::FractalOptions fractalFromContext(const TerrainContextApi* api,
                                         TerrainEvaluationContext* context, int firstIndex) {
    noise::FractalOptions options;
    options.octaves = static_cast<int>(api->parameterNumber(context, firstIndex));
    options.gain = static_cast<float>(api->parameterNumber(context, firstIndex + 1));
    options.lacunarity = static_cast<float>(api->parameterNumber(context, firstIndex + 2));
    return options;
}

/* -------------------------------------------------------------- Perlin --- */

const TerrainPortDescriptor noiseOutputs[] = {
    {"Out", "The generated field", TerrainPortType_Field, TerrainFieldSemantics_Heightfield, 0}};

const TerrainParameterDescriptor perlinParameters[] = {
    {"featureSize", "Feature size", "Ground distance between the largest bumps", "m",
     TerrainParameterType_Float, 2000.0, 1.0, 100000.0, "", nullptr, 0},
    {"seed", "Seed", "Varies this node alone, on top of the graph seed", "",
     TerrainParameterType_Seed, 0.0, 0.0, 4294967295.0, "", nullptr, 0},
    {"octaves", "Octaves", "How many times the detail is halved and repeated", "",
     TerrainParameterType_Int, 6.0, 1.0, 12.0, "", nullptr, 0},
    {"gain", "Gain", "How much of its amplitude each octave keeps", "",
     TerrainParameterType_Float, 0.5, 0.0, 1.0, "", nullptr, 0},
    {"lacunarity", "Lacunarity", "How much finer each octave is than the one before", "",
     TerrainParameterType_Float, 2.0, 1.0, 4.0, "", nullptr, 0},
    {"amplitude", "Amplitude", "Height of the tallest feature", "m", TerrainParameterType_Float,
     300.0, 0.0, 12000.0, "", nullptr, 0},
    {"offset", "Offset", "Added to every cell once the shape is generated", "m",
     TerrainParameterType_Float, 0.0, -12000.0, 12000.0, "", nullptr, 0}};

void evaluatePerlin(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const NoiseFrame frame = frameFromContext(api, context, 0, 1);
    const noise::FractalOptions options = fractalFromContext(api, context, 2);
    const float amplitude = static_cast<float>(api->parameterNumber(context, 5));
    const float offset = static_cast<float>(api->parameterNumber(context, 6));
    float* output = api->outputField(context, 0);

    forEachRowRange(api, context, frame.size, [&](int firstRow, int lastRow) {
        for (int row = firstRow; row < lastRow; ++row) {
            float* line = output + static_cast<std::size_t>(row) * frame.size;
            for (int column = 0; column < frame.size; ++column) {
                const float value = noise::fractalPerlin(frame.noiseX(column), frame.noiseY(row),
                                                         frame.seed, options);
                line[column] = value * amplitude + offset;
            }
        }
    });
}

const TerrainNodeDescriptor perlinDescriptor = {
    "Perlin",
    "Generate",
    "Layered gradient noise: the general-purpose rolling landscape",
    1,
    nullptr,
    0,
    noiseOutputs,
    1,
    perlinParameters,
    7,
    &evaluatePerlin};

/* --------------------------------------------------------- RidgedNoise --- */

const TerrainParameterDescriptor ridgedParameters[] = {
    {"featureSize", "Feature size", "Ground distance between the main ridges", "m",
     TerrainParameterType_Float, 4000.0, 1.0, 100000.0, "", nullptr, 0},
    {"seed", "Seed", "Varies this node alone, on top of the graph seed", "",
     TerrainParameterType_Seed, 0.0, 0.0, 4294967295.0, "", nullptr, 0},
    {"octaves", "Octaves", "How many times the detail is halved and repeated", "",
     TerrainParameterType_Int, 7.0, 1.0, 12.0, "", nullptr, 0},
    {"gain", "Gain", "How much of its amplitude each octave keeps", "",
     TerrainParameterType_Float, 0.5, 0.0, 1.0, "", nullptr, 0},
    {"lacunarity", "Lacunarity", "How much finer each octave is than the one before", "",
     TerrainParameterType_Float, 2.0, 1.0, 4.0, "", nullptr, 0},
    {"sharpness", "Sharpness", "Higher values narrow the ridge crests", "",
     TerrainParameterType_Float, 1.0, 0.1, 4.0, "", nullptr, 0},
    {"amplitude", "Amplitude", "Height from valley floor to the highest crest", "m",
     TerrainParameterType_Float, 800.0, 0.0, 12000.0, "", nullptr, 0},
    {"offset", "Offset", "Added to every cell once the shape is generated", "m",
     TerrainParameterType_Float, 0.0, -12000.0, 12000.0, "", nullptr, 0}};

void evaluateRidged(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const NoiseFrame frame = frameFromContext(api, context, 0, 1);
    const noise::FractalOptions options = fractalFromContext(api, context, 2);
    const float sharpness = static_cast<float>(api->parameterNumber(context, 5));
    const float amplitude = static_cast<float>(api->parameterNumber(context, 6));
    const float offset = static_cast<float>(api->parameterNumber(context, 7));
    float* output = api->outputField(context, 0);

    forEachRowRange(api, context, frame.size, [&](int firstRow, int lastRow) {
        for (int row = firstRow; row < lastRow; ++row) {
            float* line = output + static_cast<std::size_t>(row) * frame.size;
            for (int column = 0; column < frame.size; ++column) {
                const float value = noise::ridged(frame.noiseX(column), frame.noiseY(row),
                                                  frame.seed, options, sharpness);
                line[column] = value * amplitude + offset;
            }
        }
    });
}

const TerrainNodeDescriptor ridgedDescriptor = {
    "RidgedNoise",
    "Generate",
    "Folded noise that leaves sharp crests and rounded valleys",
    1,
    nullptr,
    0,
    noiseOutputs,
    1,
    ridgedParameters,
    8,
    &evaluateRidged};

/* -------------------------------------------------------------- Voronoi -- */

const TerrainEnumOption voronoiModes[] = {
    {"cells", "Cells", 0}, {"walls", "Walls", 1}, {"plates", "Plates", 2}};

const TerrainParameterDescriptor voronoiParameters[] = {
    {"featureSize", "Feature size", "Ground distance across one cell", "m",
     TerrainParameterType_Float, 1500.0, 1.0, 100000.0, "", nullptr, 0},
    {"seed", "Seed", "Varies this node alone, on top of the graph seed", "",
     TerrainParameterType_Seed, 0.0, 0.0, 4294967295.0, "", nullptr, 0},
    {"jitter", "Jitter", "How far each cell's centre may wander off the grid", "",
     TerrainParameterType_Float, 1.0, 0.0, 1.0, "", nullptr, 0},
    {"mode", "Mode", "Cells rise from their centres, walls stand along the seams, plates are flat",
     "", TerrainParameterType_Enum, 0.0, 0.0, 2.0, "", voronoiModes, 3},
    {"amplitude", "Amplitude", "Height of the pattern", "m", TerrainParameterType_Float, 200.0,
     0.0, 12000.0, "", nullptr, 0},
    {"offset", "Offset", "Added to every cell once the shape is generated", "m",
     TerrainParameterType_Float, 0.0, -12000.0, 12000.0, "", nullptr, 0}};

float voronoiValue(const noise::VoronoiSample& sample, int mode) {
    if (mode == 1) {
        return std::clamp(sample.secondDistance - sample.nearestDistance, 0.0f, 1.0f);
    }
    if (mode == 2) {
        return sample.cellValue;
    }
    return std::clamp(sample.nearestDistance, 0.0f, 1.0f);
}

void evaluateVoronoi(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const NoiseFrame frame = frameFromContext(api, context, 0, 1);
    const float jitter = static_cast<float>(api->parameterNumber(context, 2));
    const int mode = static_cast<int>(api->parameterNumber(context, 3));
    const float amplitude = static_cast<float>(api->parameterNumber(context, 4));
    const float offset = static_cast<float>(api->parameterNumber(context, 5));
    float* output = api->outputField(context, 0);

    forEachRowRange(api, context, frame.size, [&](int firstRow, int lastRow) {
        for (int row = firstRow; row < lastRow; ++row) {
            float* line = output + static_cast<std::size_t>(row) * frame.size;
            for (int column = 0; column < frame.size; ++column) {
                const noise::VoronoiSample sample =
                    noise::voronoi(frame.noiseX(column), frame.noiseY(row), frame.seed, jitter);
                line[column] = voronoiValue(sample, mode) * amplitude + offset;
            }
        }
    });
}

const TerrainNodeDescriptor voronoiDescriptor = {
    "Voronoi",
    "Generate",
    "Cellular noise: mesas, cracked plates, or the seams between them",
    1,
    nullptr,
    0,
    noiseOutputs,
    1,
    voronoiParameters,
    6,
    &evaluateVoronoi};

/* -------------------------------------------------------------- Pattern -- */

const TerrainEnumOption patternKinds[] = {{"checker", "Checker", 0},
                                          {"stripes", "Stripes", 1},
                                          {"grid", "Grid", 2},
                                          {"bricks", "Bricks", 3},
                                          {"dots", "Dots", 4}};

const TerrainParameterDescriptor patternParameters[] = {
    {"featureSize", "Feature size", "Ground distance across one tile of the pattern", "m",
     TerrainParameterType_Float, 500.0, 1.0, 100000.0, "", nullptr, 0},
    {"pattern", "Pattern", "Which repeating figure is drawn", "", TerrainParameterType_Enum, 0.0,
     0.0, 4.0, "", patternKinds, 5},
    {"rotation", "Rotation", "How far the pattern is turned clockwise", "deg",
     TerrainParameterType_Float, 0.0, 0.0, 360.0, "", nullptr, 0},
    {"lineWidth", "Line width", "Width of a grid or brick line, or the diameter of a dot", "m",
     TerrainParameterType_Float, 50.0, 0.0, 100000.0, "", nullptr, 0},
    {"softness", "Softness", "How far the edges of the figure fade out", "m",
     TerrainParameterType_Float, 10.0, 0.0, 100000.0, "", nullptr, 0},
    {"amplitude", "Amplitude", "Height of the raised part of the pattern", "m",
     TerrainParameterType_Float, 100.0, -12000.0, 12000.0, "", nullptr, 0},
    {"offset", "Offset", "Added to every cell once the pattern is drawn", "m",
     TerrainParameterType_Float, 0.0, -12000.0, 12000.0, "", nullptr, 0}};

void evaluatePattern(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const WorldFrame frame = worldFrameFor(api, context);
    const ShapePlacement placement =
        placementFrom(frame, 0.5, 0.5, api->parameterNumber(context, 2));
    const double featureSizeMeters = api->parameterNumber(context, 0);
    const auto kind = static_cast<PatternKind>(static_cast<int>(api->parameterNumber(context, 1)));
    const double lineWidthMeters = api->parameterNumber(context, 3);
    const double softnessMeters = api->parameterNumber(context, 4);
    const float amplitude = static_cast<float>(api->parameterNumber(context, 5));
    const float offset = static_cast<float>(api->parameterNumber(context, 6));
    float* output = api->outputField(context, 0);

    forEachRowRange(api, context, frame.size, [&](int firstRow, int lastRow) {
        for (int row = firstRow; row < lastRow; ++row) {
            float* line = output + static_cast<std::size_t>(row) * frame.size;
            for (int column = 0; column < frame.size; ++column) {
                const LocalPoint point =
                    placement.localPoint(frame.eastingAt(column), frame.northingAt(row));
                const double distance =
                    patternDistance(kind, point, featureSizeMeters, lineWidthMeters);
                line[column] = insideWeight(distance, softnessMeters) * amplitude + offset;
            }
        }
    });
}

const TerrainNodeDescriptor patternDescriptor = {
    "Pattern",
    "Generate",
    "Repeating geometry: checkers, stripes, a grid, brickwork or dots",
    1,
    nullptr,
    0,
    noiseOutputs,
    1,
    patternParameters,
    7,
    &evaluatePattern};

/* ---------------------------------------------------------------- Noise -- */

const TerrainParameterDescriptor whiteNoiseParameters[] = {
    {"seed", "Seed", "Varies this node alone, on top of the graph seed", "",
     TerrainParameterType_Seed, 0.0, 0.0, 4294967295.0, "", nullptr, 0},
    {"amplitude", "Amplitude", "How far a cell may stray either side of the offset", "m",
     TerrainParameterType_Float, 5.0, 0.0, 12000.0, "", nullptr, 0},
    {"offset", "Offset", "The value every cell varies around", "m", TerrainParameterType_Float,
     0.0, -12000.0, 12000.0, "", nullptr, 0}};

/**
 * One independent draw per cell, so unlike every other generator this one has
 * no feature size and its result changes with the graph resolution. That is
 * what makes it useful: it is the grain you multiply into a mask, not a shape.
 */
void evaluateWhiteNoise(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const int size = api->width(context);
    const std::uint64_t seed = terrainSeedForLane(
        api->randomSeed(context),
        static_cast<std::uint64_t>(api->parameterNumber(context, 0)));
    const float amplitude = static_cast<float>(api->parameterNumber(context, 1));
    const float offset = static_cast<float>(api->parameterNumber(context, 2));
    float* output = api->outputField(context, 0);

    forEachRowRange(api, context, size, [&](int firstRow, int lastRow) {
        for (int row = firstRow; row < lastRow; ++row) {
            float* line = output + static_cast<std::size_t>(row) * size;
            for (int column = 0; column < size; ++column) {
                std::uint64_t state = terrainSeedForLane(
                    seed, static_cast<std::uint64_t>(row) * size + column);
                const float unit = terrainRandomUnitFloat(&state);
                line[column] = (unit * 2.0f - 1.0f) * amplitude + offset;
            }
        }
    });
}

const TerrainNodeDescriptor whiteNoiseDescriptor = {
    "Noise",
    "Generate",
    "An independent draw in every cell: grain, rather than a landform",
    1,
    nullptr,
    0,
    noiseOutputs,
    1,
    whiteNoiseParameters,
    3,
    &evaluateWhiteNoise};

}  // namespace

void registerGenerateNodes(const TerrainRegistryApi& api, TerrainRegistry* registry) {
    api.registerNode(registry, &perlinDescriptor);
    api.registerNode(registry, &ridgedDescriptor);
    api.registerNode(registry, &voronoiDescriptor);
    api.registerNode(registry, &patternDescriptor);
    api.registerNode(registry, &whiteNoiseDescriptor);
}

}  // namespace terrain::nodes
