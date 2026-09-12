#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <vector>

#include "color_lookup_support.h"
#include "erosion_support.h"
#include "flow_support.h"
#include "node_support.h"
#include "terrain/noise.h"

namespace terrain::nodes {
namespace {

/* ================================================================= shared == */

/// A band that has no upper end: everything from the snow line upwards.
constexpr float aboveEverythingMeters = 1.0e9f;
/// Ground gentler than this faces nowhere in particular, so neither the sun
/// nor the wind can tell one side of it from the other.
constexpr float facelessSlopeDegrees = 5.0f;
constexpr float degreesToRadians = std::numbers::pi_v<float> / 180.0f;

/// Compass bearing the ground looks towards: downhill, so the opposite of the
/// uphill gradient. 0 is north, 90 east.
float aspectDegrees(const SurfaceGradient& gradient) {
    const float radians = std::atan2(-gradient.eastward, -gradient.northward);
    const float degrees = radians / degreesToRadians;
    if (degrees < 0.0f) {
        return degrees + 360.0f;
    }
    return degrees;
}

/// How squarely a slope looks towards a bearing: 1 face on, 0 turned away,
/// and 0.5 wherever the ground is too flat to face anywhere.
float facingShare(const SurfaceGradient& gradient, float bearingDegrees) {
    const float difference = (aspectDegrees(gradient) - bearingDegrees) * degreesToRadians;
    const float steepEnough = smoothStep(slopeDegrees(gradient) / facelessSlopeDegrees);
    return 0.5f + 0.5f * std::cos(difference) * steepEnough;
}

/// An optional mask reads as full effect where it is not connected.
float maskAt(const float* mask, std::size_t index) {
    if (mask == nullptr) {
        return 1.0f;
    }
    return std::clamp(mask[index], 0.0f, 1.0f);
}

float valueAt(const std::vector<float>& field, std::size_t index) {
    if (field.empty()) {
        return 0.0f;
    }
    return field[index];
}

std::size_t cellIndex(int size, int row, int column) {
    return static_cast<std::size_t>(row) * size + column;
}

/// Snow depth as cover: a cell holding a whole winter's fall reads as one, a
/// deeper drift no more than that. Scaling against the deepest drift instead
/// would leave everything but a few hollows near zero.
void writeSnowCover(const std::vector<float>& depth, float fullDepthMeters, float* destination) {
    const float scale = 1.0f / std::max(fullDepthMeters, 1.0e-6f);
    for (std::size_t index = 0; index < depth.size(); ++index) {
        destination[index] = std::clamp(depth[index] * scale, 0.0f, 1.0f);
    }
}

void writeSnowSurface(const float* ground, const std::vector<float>& depth, float* destination) {
    for (std::size_t index = 0; index < depth.size(); ++index) {
        destination[index] = ground[index] + depth[index];
    }
}

/* =============================================================== snowfall == */

/**
 * What decides how much of a snowfall a cell takes, all of it read from the
 * ground the snow lands on rather than from the snow already lying there.
 */
struct SnowfallShape {
    int size = 0;
    double cellSizeMeters = 1.0;
    float snowLineMeters = 1500.0f;
    float snowLineFalloffMeters = 300.0f;
    float slipOffAngleDegrees = 38.0f;
    /// The share of a snowfall a wall still takes.
    float verticality = 0.2f;
    /// Where the falling snow is driven. Negative for snow that comes straight
    /// down.
    float driftBearingDegrees = -1.0f;
};

/// How much of a snowfall the wind takes off the faces turned away from where
/// it is driving the snow.
constexpr float driftLoss = 0.5f;

float driftWeight(const SnowfallShape& shape, const SurfaceGradient& gradient) {
    if (shape.driftBearingDegrees < 0.0f) {
        return 1.0f;
    }
    return 1.0f - driftLoss * (1.0f - facingShare(gradient, shape.driftBearingDegrees));
}

float snowfallWeightAt(const SnowfallShape& shape, const float* ground, int row, int column) {
    const std::size_t index = cellIndex(shape.size, row, column);
    const SurfaceGradient gradient =
        sampleGradient(ground, shape.size, row, column, shape.cellSizeMeters);
    const float altitude = bandWeight(ground[index], shape.snowLineMeters, aboveEverythingMeters,
                                      shape.snowLineFalloffMeters);
    // The fade runs from the slip-off angle to vertical, so merely steep ground
    // still takes most of a snowfall and only a wall is left with the
    // verticality share.
    const float holding = bandWeight(slopeDegrees(gradient), 0.0f, shape.slipOffAngleDegrees,
                                     std::max(90.0f - shape.slipOffAngleDegrees, 1.0f));
    const float slope = shape.verticality + (1.0f - shape.verticality) * holding;
    return altitude * slope * driftWeight(shape, gradient);
}

/**
 * The share of every snowfall each cell takes, worked out once. Snow falls on
 * the terrain; what happens to it after it lands is the settling's business,
 * so there is nothing to gain from re-reading a surface that differs only by
 * the snow already on it.
 */
std::vector<float> snowfallWeights(const TerrainContextApi* api,
                                   TerrainEvaluationContext* context, const SnowfallShape& shape,
                                   const float* mask) {
    const float* ground = api->inputField(context, 0);
    std::vector<float> weights(cellCount(api, context), 0.0f);
    forEachRowRange(api, context, shape.size, [&](int firstRow, int lastRow) {
        for (int row = firstRow; row < lastRow; ++row) {
            for (int column = 0; column < shape.size; ++column) {
                const std::size_t index = cellIndex(shape.size, row, column);
                weights[index] =
                    snowfallWeightAt(shape, ground, row, column) * maskAt(mask, index);
            }
        }
    });
    return weights;
}

void addSnowfall(std::vector<float>& depth, const std::vector<float>& weights,
                 float depthPerPassMeters) {
    for (std::size_t index = 0; index < depth.size(); ++index) {
        depth[index] += weights[index] * depthPerPassMeters;
    }
}

/* =============================================================== settling == */

struct SnowSettling {
    float slipOffAngleDegrees = 38.0f;
    float adheredSnowMassMeters = 0.3f;
    float settleDurationMeters = 60.0f;
};

/// Buffers the settling reuses, so a long simulation does not reallocate the
/// whole grid on every pass.
struct SettleScratch {
    std::vector<float> mobile;
    std::vector<float> surface;

    explicit SettleScratch(std::size_t count) : mobile(count, 0.0f), surface(count, 0.0f) {}
};

/**
 * Lets the snow slide over the ground until nothing lies steeper than the
 * slip-off angle. The first `adheredSnowMass` meters are held out of the slide
 * and put back afterwards, so a steep face keeps a crust instead of being
 * swept to bare rock.
 */
void settleSnow(const TerrainContextApi* api, TerrainEvaluationContext* context,
                const float* ground, const SnowSettling& settling, std::vector<float>& depth,
                SettleScratch& scratch) {
    for (std::size_t index = 0; index < depth.size(); ++index) {
        scratch.mobile[index] = std::max(depth[index] - settling.adheredSnowMassMeters, 0.0f);
        scratch.surface[index] = ground[index] + depth[index];
    }

    LayerSlump slump;
    slump.settleAngleDegrees = settling.slipOffAngleDegrees;
    slump.travelMeters = settling.settleDurationMeters;
    slumpLayer(api, context, scratch.surface.data(), scratch.mobile.data(), slump);

    for (std::size_t index = 0; index < depth.size(); ++index) {
        depth[index] =
            std::min(depth[index], settling.adheredSnowMassMeters) + scratch.mobile[index];
    }
}

/* =================================================================== melt == */

/// Salts keep two draws on the same cell and seed from coming out equal.
constexpr std::uint64_t remnantGrainSalt = 1;
const noise::FractalOptions remnantOctaves{3, 0.5f, 2.0f};
constexpr std::uint64_t grittyGrainSalt = 2;

struct SnowMelt {
    int size = 0;
    double cellSizeMeters = 1.0;
    float share = 0.0f;
    bool isDirectional = false;
    float sunBearingDegrees = 180.0f;
    float remnants = 0.0f;
    float patchFeatureSizeMeters = 240.0f;
    std::uint64_t seed = 0;
};

/// The pattern surviving patches are cut from: low-frequency shapes in world
/// meters, roughened cell by cell so a patch edge is ragged rather than a
/// smooth curve.
float remnantPattern(const SnowMelt& melt, int row, int column) {
    const float x =
        static_cast<float>((column + 0.5) * melt.cellSizeMeters) / melt.patchFeatureSizeMeters;
    const float y =
        static_cast<float>((row + 0.5) * melt.cellSizeMeters) / melt.patchFeatureSizeMeters;
    const float shape = 0.5f + 0.5f * noise::fractalPerlin(x, y, melt.seed, remnantOctaves);
    const float grain = cellNoise(melt.seed, cellIndex(melt.size, row, column), remnantGrainSalt);
    return 0.75f * shape + 0.25f * grain;
}

/// How sharply a surviving patch gives way to bare ground, in pattern units.
constexpr float remnantEdgeWidth = 0.15f;

/// The share of a cell's snow the melt spares. Remnants of 0 melt everything
/// the sun reaches; remnants of 1 leave it all.
float meltSurvival(const SnowMelt& melt, int row, int column) {
    if (melt.remnants <= 0.0f) {
        return 0.0f;
    }
    return smoothStep((melt.remnants - remnantPattern(melt, row, column)) / remnantEdgeWidth);
}

float meltShare(const SnowMelt& melt, const SurfaceGradient& gradient) {
    if (!melt.isDirectional) {
        return melt.share;
    }
    return melt.share * facingShare(gradient, melt.sunBearingDegrees);
}

float meltedDepth(const SnowMelt& melt, const float* ground, float depthMeters, int row,
                  int column) {
    const SurfaceGradient gradient =
        sampleGradient(ground, melt.size, row, column, melt.cellSizeMeters);
    const float lost = meltShare(melt, gradient) * (1.0f - meltSurvival(melt, row, column));
    return depthMeters * (1.0f - lost);
}

void meltSnow(const TerrainContextApi* api, TerrainEvaluationContext* context,
              const float* ground, const SnowMelt& melt, std::vector<float>& depth) {
    if (melt.share <= 0.0f) {
        return;
    }
    forEachRowRange(api, context, melt.size, [&](int firstRow, int lastRow) {
        for (int row = firstRow; row < lastRow; ++row) {
            for (int column = 0; column < melt.size; ++column) {
                const std::size_t index = cellIndex(melt.size, row, column);
                depth[index] = meltedDepth(melt, ground, depth[index], row, column);
            }
        }
    });
}

/* =================================================================== Snow == */

const TerrainPortDescriptor snowInputs[] = {
    {"In", "The ground the snow falls on", TerrainPortType_Field,
     TerrainFieldSemantics_Heightfield, 0},
    {"Mask", "Scales where snow is allowed to fall", TerrainPortType_Field,
     TerrainFieldSemantics_Mask, 1}};

const TerrainPortDescriptor snowOutputs[] = {
    {"Out", "The ground with the settled snow on it", TerrainPortType_Field,
     TerrainFieldSemantics_Heightfield, 0},
    {"Snow", "Snow cover: one where a whole winter's fall lies, less where it slid or thawed",
     TerrainPortType_Field, TerrainFieldSemantics_Mask, 0},
    {"Depth", "How deep the snow ended up, in meters", TerrainPortType_Field,
     TerrainFieldSemantics_Mask, 0}};

const TerrainEnumOption meltTypes[] = {{"uniform", "Uniform", 0},
                                       {"directional", "Directional", 1}};

const TerrainParameterDescriptor snowParameters[] = {
    {"duration", "Duration", "How many falls of snow the winter is divided into", "",
     TerrainParameterType_Int, 12.0, 1.0, 200.0, "", nullptr, 0},
    {"intensity", "Intensity", "Total depth of snow that falls over the winter", "m",
     TerrainParameterType_Float, 3.0, 0.0, 100.0, "", nullptr, 0},
    {"settleDuration", "Settle duration", "How far snow creeps downhill before it comes to rest",
     "m", TerrainParameterType_Float, 60.0, 1.0, 2000.0, "", nullptr, 0},
    {"meltType", "Melt type", "Whether the thaw is even everywhere or follows the sun", "",
     TerrainParameterType_Enum, 0.0, 0.0, 1.0, "", meltTypes, 2},
    {"melt", "Melt", "How much of the settled snow the thaw takes away", "",
     TerrainParameterType_Float, 0.2, 0.0, 1.0, "", nullptr, 0},
    {"meltRemnants", "Melt remnants", "How much of the melted snow survives as ragged patches",
     "", TerrainParameterType_Float, 0.3, 0.0, 1.0, "", nullptr, 0},
    {"direction", "Sun direction", "Compass bearing the sun comes from; 180 is south", "deg",
     TerrainParameterType_Float, 180.0, 0.0, 360.0, "", nullptr, 0},
    {"snowLine", "Snow line", "No snow settles below this altitude", "m",
     TerrainParameterType_Float, 1500.0, -12000.0, 12000.0, "", nullptr, 0},
    {"snowLineFalloff", "Snow line falloff", "How gradually the snow line gives way to bare ground",
     "m", TerrainParameterType_Float, 300.0, 0.0, 5000.0, "", nullptr, 0},
    {"slipOffAngle", "Slip off angle", "The steepest slope settled snow will hold on", "deg",
     TerrainParameterType_Float, 38.0, 0.0, 89.0, "", nullptr, 0},
    {"adheredSnowMass", "Adhered snow mass",
     "Depth of the crust that clings on however steep the face", "m",
     TerrainParameterType_Float, 0.3, 0.0, 20.0, "", nullptr, 0},
    {"verticality", "Verticality", "The share of a snowfall a sheer face still catches", "",
     TerrainParameterType_Float, 0.2, 0.0, 1.0, "", nullptr, 0},
    {"seed", "Seed", "Varies the melt patches alone, on top of the graph seed", "",
     TerrainParameterType_Seed, 0.0, 0.0, 4294967295.0, "", nullptr, 0}};

/// Everything one Snow evaluation needs, read off the parameters in the order
/// the descriptor above declares them.
struct SnowRun {
    SnowfallShape shape;
    SnowSettling settling;
    SnowMelt melt;
    int duration = 12;
    float intensityMeters = 3.0f;
};

std::uint64_t seedFromParameter(const TerrainContextApi* api, TerrainEvaluationContext* context,
                                int parameterIndex) {
    return terrainSeedForLane(
        api->randomSeed(context),
        static_cast<std::uint64_t>(api->parameterNumber(context, parameterIndex)));
}

SnowRun readSnowRun(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    SnowRun run;
    run.duration = std::max(static_cast<int>(api->parameterNumber(context, 0)), 1);
    run.intensityMeters = static_cast<float>(api->parameterNumber(context, 1));
    run.settling.settleDurationMeters = static_cast<float>(api->parameterNumber(context, 2));
    run.melt.isDirectional = api->parameterNumber(context, 3) >= 0.5;
    run.melt.share = static_cast<float>(api->parameterNumber(context, 4));
    run.melt.remnants = static_cast<float>(api->parameterNumber(context, 5));
    run.melt.sunBearingDegrees = static_cast<float>(api->parameterNumber(context, 6));
    run.shape.snowLineMeters = static_cast<float>(api->parameterNumber(context, 7));
    run.shape.snowLineFalloffMeters = static_cast<float>(api->parameterNumber(context, 8));
    run.shape.slipOffAngleDegrees = static_cast<float>(api->parameterNumber(context, 9));
    run.settling.slipOffAngleDegrees = run.shape.slipOffAngleDegrees;
    run.settling.adheredSnowMassMeters = static_cast<float>(api->parameterNumber(context, 10));
    run.shape.verticality = static_cast<float>(api->parameterNumber(context, 11));
    run.melt.seed = seedFromParameter(api, context, 12);

    run.shape.size = api->width(context);
    run.shape.cellSizeMeters = api->cellSizeMeters(context);
    run.melt.size = run.shape.size;
    run.melt.cellSizeMeters = run.shape.cellSizeMeters;
    // Patches the size of a few slides read as thaw rather than as noise.
    run.melt.patchFeatureSizeMeters = std::max(run.settling.settleDurationMeters * 4.0f, 1.0f);
    return run;
}

/**
 * A winter of snowfall: each pass lays down its share of the total, and the
 * whole layer is then allowed to slide over the ground it is lying on. Falling
 * and settling alternate rather than running one after the other, because snow
 * that has already slid into a gully is what the next fall lands on.
 */
void evaluateSnow(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const std::size_t count = cellCount(api, context);
    const float* ground = api->inputField(context, 0);
    const SnowRun run = readSnowRun(api, context);
    const std::vector<float> weights =
        snowfallWeights(api, context, run.shape, api->inputField(context, 1));

    std::vector<float> depth(count, 0.0f);
    SettleScratch scratch(count);
    for (int pass = 0; pass < run.duration; ++pass) {
        addSnowfall(depth, weights, run.intensityMeters / run.duration);
        settleSnow(api, context, ground, run.settling, depth, scratch);
        api->reportProgress(context, static_cast<float>(pass + 1) / run.duration);
    }
    meltSnow(api, context, ground, run.melt, depth);

    writeSnowSurface(ground, depth, api->outputField(context, 0));
    writeSnowCover(depth, run.intensityMeters, api->outputField(context, 1));
    std::copy(depth.begin(), depth.end(), api->outputField(context, 2));
}

const TerrainNodeDescriptor snowDescriptor = {
    "Snow",
    "Simulate",
    "Drops a winter of snow, lets it slide off the steep ground and thaws what the sun reaches",
    2,
    snowInputs,
    2,
    snowOutputs,
    3,
    snowParameters,
    13,
    &evaluateSnow};

/* ============================================================== Snowfield == */

const TerrainPortDescriptor snowfieldOutputs[] = {
    {"Out", "The ground with the settled snow on it", TerrainPortType_Field,
     TerrainFieldSemantics_Heightfield, 0},
    {"Snow", "Snow cover: one where every spell's fall lies, less where it slid or thawed",
     TerrainPortType_Field, TerrainFieldSemantics_Mask, 0}};

const TerrainEnumOption driftDirections[] = {{"none", "None", 0},
                                             {"north", "North", 1},
                                             {"east", "East", 2},
                                             {"south", "South", 3},
                                             {"west", "West", 4}};

const TerrainParameterDescriptor snowfieldParameters[] = {
    {"cascades", "Cascades", "How many separate spells of snowfall the season brings", "",
     TerrainParameterType_Int, 3.0, 1.0, 20.0, "", nullptr, 0},
    {"duration", "Duration", "How many falls of snow each spell is divided into", "",
     TerrainParameterType_Int, 8.0, 1.0, 200.0, "", nullptr, 0},
    {"intensity", "Intensity", "Depth of snow one spell brings", "m",
     TerrainParameterType_Float, 2.0, 0.0, 100.0, "", nullptr, 0},
    {"taperSnowfall", "Taper snowfall", "Lets each later spell bring less snow than the one before",
     "", TerrainParameterType_Bool, 0.0, 0.0, 1.0, "", nullptr, 0},
    {"taperAmount", "Taper amount", "How much of the snowfall each later spell loses", "",
     TerrainParameterType_Float, 0.3, 0.0, 1.0, "", nullptr, 0},
    {"settleDuration", "Settle duration", "How far snow creeps downhill before it comes to rest",
     "m", TerrainParameterType_Float, 60.0, 1.0, 2000.0, "", nullptr, 0},
    {"melt", "Melt", "How much of the settled snow the thaw takes away, evenly", "",
     TerrainParameterType_Float, 0.1, 0.0, 1.0, "", nullptr, 0},
    {"snowLine", "Snow line", "No snow settles below this altitude", "m",
     TerrainParameterType_Float, 1500.0, -12000.0, 12000.0, "", nullptr, 0},
    {"snowLineFalloff", "Snow line falloff", "How gradually the snow line gives way to bare ground",
     "m", TerrainParameterType_Float, 300.0, 0.0, 5000.0, "", nullptr, 0},
    {"slipOffAngle", "Slip off angle", "The steepest slope settled snow will hold on", "deg",
     TerrainParameterType_Float, 38.0, 0.0, 89.0, "", nullptr, 0},
    {"adheredSnowMass", "Adhered snow mass",
     "Depth of the crust that clings on however steep the face", "m",
     TerrainParameterType_Float, 0.3, 0.0, 20.0, "", nullptr, 0},
    {"flows", "Flows", "Cuts furrows down the snow where it would run together", "",
     TerrainParameterType_Bool, 0.0, 0.0, 1.0, "", nullptr, 0},
    {"flowsDepth", "Flows depth", "How deep the furrows are cut into the snow", "m",
     TerrainParameterType_Float, 0.5, 0.0, 50.0, "", nullptr, 0},
    {"sharpBuildup", "Sharp buildup",
     "Leaves the last fall of snow unsettled, so piles keep their edges", "",
     TerrainParameterType_Bool, 0.0, 0.0, 1.0, "", nullptr, 0},
    {"surfaceDetails", "Surface details",
     "Small relief over the snow, in proportion to how deep it lies", "",
     TerrainParameterType_Float, 0.2, 0.0, 1.0, "", nullptr, 0},
    {"direction", "Direction", "Which way the wind drives the falling snow", "",
     TerrainParameterType_Enum, 0.0, 0.0, 4.0, "", driftDirections, 5},
    {"seed", "Seed", "Varies this node alone, on top of the graph seed", "",
     TerrainParameterType_Seed, 0.0, 0.0, 4294967295.0, "", nullptr, 0}};

struct SnowfieldRun {
    SnowfallShape shape;
    SnowSettling settling;
    SnowMelt melt;
    int cascades = 3;
    int duration = 8;
    float intensityMeters = 2.0f;
    bool hasTaper = false;
    float taperAmount = 0.3f;
    bool hasFlows = false;
    float flowsDepthMeters = 0.5f;
    bool hasSharpBuildup = false;
    float surfaceDetails = 0.2f;
    std::uint64_t seed = 0;
};

/// The bearing the wind drives the snow towards. Snow gathers on the faces
/// that look that way, which are the ones in the lee of the wind.
float driftBearingFor(int direction) {
    if (direction <= 0 || direction > 4) {
        return -1.0f;
    }
    return (direction - 1) * 90.0f;
}

SnowfieldRun readSnowfieldRun(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    SnowfieldRun run;
    run.cascades = std::max(static_cast<int>(api->parameterNumber(context, 0)), 1);
    run.duration = std::max(static_cast<int>(api->parameterNumber(context, 1)), 1);
    run.intensityMeters = static_cast<float>(api->parameterNumber(context, 2));
    run.hasTaper = api->parameterNumber(context, 3) >= 0.5;
    run.taperAmount = static_cast<float>(api->parameterNumber(context, 4));
    run.settling.settleDurationMeters = static_cast<float>(api->parameterNumber(context, 5));
    run.melt.share = static_cast<float>(api->parameterNumber(context, 6));
    run.shape.snowLineMeters = static_cast<float>(api->parameterNumber(context, 7));
    run.shape.snowLineFalloffMeters = static_cast<float>(api->parameterNumber(context, 8));
    run.shape.slipOffAngleDegrees = static_cast<float>(api->parameterNumber(context, 9));
    run.settling.slipOffAngleDegrees = run.shape.slipOffAngleDegrees;
    run.settling.adheredSnowMassMeters = static_cast<float>(api->parameterNumber(context, 10));
    run.hasFlows = api->parameterNumber(context, 11) >= 0.5;
    run.flowsDepthMeters = static_cast<float>(api->parameterNumber(context, 12));
    run.hasSharpBuildup = api->parameterNumber(context, 13) >= 0.5;
    run.surfaceDetails = static_cast<float>(api->parameterNumber(context, 14));
    run.shape.driftBearingDegrees =
        driftBearingFor(static_cast<int>(api->parameterNumber(context, 15)));
    run.seed = seedFromParameter(api, context, 16);

    run.shape.size = api->width(context);
    run.shape.cellSizeMeters = api->cellSizeMeters(context);
    run.melt.size = run.shape.size;
    run.melt.cellSizeMeters = run.shape.cellSizeMeters;
    return run;
}

float cascadeIntensity(const SnowfieldRun& run, int cascade) {
    if (!run.hasTaper) {
        return run.intensityMeters;
    }
    return run.intensityMeters * std::pow(1.0f - run.taperAmount, static_cast<float>(cascade));
}

/// Everything the season drops on a cell that takes every fall in full.
float seasonDepthMeters(const SnowfieldRun& run) {
    float total = 0.0f;
    for (int cascade = 0; cascade < run.cascades; ++cascade) {
        total += cascadeIntensity(run, cascade);
    }
    return total;
}

bool isLastFall(const SnowfieldRun& run, int cascade, int pass) {
    return cascade + 1 == run.cascades && pass + 1 == run.duration;
}

void runCascade(const TerrainContextApi* api, TerrainEvaluationContext* context,
                const float* ground, const SnowfieldRun& run, int cascade,
                const std::vector<float>& weights, std::vector<float>& depth,
                SettleScratch& scratch) {
    const float perPass = cascadeIntensity(run, cascade) / run.duration;
    for (int pass = 0; pass < run.duration; ++pass) {
        addSnowfall(depth, weights, perPass);
        if (run.hasSharpBuildup && isLastFall(run, cascade, pass)) {
            return;
        }
        settleSnow(api, context, ground, run.settling, depth, scratch);
    }
}

/// How hard the water is pulled together into channels, and how far the faint
/// headwaters are lifted towards them. Furrows in snow are broad, so both sit
/// where `FlowMask` leaves them.
constexpr float furrowConcentration = 1.4f;
constexpr float furrowContrast = 1.0f;

/**
 * Cuts furrows down the snow along the lines water would take over it. The
 * flow is read from the snow surface rather than from the rock, because a
 * filled gully no longer runs where the rock underneath does.
 */
void carveFlows(const TerrainContextApi* api, TerrainEvaluationContext* context,
                const float* ground, const SnowfieldRun& run, std::vector<float>& depth) {
    const std::size_t count = depth.size();
    std::vector<float> surface(count, 0.0f);
    writeSnowSurface(ground, depth, surface.data());

    std::vector<float> accumulation(count, 0.0f);
    std::vector<float> flow(count, 0.0f);
    accumulateFlow(surface.data(), run.shape.size, run.shape.cellSizeMeters, furrowConcentration,
                   accumulation.data());
    normaliseFlow(accumulation.data(), count, furrowContrast, flow.data());

    for (std::size_t index = 0; index < count; ++index) {
        depth[index] = std::max(depth[index] - run.flowsDepthMeters * flow[index], 0.0f);
    }
}

/// How much of the local depth the finest relief may take, at full strength.
constexpr float surfaceDetailShare = 0.1f;
const noise::FractalOptions detailOctaves{4, 0.5f, 2.0f};

float detailFeatureSizeMeters(const SnowfieldRun& run) {
    return std::max(run.settling.settleDurationMeters * 0.5f, 1.0f);
}

/// Small relief over the snow, in proportion to how deep it lies: bare ground
/// stays bare and a deep drift gets a rolling surface.
void addSurfaceDetail(const TerrainContextApi* api, TerrainEvaluationContext* context,
                      const SnowfieldRun& run, std::vector<float>& depth) {
    if (run.surfaceDetails <= 0.0f) {
        return;
    }
    const float featureSize = detailFeatureSizeMeters(run);
    const int size = run.shape.size;
    forEachRowRange(api, context, size, [&](int firstRow, int lastRow) {
        for (int row = firstRow; row < lastRow; ++row) {
            for (int column = 0; column < size; ++column) {
                const float x =
                    static_cast<float>((column + 0.5) * run.shape.cellSizeMeters) / featureSize;
                const float y =
                    static_cast<float>((row + 0.5) * run.shape.cellSizeMeters) / featureSize;
                const float relief = noise::fractalPerlin(x, y, run.seed, detailOctaves);
                depth[cellIndex(size, row, column)] *=
                    1.0f + run.surfaceDetails * surfaceDetailShare * relief;
            }
        }
    });
}

void finishSnowfield(const TerrainContextApi* api, TerrainEvaluationContext* context,
                     const float* ground, const SnowfieldRun& run, std::vector<float>& depth) {
    if (run.hasFlows) {
        carveFlows(api, context, ground, run, depth);
    }
    addSurfaceDetail(api, context, run, depth);
    meltSnow(api, context, ground, run.melt, depth);
}

/**
 * Several spells of snowfall over the same ground, which is what leaves a
 * field settled rather than freshly fallen: every spell slides into what the
 * one before it left, so the hollows fill and the shoulders round off.
 */
void evaluateSnowfield(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const std::size_t count = cellCount(api, context);
    const float* ground = api->inputField(context, 0);
    const SnowfieldRun run = readSnowfieldRun(api, context);
    const std::vector<float> weights =
        snowfallWeights(api, context, run.shape, api->inputField(context, 1));

    std::vector<float> depth(count, 0.0f);
    SettleScratch scratch(count);
    for (int cascade = 0; cascade < run.cascades; ++cascade) {
        runCascade(api, context, ground, run, cascade, weights, depth, scratch);
        api->reportProgress(context, static_cast<float>(cascade + 1) / run.cascades);
    }
    finishSnowfield(api, context, ground, run, depth);

    writeSnowSurface(ground, depth, api->outputField(context, 0));
    writeSnowCover(depth, seasonDepthMeters(run), api->outputField(context, 1));
}

const TerrainNodeDescriptor snowfieldDescriptor = {
    "Snowfield",
    "Simulate",
    "Builds broad settled snow cover out of several spells of snowfall",
    2,
    snowInputs,
    2,
    snowfieldOutputs,
    2,
    snowfieldParameters,
    17,
    &evaluateSnowfield};

/* ================================================================ Dusting == */

/// The ground a peak is measured against: how high a cell stands over the mean
/// of the ground within this much of it.
constexpr double peakWindowMeters = 200.0;
/// Ground distance across one patch of the dusting's own cover pattern.
constexpr float dustingPatchMeters = 300.0f;
/// How sharply a patch of dusting gives way to bare ground, in pattern units.
constexpr float coverageEdgeWidth = 0.15f;
/// How far the gritty grain pushes a cell either side of its patch value.
constexpr float grittyGrainWidth = 0.25f;
const noise::FractalOptions coverOctaves{4, 0.5f, 2.0f};

void meanAlongOneRow(const float* source, int size, int radiusCells, float* destination) {
    std::vector<double> runningSum(static_cast<std::size_t>(size) + 1, 0.0);
    for (int column = 0; column < size; ++column) {
        runningSum[column + 1] = runningSum[column] + source[column];
    }
    for (int column = 0; column < size; ++column) {
        const int low = std::max(column - radiusCells, 0);
        const int high = std::min(column + radiusCells + 1, size);
        const double window = high - low;
        destination[column] = static_cast<float>((runningSum[high] - runningSum[low]) / window);
    }
}

void meanAlongRows(const std::vector<float>& source, int size, int radiusCells,
                   std::vector<float>& destination) {
    for (int row = 0; row < size; ++row) {
        const std::size_t start = cellIndex(size, row, 0);
        meanAlongOneRow(source.data() + start, size, radiusCells, destination.data() + start);
    }
}

void transpose(const std::vector<float>& source, int size, std::vector<float>& destination) {
    for (int row = 0; row < size; ++row) {
        for (int column = 0; column < size; ++column) {
            destination[cellIndex(size, column, row)] = source[cellIndex(size, row, column)];
        }
    }
}

/// The mean height over a square window, as two passes of a running sum along
/// rows with a transpose between them, so the cost does not grow with the
/// window.
std::vector<float> localMeanHeights(const float* ground, int size, int radiusCells) {
    const std::size_t count = static_cast<std::size_t>(size) * size;
    const std::vector<float> source(ground, ground + count);
    std::vector<float> alongRows(count, 0.0f);
    std::vector<float> turned(count, 0.0f);
    std::vector<float> alongColumns(count, 0.0f);
    std::vector<float> result(count, 0.0f);

    meanAlongRows(source, size, radiusCells, alongRows);
    transpose(alongRows, size, turned);
    meanAlongRows(turned, size, radiusCells, alongColumns);
    transpose(alongColumns, size, result);
    return result;
}

/**
 * How far each cell stands above the ground around it, as [0, 1] against the
 * strongest protrusion on the grid. A fixed height in meters would mean one
 * thing on a ridge of boulders and another on an alpine face, so the scale
 * comes from the terrain itself.
 */
std::vector<float> protrusionField(const float* ground, int size, double cellSizeMeters) {
    const int radius =
        std::max(static_cast<int>(std::lround(peakWindowMeters * 0.5 / cellSizeMeters)), 1);
    const std::vector<float> mean = localMeanHeights(ground, size, radius);

    std::vector<float> protrusion(mean.size(), 0.0f);
    float strongest = 0.0f;
    for (std::size_t index = 0; index < mean.size(); ++index) {
        protrusion[index] = std::max(ground[index] - mean[index], 0.0f);
        strongest = std::max(strongest, protrusion[index]);
    }
    const float scale = 1.0f / std::max(strongest, 1.0e-6f);
    for (float& value : protrusion) {
        value = std::clamp(value * scale, 0.0f, 1.0f);
    }
    return protrusion;
}

std::vector<float> flowField(const float* ground, int size, double cellSizeMeters) {
    const std::size_t count = static_cast<std::size_t>(size) * size;
    std::vector<float> accumulation(count, 0.0f);
    std::vector<float> flow(count, 0.0f);
    accumulateFlow(ground, size, cellSizeMeters, furrowConcentration, accumulation.data());
    normaliseFlow(accumulation.data(), count, furrowContrast, flow.data());
    return flow;
}

struct DustingShape {
    int size = 0;
    double cellSizeMeters = 1.0;
    float strength = 0.7f;
    float snowLineMeters = 1500.0f;
    float falloffMeters = 300.0f;
    bool isDirectional = false;
    float directionDegrees = 0.0f;
    float peaks = 0.3f;
    float coverage = 0.6f;
    float slopeCoverageStartDegrees = 30.0f;
    float slopeCoverageEndDegrees = 60.0f;
    float flow = 0.0f;
    float melt = 0.0f;
    bool isGritty = false;
    bool includesExistingSnow = false;
    float depthMeters = 0.05f;
    std::uint64_t seed = 0;
};

/// The fields a cell's dusting is read from. The last two are empty unless the
/// dial that pays for them is turned up.
struct DustingFields {
    const float* ground = nullptr;
    const float* mask = nullptr;
    const float* existingSnow = nullptr;
    std::vector<float> protrusion;
    std::vector<float> flow;
};

float grittyGrain(const DustingShape& shape, std::size_t index) {
    if (!shape.isGritty) {
        return 0.0f;
    }
    return (cellNoise(shape.seed, index, grittyGrainSalt) - 0.5f) * grittyGrainWidth;
}

/// How willing a cell is to hold a dusting, before the coverage threshold is
/// applied: a patchy base pattern, raised on the protrusions and in the lines
/// water would take.
float dustingLevel(const DustingShape& shape, const DustingFields& fields, int row, int column) {
    const std::size_t index = cellIndex(shape.size, row, column);
    const float x = static_cast<float>((column + 0.5) * shape.cellSizeMeters) / dustingPatchMeters;
    const float y = static_cast<float>((row + 0.5) * shape.cellSizeMeters) / dustingPatchMeters;
    const float patch = 0.5f + 0.5f * noise::fractalPerlin(x, y, shape.seed, coverOctaves);
    return patch + grittyGrain(shape, index) + shape.peaks * valueAt(fields.protrusion, index)
           + shape.flow * valueAt(fields.flow, index);
}

/// What the wind and the sun do to a face: the wind takes the dusting off the
/// slopes turned away from where it drives the snow, and the sun melts it off
/// the slopes that look towards it.
float weatherWeight(const DustingShape& shape, const SurfaceGradient& gradient) {
    const float facing = facingShare(gradient, shape.directionDegrees);
    const float melted = 1.0f - shape.melt * facing;
    if (!shape.isDirectional) {
        return melted;
    }
    return melted * (1.0f - driftLoss * (1.0f - facing));
}

float dustingAt(const DustingShape& shape, const DustingFields& fields, int row, int column) {
    const std::size_t index = cellIndex(shape.size, row, column);
    const SurfaceGradient gradient =
        sampleGradient(fields.ground, shape.size, row, column, shape.cellSizeMeters);
    const float altitude = bandWeight(fields.ground[index], shape.snowLineMeters,
                                      aboveEverythingMeters, shape.falloffMeters);
    const float slopeBand =
        bandWeight(slopeDegrees(gradient), 0.0f, shape.slopeCoverageStartDegrees,
                   std::max(shape.slopeCoverageEndDegrees - shape.slopeCoverageStartDegrees,
                            1.0e-3f));
    const float level = dustingLevel(shape, fields, row, column) - (1.0f - shape.coverage);
    const float patch = smoothStep(level / coverageEdgeWidth);
    return shape.strength * altitude * slopeBand * patch * weatherWeight(shape, gradient)
           * maskAt(fields.mask, index);
}

float withExistingSnow(const DustingShape& shape, const DustingFields& fields, std::size_t index,
                       float dusting) {
    if (!shape.includesExistingSnow || fields.existingSnow == nullptr) {
        return dusting;
    }
    return std::max(dusting, std::clamp(fields.existingSnow[index], 0.0f, 1.0f));
}

const TerrainPortDescriptor dustingInputs[] = {
    {"In", "The ground the dusting settles on", TerrainPortType_Field,
     TerrainFieldSemantics_Heightfield, 0},
    {"Mask", "Scales where the dusting is allowed to lie", TerrainPortType_Field,
     TerrainFieldSemantics_Mask, 1},
    {"ExistingSnow", "Snow already lying, which the dusting adds to", TerrainPortType_Field,
     TerrainFieldSemantics_Mask, 1}};

const TerrainPortDescriptor dustingOutputs[] = {
    {"Out", "Where the dusting lies, from 0 to 1", TerrainPortType_Field,
     TerrainFieldSemantics_Mask, 0},
    {"Surface", "The ground raised by the depth of the dusting", TerrainPortType_Field,
     TerrainFieldSemantics_Heightfield, 0}};

const TerrainParameterDescriptor dustingParameters[] = {
    {"strength", "Strength", "How strongly the dusting shows where it lies", "",
     TerrainParameterType_Float, 0.7, 0.0, 1.0, "", nullptr, 0},
    {"snowLine", "Snow line", "No dusting settles below this altitude", "m",
     TerrainParameterType_Float, 1500.0, -12000.0, 12000.0, "", nullptr, 0},
    {"falloff", "Falloff", "How gradually the snow line gives way to bare ground", "m",
     TerrainParameterType_Float, 300.0, 0.0, 5000.0, "", nullptr, 0},
    {"directional", "Directional", "Lets the wind gather the dusting on one side of every rise",
     "", TerrainParameterType_Bool, 0.0, 0.0, 1.0, "", nullptr, 0},
    {"direction", "Direction",
     "Compass bearing the wind drives the snow towards, and the sun stands in",
     "deg", TerrainParameterType_Float, 0.0, 0.0, 360.0, "", nullptr, 0},
    {"peaks", "Peaks", "Extra dusting on whatever stands above the ground around it", "",
     TerrainParameterType_Float, 0.3, 0.0, 1.0, "", nullptr, 0},
    {"coverage", "Coverage", "How much of the ground the dusting reaches", "",
     TerrainParameterType_Float, 0.6, 0.0, 1.0, "", nullptr, 0},
    {"slopeCoverageStart", "Slope start", "Ground gentler than this is covered in full", "deg",
     TerrainParameterType_Float, 30.0, 0.0, 90.0, "", nullptr, 0},
    {"slopeCoverageEnd", "Slope end", "Ground steeper than this holds nothing", "deg",
     TerrainParameterType_Float, 60.0, 0.0, 90.0, "", nullptr, 0},
    {"flow", "Flow", "Gathers the dusting into the lines water would take", "",
     TerrainParameterType_Float, 0.0, 0.0, 1.0, "", nullptr, 0},
    {"melt", "Melt", "How much the sun takes off the slopes facing it", "",
     TerrainParameterType_Float, 0.0, 0.0, 1.0, "", nullptr, 0},
    {"gritty", "Gritty", "Breaks the edge of the dusting up cell by cell", "",
     TerrainParameterType_Bool, 0.0, 0.0, 1.0, "", nullptr, 0},
    {"includeExistingSnow", "Include existing snow",
     "Lays the dusting over the snow on the second input", "",
     TerrainParameterType_Bool, 0.0, 0.0, 1.0, "", nullptr, 0},
    {"depth", "Depth", "How far a full dusting raises the surface output", "m",
     TerrainParameterType_Float, 0.05, 0.0, 100.0, "", nullptr, 0},
    {"seed", "Seed", "Varies this node alone, on top of the graph seed", "",
     TerrainParameterType_Seed, 0.0, 0.0, 4294967295.0, "", nullptr, 0}};

DustingShape readDustingShape(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    DustingShape shape;
    shape.strength = static_cast<float>(api->parameterNumber(context, 0));
    shape.snowLineMeters = static_cast<float>(api->parameterNumber(context, 1));
    shape.falloffMeters = static_cast<float>(api->parameterNumber(context, 2));
    shape.isDirectional = api->parameterNumber(context, 3) >= 0.5;
    shape.directionDegrees = static_cast<float>(api->parameterNumber(context, 4));
    shape.peaks = static_cast<float>(api->parameterNumber(context, 5));
    shape.coverage = static_cast<float>(api->parameterNumber(context, 6));
    shape.slopeCoverageStartDegrees = static_cast<float>(api->parameterNumber(context, 7));
    shape.slopeCoverageEndDegrees = static_cast<float>(api->parameterNumber(context, 8));
    shape.flow = static_cast<float>(api->parameterNumber(context, 9));
    shape.melt = static_cast<float>(api->parameterNumber(context, 10));
    shape.isGritty = api->parameterNumber(context, 11) >= 0.5;
    shape.includesExistingSnow = api->parameterNumber(context, 12) >= 0.5;
    shape.depthMeters = static_cast<float>(api->parameterNumber(context, 13));
    shape.seed = seedFromParameter(api, context, 14);

    shape.size = api->width(context);
    shape.cellSizeMeters = api->cellSizeMeters(context);
    return shape;
}

DustingFields readDustingFields(const TerrainContextApi* api, TerrainEvaluationContext* context,
                                const DustingShape& shape) {
    DustingFields fields;
    fields.ground = api->inputField(context, 0);
    fields.mask = api->inputField(context, 1);
    fields.existingSnow = api->inputField(context, 2);
    if (shape.peaks > 0.0f) {
        fields.protrusion = protrusionField(fields.ground, shape.size, shape.cellSizeMeters);
    }
    if (shape.flow > 0.0f) {
        fields.flow = flowField(fields.ground, shape.size, shape.cellSizeMeters);
    }
    return fields;
}

/**
 * A thin sprinkling of snow, worked out cell by cell rather than simulated.
 * Nothing here moves any snow around: where a dusting lies is decided by the
 * altitude, the slope, what stands proud of the ground around it and a patchy
 * cover pattern, which is all a light fall of snow answers to.
 */
void evaluateDusting(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const DustingShape shape = readDustingShape(api, context);
    const DustingFields fields = readDustingFields(api, context, shape);
    float* dusting = api->outputField(context, 0);
    float* surface = api->outputField(context, 1);

    forEachRowRange(api, context, shape.size, [&](int firstRow, int lastRow) {
        for (int row = firstRow; row < lastRow; ++row) {
            for (int column = 0; column < shape.size; ++column) {
                const std::size_t index = cellIndex(shape.size, row, column);
                const float lying = withExistingSnow(shape, fields, index,
                                                     dustingAt(shape, fields, row, column));
                dusting[index] = std::clamp(lying, 0.0f, 1.0f);
                surface[index] = fields.ground[index] + dusting[index] * shape.depthMeters;
            }
        }
    });
}

const TerrainNodeDescriptor dustingDescriptor = {
    "Dusting",
    "Simulate",
    "Sprinkles a thin layer of snow over the high, gentle and exposed ground",
    1,
    dustingInputs,
    3,
    dustingOutputs,
    2,
    dustingParameters,
    15,
    &evaluateDusting};

}  // namespace

void registerSnowNodes(const TerrainRegistryApi& api, TerrainRegistry* registry) {
    api.registerNode(registry, &snowDescriptor);
    api.registerNode(registry, &snowfieldDescriptor);
    api.registerNode(registry, &dustingDescriptor);
}

}  // namespace terrain::nodes
