#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <numbers>
#include <vector>

#include "color_lookup_support.h"
#include "erosion_support.h"
#include "flow_support.h"
#include "node_support.h"
#include "terrain/noise.h"

namespace terrain::nodes {
namespace {

/* =================================================================== shared */

const TerrainPortDescriptor deriveInputs[] = {
    {"In", "The surface to read", TerrainPortType_Field, TerrainFieldSemantics_Heightfield, 0}};

const TerrainPortDescriptor deriveOutputs[] = {
    {"Out", "Weights in 0 to 1", TerrainPortType_Field, TerrainFieldSemantics_Mask, 0}};

/// The slope a derived mask already reads as wall. Past this the ground is as
/// steep as anything these nodes need to tell apart.
constexpr float steepestReadableSlopeDegrees = 60.0f;

/// How steep the ground is under one cell, as [0, 1].
float steepnessAt(const float* field, int size, int row, int column, double cellSizeMeters) {
    const float degrees = slopeDegrees(sampleGradient(field, size, row, column, cellSizeMeters));
    return smoothStep(degrees / steepestReadableSlopeDegrees);
}

std::size_t cellIndex(int size, int row, int column) {
    return static_cast<std::size_t>(row) * size + column;
}

int radiusInCells(double radiusMeters, double cellSizeMeters) {
    return std::max(1, static_cast<int>(std::lround(radiusMeters / cellSizeMeters)));
}

/// Scales a field against its own strongest cell and clips it into [0, 1].
void normaliseAgainstStrongest(std::vector<float>& values) {
    const FieldRange range = fieldRange(values.data(), values.size());
    const float strongest =
        std::max(std::max(std::abs(range.minimum), std::abs(range.maximum)), 1.0e-12f);
    for (float& value : values) {
        value = std::clamp(value / strongest, 0.0f, 1.0f);
    }
}

/// How much harder than the typical cell the ground has to bend before it
/// reads as fully hollow or fully raised.
constexpr float fullBendMultiple = 4.0f;

/**
 * Curvature over the whole grid, and the bend it is read against. Curvature is
 * in 1/m and its useful magnitude changes completely between a floodplain and
 * an arete, so a mask built from it is measured against this field's own
 * bending rather than against a number nobody could guess. The reference is a
 * multiple of the typical bend and not the single strongest cell, since one
 * spike on a summit would otherwise flatten every other reading to nothing.
 */
struct CurvatureField {
    std::vector<float> values;
    float reference = 1.0f;

    /// -1 in a deep hollow, +1 over a sharp rise.
    float signedBendAt(std::size_t index) const {
        return std::clamp(values[index] / reference, -1.0f, 1.0f);
    }
};

float typicalBendOf(const std::vector<float>& values) {
    double total = 0.0;
    for (const float value : values) {
        total += std::abs(value);
    }
    return static_cast<float>(total / std::max<std::size_t>(values.size(), 1));
}

CurvatureField curvatureOf(const TerrainContextApi* api, TerrainEvaluationContext* context,
                           const float* field) {
    const int size = api->width(context);
    const double cellSize = api->cellSizeMeters(context);
    CurvatureField curvature;
    curvature.values.resize(cellCount(api, context));

    // The Laplacian is positive where a cell sits below the ground around it,
    // so the sign is turned over: a bend reads positive over a rise.
    forEachRowRange(api, context, size, [&](int firstRow, int lastRow) {
        for (int row = firstRow; row < lastRow; ++row) {
            for (int column = 0; column < size; ++column) {
                curvature.values[cellIndex(size, row, column)] =
                    -sampleCurvature(field, size, row, column, cellSize);
            }
        }
    });

    curvature.reference =
        std::max(typicalBendOf(curvature.values) * fullBendMultiple, 1.0e-12f);
    return curvature;
}

/* ================================================================ box means */

/// One row or one column of the grid, as a view a running-sum pass can walk
/// without caring which way it runs.
struct GridLine {
    const float* source = nullptr;
    float* destination = nullptr;
    int count = 0;
    std::ptrdiff_t step = 1;
};

/// The window a box mean carries along a line. It shrinks at the ends, so an
/// edge cell averages the samples that exist rather than repeats of itself.
class BoxWindow {
public:
    BoxWindow(const GridLine& line, int radius)
        : line_(line), radius_(radius), last_(std::min(radius, line.count - 1)) {
        for (int index = 0; index <= last_; ++index) {
            total_ += sampleAt(index);
        }
    }

    /// Slides the window onto a new centre, taking in what came within reach
    /// and dropping what fell out of it.
    void advanceTo(int centre) {
        const int last = std::min(centre + radius_, line_.count - 1);
        const int first = std::max(centre - radius_, 0);
        for (int index = last_ + 1; index <= last; ++index) {
            total_ += sampleAt(index);
        }
        for (int index = first_; index < first; ++index) {
            total_ -= sampleAt(index);
        }
        first_ = first;
        last_ = last;
    }

    float mean() const { return static_cast<float>(total_ / (last_ - first_ + 1)); }

private:
    double sampleAt(int index) const { return line_.source[index * line_.step]; }

    GridLine line_;
    int radius_ = 1;
    int last_ = 0;
    int first_ = 0;
    double total_ = 0.0;
};

void boxMeanAlongLine(const GridLine& line, int radius) {
    BoxWindow window(line, radius);
    for (int index = 0; index < line.count; ++index) {
        window.advanceTo(index);
        line.destination[index * line.step] = window.mean();
    }
}

/**
 * The mean of the field over a square window, as two running-sum passes. Cost
 * is the same whatever the radius, which is what lets the radius be given in
 * meters and still be affordable on a fine graph.
 */
void localMean(const TerrainContextApi* api, TerrainEvaluationContext* context, const float* field,
               int radius, float* destination) {
    const int size = api->width(context);
    std::vector<float> alongRows(cellCount(api, context));

    forEachRowRange(api, context, size, [&](int firstRow, int lastRow) {
        for (int row = firstRow; row < lastRow; ++row) {
            const std::size_t start = cellIndex(size, row, 0);
            boxMeanAlongLine(GridLine{field + start, alongRows.data() + start, size, 1}, radius);
        }
    });
    forEachRowRange(api, context, size, [&](int firstColumn, int lastColumn) {
        for (int column = firstColumn; column < lastColumn; ++column) {
            boxMeanAlongLine(
                GridLine{alongRows.data() + column, destination + column, size, size}, radius);
        }
    });
}

/// How far each cell stands above the ground around it, in meters. Positive on
/// anything that protrudes, negative in anything recessed.
std::vector<float> protrusionOver(const TerrainContextApi* api, TerrainEvaluationContext* context,
                                  const float* field, int radius) {
    const std::size_t count = cellCount(api, context);
    std::vector<float> mean(count);
    localMean(api, context, field, radius, mean.data());

    std::vector<float> protrusion(count);
    for (std::size_t index = 0; index < count; ++index) {
        protrusion[index] = field[index] - mean[index];
    }
    return protrusion;
}

/* ============================================================= TextureBase */

const TerrainParameterDescriptor textureBaseParameters[] = {
    {"slope", "Slope", "How much steep ground shows through the texture", "",
     TerrainParameterType_Float, 0.5, 0.0, 1.0, "", nullptr, 0},
    {"flows", "Flows", "How much the paths water takes show through", "",
     TerrainParameterType_Float, 0.5, 0.0, 1.0, "", nullptr, 0},
    {"soil", "Soil", "How much the ground loose material settles on shows through", "",
     TerrainParameterType_Float, 0.3, 0.0, 1.0, "", nullptr, 0},
    {"patches", "Patches", "How strongly broad blotches break the distribution up", "",
     TerrainParameterType_Float, 0.3, 0.0, 1.0, "", nullptr, 0},
    {"chaos", "Chaos", "How strongly fine disorder breaks the distribution up", "",
     TerrainParameterType_Float, 0.3, 0.0, 1.0, "", nullptr, 0},
    {"peaks", "Peaks", "How much protruding ground shows through", "",
     TerrainParameterType_Float, 0.2, 0.0, 1.0, "", nullptr, 0},
    {"featureScale", "Feature scale", "Ground distance across one patch or protrusion", "m",
     TerrainParameterType_Float, 400.0, 10.0, 10000.0, "", nullptr, 0},
    {"enhance", "Enhance", "How the combined terms are spread over 0 to 1", "",
     TerrainParameterType_Enum, 1.0, 0.0, 2.0, "", lookupEnhanceOptions, 3},
    {"reverse", "Reverse", "Swaps the dark and the light ground", "", TerrainParameterType_Bool,
     0.0, 0.0, 1.0, "", nullptr, 0},
    {"seed", "Seed", "Varies this node alone, on top of the graph seed", "",
     TerrainParameterType_Seed, 0.0, 0.0, 4294967295.0, "", nullptr, 0}};

/// What every term of the texture base reads: the surface, and the frame the
/// noise terms are sampled on. Noise is sampled in world meters from the
/// north-west corner, so the patches keep their ground size at any resolution.
struct TextureBaseSource {
    const TerrainContextApi* api = nullptr;
    TerrainEvaluationContext* context = nullptr;
    const float* field = nullptr;
    int size = 0;
    double cellSizeMeters = 1.0;
    float featureScaleMeters = 400.0f;
    std::uint64_t seed = 0;

    std::size_t count() const { return cellCount(api, context); }

    float noiseX(int column, float scaleFactor) const {
        return static_cast<float>((column + 0.5) * cellSizeMeters)
               / (featureScaleMeters * scaleFactor);
    }
    float noiseY(int row, float scaleFactor) const {
        return static_cast<float>((row + 0.5) * cellSizeMeters)
               / (featureScaleMeters * scaleFactor);
    }
};

/// One weighted ingredient of the texture base, already on [0, 1].
struct TextureTerm {
    std::vector<float> values;
    float weight = 0.0f;
};

std::vector<float> slopeValues(const TextureBaseSource& source) {
    std::vector<float> values(source.count());
    forEachRowRange(source.api, source.context, source.size, [&](int firstRow, int lastRow) {
        for (int row = firstRow; row < lastRow; ++row) {
            for (int column = 0; column < source.size; ++column) {
                values[cellIndex(source.size, row, column)] =
                    steepnessAt(source.field, source.size, row, column, source.cellSizeMeters);
            }
        }
    });
    return values;
}

std::vector<float> flowValues(const TextureBaseSource& source) {
    constexpr float flowConcentration = 1.4f;
    constexpr float flowContrast = 1.0f;
    std::vector<float> accumulation(source.count());
    accumulateFlow(source.field, source.size, source.cellSizeMeters, flowConcentration,
                   accumulation.data());

    std::vector<float> values(source.count());
    normaliseFlow(accumulation.data(), accumulation.size(), flowContrast, values.data());
    return values;
}

/// Where loose material would come to rest: gentle ground, favouring anything
/// that already curves inwards, then smoothed so it reads as a deposit rather
/// than as a per-cell reading of the slope.
std::vector<float> soilValues(const TextureBaseSource& source, const CurvatureField& curvature) {
    constexpr float smoothingFraction = 0.25f;
    std::vector<float> settling(source.count());
    forEachRowRange(source.api, source.context, source.size, [&](int firstRow, int lastRow) {
        for (int row = firstRow; row < lastRow; ++row) {
            for (int column = 0; column < source.size; ++column) {
                const std::size_t index = cellIndex(source.size, row, column);
                const float gentle = 1.0f
                                     - steepnessAt(source.field, source.size, row, column,
                                                   source.cellSizeMeters);
                const float concave = std::max(-curvature.signedBendAt(index), 0.0f);
                settling[index] = gentle * (0.5f + 0.5f * concave);
            }
        }
    });

    std::vector<float> values(source.count());
    localMean(source.api, source.context, settling.data(),
              radiusInCells(source.featureScaleMeters * smoothingFraction, source.cellSizeMeters),
              values.data());
    return values;
}

/// Fractal Perlin folded into [0, 1]. The scale factor shrinks the feature
/// size, which is how the chaos term reads finer than the patches term.
std::vector<float> noiseValues(const TextureBaseSource& source, float scaleFactor, int octaves,
                               std::uint64_t salt) {
    noise::FractalOptions options;
    options.octaves = octaves;
    const std::uint64_t seed = terrainSeedForLane(source.seed, salt);

    std::vector<float> values(source.count());
    forEachRowRange(source.api, source.context, source.size, [&](int firstRow, int lastRow) {
        for (int row = firstRow; row < lastRow; ++row) {
            for (int column = 0; column < source.size; ++column) {
                const float sample =
                    noise::fractalPerlin(source.noiseX(column, scaleFactor),
                                         source.noiseY(row, scaleFactor), seed, options);
                values[cellIndex(source.size, row, column)] = 0.5f + 0.5f * sample;
            }
        }
    });
    return values;
}

std::vector<float> peakValues(const TextureBaseSource& source) {
    std::vector<float> values =
        protrusionOver(source.api, source.context, source.field,
                       radiusInCells(source.featureScaleMeters, source.cellSizeMeters));
    for (float& value : values) {
        value = std::max(value, 0.0f);
    }
    normaliseAgainstStrongest(values);
    return values;
}

/// The weighted average of the terms. A node with every weight at zero has
/// nothing to say, and says it as a flat field rather than as a division by
/// nothing.
void combineTerms(const std::vector<TextureTerm>& terms, std::size_t count, float* combined) {
    float totalWeight = 0.0f;
    for (const TextureTerm& term : terms) {
        totalWeight += term.weight;
    }
    std::fill(combined, combined + count, 0.0f);
    if (totalWeight <= 0.0f) {
        return;
    }
    for (const TextureTerm& term : terms) {
        for (std::size_t index = 0; index < count; ++index) {
            combined[index] += term.values[index] * term.weight / totalWeight;
        }
    }
}

TextureBaseSource textureBaseSourceFor(const TerrainContextApi* api,
                                       TerrainEvaluationContext* context) {
    TextureBaseSource source;
    source.api = api;
    source.context = context;
    source.field = api->inputField(context, 0);
    source.size = api->width(context);
    source.cellSizeMeters = api->cellSizeMeters(context);
    source.featureScaleMeters =
        std::max(static_cast<float>(api->parameterNumber(context, 6)), 1.0f);
    source.seed = terrainSeedForLane(api->randomSeed(context),
                                     static_cast<std::uint64_t>(api->parameterNumber(context, 9)));
    return source;
}

LookupSettings textureBaseLookupFor(const TerrainContextApi* api,
                                    TerrainEvaluationContext* context) {
    LookupSettings settings;
    settings.enhance =
        static_cast<LookupEnhance>(static_cast<int>(api->parameterNumber(context, 7)));
    settings.reverse = api->parameterNumber(context, 8) != 0.0;
    settings.bias = 0.0f;
    return settings;
}

/**
 * The material distribution a colour node is flowed through: the handful of
 * readings an artist would otherwise wire up by hand — steepness, the paths
 * water takes, where dust settles, broad blotches, fine disorder and what
 * protrudes — mixed in the proportions the dials ask for.
 */
void evaluateTextureBase(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    constexpr float chaosScaleFactor = 0.15f;
    const TextureBaseSource source = textureBaseSourceFor(api, context);
    const CurvatureField curvature = curvatureOf(api, context, source.field);

    std::vector<TextureTerm> terms;
    terms.push_back({slopeValues(source), static_cast<float>(api->parameterNumber(context, 0))});
    terms.push_back({flowValues(source), static_cast<float>(api->parameterNumber(context, 1))});
    terms.push_back(
        {soilValues(source, curvature), static_cast<float>(api->parameterNumber(context, 2))});
    terms.push_back(
        {noiseValues(source, 1.0f, 3, 1), static_cast<float>(api->parameterNumber(context, 3))});
    terms.push_back({noiseValues(source, chaosScaleFactor, 4, 2),
                     static_cast<float>(api->parameterNumber(context, 4))});
    terms.push_back({peakValues(source), static_cast<float>(api->parameterNumber(context, 5))});

    std::vector<float> combined(source.count());
    combineTerms(terms, combined.size(), combined.data());
    lookupPositions(combined.data(), combined.size(), textureBaseLookupFor(api, context),
                    api->outputField(context, 0));
}

const TerrainNodeDescriptor textureBaseDescriptor = {
    "TextureBase",
    "Mask",
    "A material distribution mask for a colour node to be flowed through",
    1,
    deriveInputs,
    1,
    deriveOutputs,
    1,
    textureBaseParameters,
    10,
    &evaluateTextureBase};

/* ================================================================= RockMap */

const TerrainParameterDescriptor rockMapParameters[] = {
    {"coverage", "Coverage", "How much of the ground the rock formations take up", "",
     TerrainParameterType_Float, 0.5, 0.0, 1.0, "", nullptr, 0},
    {"density", "Density", "How finely the formations are broken up", "",
     TerrainParameterType_Float, 0.5, 0.0, 1.0, "", nullptr, 0},
    {"featureScale", "Feature scale", "Ground distance across one rock formation", "m",
     TerrainParameterType_Float, 200.0, 10.0, 10000.0, "", nullptr, 0},
    {"slopeInfluence", "Slope influence",
     "How much the steep, outward-bulging ground decides where rock is exposed", "",
     TerrainParameterType_Float, 0.7, 0.0, 1.0, "", nullptr, 0},
    {"seed", "Seed", "Varies this node alone, on top of the graph seed", "",
     TerrainParameterType_Seed, 0.0, 0.0, 4294967295.0, "", nullptr, 0}};

/// How wide the crack between two neighbouring outcrops reads, in noise units.
constexpr float outcropWallWidth = 0.3f;
/// How far the coverage dial reaches past either end of the score, so 0 leaves
/// bare ground and 1 covers all of it.
constexpr float outcropEdgeWidth = 0.15f;

struct RockNoise {
    std::uint64_t cellSeed = 0;
    std::uint64_t grainSeed = 0;
    float formationScaleMeters = 200.0f;
    double cellSizeMeters = 1.0;

    float noiseX(int column) const {
        return static_cast<float>((column + 0.5) * cellSizeMeters) / formationScaleMeters;
    }
    float noiseY(int row) const {
        return static_cast<float>((row + 0.5) * cellSizeMeters) / formationScaleMeters;
    }
};

/// The outcrop the noise traces at one cell: the body of a cellular cell, away
/// from the cracks between cells, roughened so the outline is not a polygon
/// and dimmed by a value of the cell's own, so coverage has something to sort.
float formationAt(const RockNoise& rock, int row, int column) {
    constexpr float grainFrequency = 2.5f;
    noise::FractalOptions grainOptions;
    grainOptions.octaves = 3;

    const float x = rock.noiseX(column);
    const float y = rock.noiseY(row);
    const noise::VoronoiSample cell = noise::voronoi(x, y, rock.cellSeed, 0.9f);
    const float interior =
        smoothStep((cell.secondDistance - cell.nearestDistance) / outcropWallWidth);
    const float grain =
        noise::ridged(x * grainFrequency, y * grainFrequency, rock.grainSeed, grainOptions, 1.0f);
    return interior * (0.55f + 0.45f * grain) * (0.4f + 0.6f * cell.cellValue);
}

/// Where bare rock is exposed: steep ground that also bulges outwards, since a
/// convex face sheds whatever would otherwise settle over it.
float rockExposureAt(const float* field, int size, int row, int column, double cellSizeMeters,
                     const CurvatureField& curvature) {
    constexpr float steepnessShare = 0.7f;
    const float steep = steepnessAt(field, size, row, column, cellSizeMeters);
    const float convex = std::max(curvature.signedBendAt(cellIndex(size, row, column)), 0.0f);
    return std::clamp(steepnessShare * steep + (1.0f - steepnessShare) * convex, 0.0f, 1.0f);
}

/// The score a cell has to beat before it reads as rock.
float rockThresholdFor(float coverage) {
    return (1.0f - coverage) * (1.0f + 2.0f * outcropEdgeWidth) - outcropEdgeWidth;
}

RockNoise rockNoiseFor(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    constexpr float finestDensityFactor = 4.0f;
    const float density = static_cast<float>(api->parameterNumber(context, 1));
    const float featureScale =
        std::max(static_cast<float>(api->parameterNumber(context, 2)), 1.0f);
    const std::uint64_t seed = terrainSeedForLane(
        api->randomSeed(context), static_cast<std::uint64_t>(api->parameterNumber(context, 4)));

    RockNoise rock;
    rock.cellSizeMeters = api->cellSizeMeters(context);
    rock.formationScaleMeters = featureScale / (1.0f + (finestDensityFactor - 1.0f) * density);
    rock.cellSeed = terrainSeedForLane(seed, 1);
    rock.grainSeed = terrainSeedForLane(seed, 2);
    return rock;
}

/**
 * Rock coverage as an outline rather than as a slope reading: the exposed
 * ground decides where rock could be, and a cellular pattern cuts that into
 * the shapes of separate formations, so the mask traces outcrops instead of
 * shading every steep face evenly.
 */
void evaluateRockMap(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const int size = api->width(context);
    const double cellSize = api->cellSizeMeters(context);
    const float* input = api->inputField(context, 0);
    float* output = api->outputField(context, 0);
    const float coverage = static_cast<float>(api->parameterNumber(context, 0));
    const float slopeInfluence = static_cast<float>(api->parameterNumber(context, 3));
    const CurvatureField curvature = curvatureOf(api, context, input);
    const RockNoise rock = rockNoiseFor(api, context);

    std::vector<float> score(cellCount(api, context));
    forEachRowRange(api, context, size, [&](int firstRow, int lastRow) {
        for (int row = firstRow; row < lastRow; ++row) {
            for (int column = 0; column < size; ++column) {
                const float exposure =
                    rockExposureAt(input, size, row, column, cellSize, curvature);
                const float weighted = 1.0f - slopeInfluence + slopeInfluence * exposure;
                score[cellIndex(size, row, column)] = weighted * formationAt(rock, row, column);
            }
        }
    });

    normaliseAgainstStrongest(score);
    const float threshold = rockThresholdFor(coverage);
    for (std::size_t index = 0; index < score.size(); ++index) {
        output[index] =
            smoothStep((score[index] - threshold + outcropEdgeWidth) / (2.0f * outcropEdgeWidth));
    }
}

const TerrainNodeDescriptor rockMapDescriptor = {
    "RockMap",
    "Mask",
    "Traces the outlines of exposed rock formations across the surface",
    1,
    deriveInputs,
    1,
    deriveOutputs,
    1,
    rockMapParameters,
    5,
    &evaluateRockMap};

/* ==================================================================== Soil */

const TerrainParameterDescriptor soilParameters[] = {
    {"amount", "Amount", "How many settling cycles the dust goes through", "",
     TerrainParameterType_Float, 0.5, 0.0, 1.0, "", nullptr, 0},
    {"bias", "Bias", "Below zero gathers soil in the crevices, above it keeps to open ground", "",
     TerrainParameterType_Float, 0.0, -1.0, 1.0, "", nullptr, 0},
    {"settleAngle", "Settle angle", "The steepest slope soil rests on", "deg",
     TerrainParameterType_Float, 25.0, 0.0, 89.0, "", nullptr, 0},
    {"travel", "Travel", "How far soil slides before it comes to rest", "m",
     TerrainParameterType_Float, 150.0, 1.0, 1000.0, "", nullptr, 0}};

constexpr int mostSettleCycles = 8;

int settleCyclesFor(double amount) {
    const double cycles = 1.0 + std::clamp(amount, 0.0, 1.0) * (mostSettleCycles - 1);
    return static_cast<int>(std::lround(cycles));
}

/**
 * A layer thin enough that it cannot make the ground it lies on unstable, so
 * the mask traces the terrain rather than the soil's own build-up. The sliding
 * is then linear in the depth, which is what makes the normalised result
 * independent of the depth chosen here.
 */
float releaseDepthFor(double settleAngleDegrees, double cellSizeMeters) {
    const double radians = settleAngleDegrees * std::numbers::pi / 180.0;
    const double stableDrop = std::tan(radians) * cellSizeMeters;
    return static_cast<float>(std::max(0.25 * stableDrop, 0.01));
}

struct SoilSettings {
    double settleAngleDegrees = 25.0;
    double travelMeters = 150.0;
    float releaseDepthMeters = 1.0f;
    int cycleCount = 1;
};

/// Releases the layer over the first cycle and then lets it keep moving. Later
/// cycles add nothing, so a high amount sorts the same dust further rather
/// than piling more of it on.
void settleSoil(const TerrainContextApi* api, TerrainEvaluationContext* context, float* surface,
                float* layer, const SoilSettings& settings) {
    LayerSlump slump;
    slump.settleAngleDegrees = settings.settleAngleDegrees;
    slump.travelMeters = settings.travelMeters;
    slump.releasePerPassMeters =
        settings.releaseDepthMeters
        / passesToSpread(settings.travelMeters, api->cellSizeMeters(context));

    slumpLayer(api, context, surface, layer, slump);
    slump.releasePerPassMeters = 0.0f;
    for (int cycle = 1; cycle < settings.cycleCount; ++cycle) {
        slumpLayer(api, context, surface, layer, slump);
    }
}

/// Weights the settled depth by how the ground bends: a negative bias keeps
/// what gathered in the crevices, a positive one keeps only what lies on open,
/// unbent ground.
float biasWeightFor(float signedBend, float bias) {
    if (bias < 0.0f) {
        return std::clamp(1.0f + bias * signedBend, 0.0f, 2.0f);
    }
    return std::clamp(1.0f - bias * std::abs(signedBend), 0.0f, 1.0f);
}

void writeNormalisedDepth(const std::vector<float>& depth, float* destination) {
    const float deepest = *std::max_element(depth.begin(), depth.end());
    const float scale = 1.0f / std::max(deepest, 1.0e-6f);
    for (std::size_t index = 0; index < depth.size(); ++index) {
        destination[index] = std::clamp(depth[index] * scale, 0.0f, 1.0f);
    }
}

SoilSettings soilSettingsFor(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    SoilSettings settings;
    settings.cycleCount = settleCyclesFor(api->parameterNumber(context, 0));
    settings.settleAngleDegrees = api->parameterNumber(context, 2);
    settings.travelMeters = api->parameterNumber(context, 3);
    settings.releaseDepthMeters =
        releaseDepthFor(settings.settleAngleDegrees, api->cellSizeMeters(context));
    return settings;
}

/**
 * Dust is sprinkled evenly over the ground and then allowed to slide. Only the
 * dust moves, so it comes off anything steeper than the settle angle and
 * gathers wherever the ground already curves inwards — which finds the
 * crevices without anyone having to look for them first.
 */
void evaluateSoil(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const std::size_t count = cellCount(api, context);
    const float* input = api->inputField(context, 0);
    const float bias = static_cast<float>(api->parameterNumber(context, 1));
    const SoilSettings settings = soilSettingsFor(api, context);

    std::vector<float> surface(input, input + count);
    std::vector<float> depth(count, 0.0f);
    settleSoil(api, context, surface.data(), depth.data(), settings);

    const CurvatureField curvature = curvatureOf(api, context, input);
    for (std::size_t index = 0; index < count; ++index) {
        depth[index] *= biasWeightFor(curvature.signedBendAt(index), bias);
    }
    writeNormalisedDepth(depth, api->outputField(context, 0));
}

const TerrainNodeDescriptor soilDescriptor = {
    "Soil",
    "Mask",
    "Where loose material settles: dust sprinkled over the ground and left to slide",
    1,
    deriveInputs,
    1,
    deriveOutputs,
    1,
    soilParameters,
    4,
    &evaluateSoil};

/* =============================================================== Occlusion */

const TerrainParameterDescriptor occlusionParameters[] = {
    {"strength", "Strength", "How hard the mask leans towards the deeply enclosed ground", "",
     TerrainParameterType_Float, 0.5, 0.0, 1.0, "", nullptr, 0},
    {"octaves", "Octaves",
     "How many times the reach is doubled, so valleys register as well as crevices", "",
     TerrainParameterType_Int, 3.0, 1.0, 6.0, "", nullptr, 0},
    {"reach", "Reach", "How far the first octave looks for the horizon", "m",
     TerrainParameterType_Float, 50.0, 1.0, 20000.0, "", nullptr, 0},
    {"invert", "Invert", "Selects the exposed ground instead of the enclosed", "",
     TerrainParameterType_Bool, 0.0, 0.0, 1.0, "", nullptr, 0}};

/// A ray leaves in one of eight compass directions, as a unit step in grid
/// space. Row 0 is north, so a northward step counts down.
struct RayDirection {
    float rowStep = 0.0f;
    float columnStep = 0.0f;
};

constexpr float diagonalStep = 0.70710678f;
constexpr RayDirection azimuths[8] = {
    {-1.0f, 0.0f},
    {-diagonalStep, diagonalStep},
    {0.0f, 1.0f},
    {diagonalStep, diagonalStep},
    {1.0f, 0.0f},
    {diagonalStep, -diagonalStep},
    {0.0f, -1.0f},
    {-diagonalStep, -diagonalStep}};

/// At most this many samples per ray per octave, so doubling the reach costs
/// nothing extra once the strides are long enough to cover it.
constexpr int mostStepsPerRay = 64;

/// One octave of the scan: how far it looks, and how finely it samples on the
/// way out there.
struct HorizonScan {
    const float* field = nullptr;
    int size = 0;
    int stepCount = 1;
    float stepMeters = 1.0f;
    float stepInCells = 1.0f;
};

float sampleBilinear(const float* field, int size, float row, float column) {
    const float clampedRow = std::clamp(row, 0.0f, static_cast<float>(size - 1));
    const float clampedColumn = std::clamp(column, 0.0f, static_cast<float>(size - 1));
    const int lowRow = static_cast<int>(clampedRow);
    const int lowColumn = static_cast<int>(clampedColumn);
    const int highRow = std::min(lowRow + 1, size - 1);
    const int highColumn = std::min(lowColumn + 1, size - 1);
    const float rowFraction = clampedRow - lowRow;
    const float columnFraction = clampedColumn - lowColumn;

    const float northEdge = field[cellIndex(size, lowRow, lowColumn)] * (1.0f - columnFraction)
                            + field[cellIndex(size, lowRow, highColumn)] * columnFraction;
    const float southEdge = field[cellIndex(size, highRow, lowColumn)] * (1.0f - columnFraction)
                            + field[cellIndex(size, highRow, highColumn)] * columnFraction;
    return northEdge * (1.0f - rowFraction) + southEdge * rowFraction;
}

/// The sine of the highest elevation angle seen along one ray: 0 where the
/// ground only ever falls away, approaching 1 against a wall.
float horizonSineAlong(const HorizonScan& scan, int row, int column,
                       const RayDirection& direction) {
    const float origin = scan.field[cellIndex(scan.size, row, column)];
    float highestTangent = 0.0f;
    for (int step = 1; step <= scan.stepCount; ++step) {
        const float reachInCells = step * scan.stepInCells;
        const float rise = sampleBilinear(scan.field, scan.size,
                                          row + direction.rowStep * reachInCells,
                                          column + direction.columnStep * reachInCells)
                           - origin;
        highestTangent = std::max(highestTangent, rise / (step * scan.stepMeters));
    }
    return highestTangent / std::sqrt(1.0f + highestTangent * highestTangent);
}

float meanHorizonSine(const HorizonScan& scan, int row, int column) {
    float total = 0.0f;
    for (const RayDirection& direction : azimuths) {
        total += horizonSineAlong(scan, row, column, direction);
    }
    return total / std::size(azimuths);
}

/// Each octave doubles the reach and halves its say, so a crevice a few meters
/// across and a valley a kilometre wide both register in one mask.
float occlusionAt(const std::vector<HorizonScan>& octaves, int row, int column) {
    float total = 0.0f;
    float totalWeight = 0.0f;
    for (std::size_t octave = 0; octave < octaves.size(); ++octave) {
        const float weight = 1.0f / static_cast<float>(1 << octave);
        total += weight * meanHorizonSine(octaves[octave], row, column);
        totalWeight += weight;
    }
    return total / totalWeight;
}

std::vector<HorizonScan> horizonScansFor(const TerrainContextApi* api,
                                         TerrainEvaluationContext* context) {
    const double cellSize = api->cellSizeMeters(context);
    const int octaveCount = static_cast<int>(api->parameterNumber(context, 1));
    const double firstReachMeters = api->parameterNumber(context, 2);

    std::vector<HorizonScan> octaves;
    for (int octave = 0; octave < octaveCount; ++octave) {
        const double reachMeters = firstReachMeters * (1 << octave);
        HorizonScan scan;
        scan.field = api->inputField(context, 0);
        scan.size = api->width(context);
        scan.stepCount = std::clamp(static_cast<int>(std::lround(reachMeters / cellSize)), 1,
                                    mostStepsPerRay);
        scan.stepMeters = static_cast<float>(reachMeters / scan.stepCount);
        scan.stepInCells = static_cast<float>(scan.stepMeters / cellSize);
        octaves.push_back(scan);
    }
    return octaves;
}

/// A contrast exponent from the dial: a strong setting pushes the middling
/// ground up towards the deeply enclosed, a weak one holds it back.
float occlusionExponentFor(float strength) {
    return std::exp2(2.0f - 4.0f * std::clamp(strength, 0.0f, 1.0f));
}

void invertMask(float* values, std::size_t count) {
    for (std::size_t index = 0; index < count; ++index) {
        values[index] = 1.0f - values[index];
    }
}

/**
 * Ambient occlusion read for sediment rather than for light: how much sky a
 * cell can still see, as the mean horizon angle around it. A crevice comes out
 * at 1 and an exposed ridge at 0, which is the ground material collects on and
 * the ground it is stripped from.
 */
void evaluateOcclusion(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const int size = api->width(context);
    float* output = api->outputField(context, 0);
    const float exponent =
        occlusionExponentFor(static_cast<float>(api->parameterNumber(context, 0)));
    const bool invert = api->parameterNumber(context, 3) != 0.0;
    const std::vector<HorizonScan> octaves = horizonScansFor(api, context);

    forEachRowRange(api, context, size, [&](int firstRow, int lastRow) {
        for (int row = firstRow; row < lastRow; ++row) {
            for (int column = 0; column < size; ++column) {
                const float enclosed = std::clamp(occlusionAt(octaves, row, column), 0.0f, 1.0f);
                output[cellIndex(size, row, column)] = std::pow(enclosed, exponent);
            }
        }
    });
    if (invert) {
        invertMask(output, cellCount(api, context));
    }
}

const TerrainNodeDescriptor occlusionDescriptor = {
    "Occlusion",
    "Mask",
    "How enclosed each cell is, from the horizon seen around it: crevices and hollows first",
    1,
    deriveInputs,
    1,
    deriveOutputs,
    1,
    occlusionParameters,
    4,
    &evaluateOcclusion};

/* =================================================================== Peaks */

const TerrainParameterDescriptor peaksParameters[] = {
    {"falloff", "Falloff", "Higher values gather the mask onto the strongest protrusions alone",
     "", TerrainParameterType_Float, 0.5, 0.0, 1.0, "", nullptr, 0},
    {"precise", "Precise", "Reads over a shorter distance, so narrow crest lines are tracked", "",
     TerrainParameterType_Bool, 0.0, 0.0, 1.0, "", nullptr, 0},
    {"scale", "Scale", "Ground distance the surrounding height is averaged over", "m",
     TerrainParameterType_Float, 300.0, 1.0, 20000.0, "", nullptr, 0}};

/// How much of the scale a precise reading uses. A shorter average follows the
/// tips closely, and is correspondingly sensitive to fine surface noise.
constexpr double preciseScaleFraction = 0.35;

double peaksScaleFor(double scaleMeters, bool precise) {
    if (precise) {
        return scaleMeters * preciseScaleFraction;
    }
    return scaleMeters;
}

/// The exponent the falloff dial asks for: below 1 it spreads the mask down
/// the ridges, above 1 it pulls it back onto the tips.
float peaksExponentFor(float falloff) {
    return std::exp2(4.0f * std::clamp(falloff, 0.0f, 1.0f) - 2.0f);
}

/**
 * Protrusion, which is inverse curvature read at a chosen scale: how far a
 * cell stands above the ground around it. Unlike an altitude band this finds
 * the tips of two mountain groups that sit at quite different elevations.
 */
void evaluatePeaks(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const float* input = api->inputField(context, 0);
    float* output = api->outputField(context, 0);
    const float exponent = peaksExponentFor(static_cast<float>(api->parameterNumber(context, 0)));
    const bool precise = api->parameterNumber(context, 1) != 0.0;
    const double scaleMeters = peaksScaleFor(api->parameterNumber(context, 2), precise);

    std::vector<float> protrusion = protrusionOver(
        api, context, input, radiusInCells(scaleMeters, api->cellSizeMeters(context)));
    for (float& value : protrusion) {
        value = std::max(value, 0.0f);
    }
    normaliseAgainstStrongest(protrusion);
    for (std::size_t index = 0; index < protrusion.size(); ++index) {
        output[index] = std::pow(protrusion[index], exponent);
    }
}

const TerrainNodeDescriptor peaksDescriptor = {
    "Peaks",
    "Mask",
    "Selects summits and crest lines by how far they protrude, whatever altitude they sit at",
    1,
    deriveInputs,
    1,
    deriveOutputs,
    1,
    peaksParameters,
    3,
    &evaluatePeaks};

}  // namespace

void registerDeriveNodes(const TerrainRegistryApi& api, TerrainRegistry* registry) {
    api.registerNode(registry, &textureBaseDescriptor);
    api.registerNode(registry, &rockMapDescriptor);
    api.registerNode(registry, &soilDescriptor);
    api.registerNode(registry, &occlusionDescriptor);
    api.registerNode(registry, &peaksDescriptor);
}

}  // namespace terrain::nodes
