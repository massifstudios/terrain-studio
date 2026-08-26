#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

#include "node_support.h"
#include "shape_support.h"
#include "terrain/noise.h"

namespace terrain::nodes {
namespace {

const TerrainPortDescriptor landformOutputs[] = {
    {"Out", "The landform", TerrainPortType_Field, TerrainFieldSemantics_Heightfield, 0}};

/**
 * A displacement of where each cell reads the landform from, driven by two
 * decorrelated octaves of fBm.
 *
 * Analytic landforms are circles and straight lines, and a circle reads as a
 * circle however much detail is laid over it. Bending the ground the shape is
 * drawn on breaks the silhouette instead of the surface, which is what keeps a
 * mountain from looking stamped.
 */
struct DomainWarp {
    std::uint64_t eastingSeed = 0;
    std::uint64_t northingSeed = 0;
    float featureSizeMeters = 1000.0f;
    float amountMeters = 0.0f;
    noise::FractalOptions options{4, 0.5f, 2.0f};
};

struct GroundPoint {
    double eastingMeters = 0.0;
    double northingMeters = 0.0;
};

GroundPoint warpedBy(const DomainWarp& warp, double eastingMeters, double northingMeters) {
    const float x = static_cast<float>(eastingMeters) / warp.featureSizeMeters;
    const float y = static_cast<float>(northingMeters) / warp.featureSizeMeters;
    const float alongEast = noise::fractalPerlin(x, y, warp.eastingSeed, warp.options);
    const float alongNorth = noise::fractalPerlin(x, y, warp.northingSeed, warp.options);
    return GroundPoint{eastingMeters + alongEast * warp.amountMeters,
                       northingMeters + alongNorth * warp.amountMeters};
}

/// Crags, faded in by how high the underlying shape already stands, so a peak
/// breaks up while its apron stays smooth.
float cragged(float shaped, float ridges, float detailStrength) {
    return shaped * (1.0f - detailStrength + detailStrength * ridges);
}

const noise::FractalOptions ridgeOptions{6, 0.5f, 2.0f};

/* ------------------------------------------------------------- Mountain -- */

const TerrainParameterDescriptor mountainParameters[] = {
    {"centreEast", "Centre east", "Where the summit sits across the world, 0 at the west edge",
     "", TerrainParameterType_Float, 0.5, 0.0, 1.0, "", nullptr, 0},
    {"centreNorth", "Centre north", "Where the summit sits up the world, 0 at the south edge", "",
     TerrainParameterType_Float, 0.5, 0.0, 1.0, "", nullptr, 0},
    {"radius", "Radius", "Ground distance from the summit out to the foot of the slope", "m",
     TerrainParameterType_Float, 3000.0, 1.0, 100000.0, "", nullptr, 0},
    {"height", "Height", "How far the summit stands above the base altitude", "m",
     TerrainParameterType_Float, 1200.0, 0.0, 12000.0, "", nullptr, 0},
    {"seed", "Seed", "Varies this node alone, on top of the graph seed", "",
     TerrainParameterType_Seed, 0.0, 0.0, 4294967295.0, "", nullptr, 0},
    {"peakSharpness", "Peak sharpness", "Higher values pull the mass down and leave a finer peak",
     "", TerrainParameterType_Float, 1.6, 0.2, 6.0, "", nullptr, 0},
    {"irregularity", "Irregularity", "How far the outline wanders off a circle", "",
     TerrainParameterType_Float, 0.35, 0.0, 1.0, "", nullptr, 0},
    {"detailStrength", "Detail", "How much ridged detail is cut into the slopes", "",
     TerrainParameterType_Float, 0.5, 0.0, 1.0, "", nullptr, 0},
    {"featureSize", "Detail size", "Ground distance between neighbouring spurs", "m",
     TerrainParameterType_Float, 600.0, 1.0, 100000.0, "", nullptr, 0},
    {"baseAltitude", "Base altitude", "The altitude the foot of the mountain sits at", "m",
     TerrainParameterType_Float, 0.0, -12000.0, 12000.0, "", nullptr, 0}};

void evaluateMountain(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const WorldFrame frame = worldFrameFor(api, context);
    const ShapePlacement placement = placementFrom(frame, api->parameterNumber(context, 0),
                                                   api->parameterNumber(context, 1), 0.0);
    const double radiusMeters = api->parameterNumber(context, 2);
    const float height = static_cast<float>(api->parameterNumber(context, 3));
    const std::uint64_t seed = terrainSeedForLane(
        api->randomSeed(context),
        static_cast<std::uint64_t>(api->parameterNumber(context, 4)));
    const float peakSharpness = static_cast<float>(api->parameterNumber(context, 5));
    const float detailStrength = static_cast<float>(api->parameterNumber(context, 7));
    const float featureSizeMeters = static_cast<float>(api->parameterNumber(context, 8));
    const float baseAltitude = static_cast<float>(api->parameterNumber(context, 9));

    DomainWarp warp;
    warp.eastingSeed = terrainSeedForLane(seed, 1);
    warp.northingSeed = terrainSeedForLane(seed, 2);
    warp.featureSizeMeters = static_cast<float>(radiusMeters);
    warp.amountMeters =
        static_cast<float>(api->parameterNumber(context, 6) * radiusMeters * 0.5);
    const std::uint64_t detailSeed = terrainSeedForLane(seed, 3);
    float* output = api->outputField(context, 0);

    forEachRowRange(api, context, frame.size, [&](int firstRow, int lastRow) {
        for (int row = firstRow; row < lastRow; ++row) {
            float* line = output + static_cast<std::size_t>(row) * frame.size;
            for (int column = 0; column < frame.size; ++column) {
                const GroundPoint ground =
                    warpedBy(warp, frame.eastingAt(column), frame.northingAt(row));
                const LocalPoint point =
                    placement.localPoint(ground.eastingMeters, ground.northingMeters);
                const float base = radialProfileValue(RadialProfile::Smooth,
                                                      point.distanceFromCentre(), radiusMeters);
                const float ridges = noise::ridged(
                    static_cast<float>(ground.eastingMeters) / featureSizeMeters,
                    static_cast<float>(ground.northingMeters) / featureSizeMeters, detailSeed,
                    ridgeOptions, 1.0f);
                const float relief =
                    cragged(std::pow(base, peakSharpness), ridges, detailStrength);
                line[column] = relief * height + baseAltitude;
            }
        }
    });
}

const TerrainNodeDescriptor mountainDescriptor = {
    "Mountain",
    "Terrain",
    "A single mountain: an irregular mass with ridged slopes and one summit",
    1,
    nullptr,
    0,
    landformOutputs,
    1,
    mountainParameters,
    10,
    &evaluateMountain};

/* -------------------------------------------------------- MountainRange -- */

const TerrainParameterDescriptor rangeParameters[] = {
    {"centreEast", "Centre east", "Where the range is centred across the world", "",
     TerrainParameterType_Float, 0.5, 0.0, 1.0, "", nullptr, 0},
    {"centreNorth", "Centre north", "Where the range is centred up the world", "",
     TerrainParameterType_Float, 0.5, 0.0, 1.0, "", nullptr, 0},
    {"bearing", "Bearing", "Compass bearing the range runs along; 0 is north", "deg",
     TerrainParameterType_Float, 45.0, 0.0, 360.0, "", nullptr, 0},
    {"length", "Length", "How far the range reaches end to end", "m",
     TerrainParameterType_Float, 12000.0, 1.0, 400000.0, "", nullptr, 0},
    {"width", "Width", "How broad the range is across its spine", "m",
     TerrainParameterType_Float, 4000.0, 1.0, 400000.0, "", nullptr, 0},
    {"peakCount", "Peaks", "How many summits stand along the spine", "",
     TerrainParameterType_Int, 5.0, 1.0, 64.0, "", nullptr, 0},
    {"height", "Height", "How far the tallest summit stands above the base altitude", "m",
     TerrainParameterType_Float, 1800.0, 0.0, 12000.0, "", nullptr, 0},
    {"heightVariation", "Height variation", "How much shorter the lesser summits may be", "",
     TerrainParameterType_Float, 0.4, 0.0, 1.0, "", nullptr, 0},
    {"saddleHeight", "Saddle height", "How high the spine stays in the passes between summits", "",
     TerrainParameterType_Float, 0.5, 0.0, 1.0, "", nullptr, 0},
    {"peakSharpness", "Peak sharpness", "Higher values pull the mass down and leave finer peaks",
     "", TerrainParameterType_Float, 1.6, 0.2, 6.0, "", nullptr, 0},
    {"seed", "Seed", "Varies this node alone, on top of the graph seed", "",
     TerrainParameterType_Seed, 0.0, 0.0, 4294967295.0, "", nullptr, 0},
    {"irregularity", "Irregularity", "How far the spine and its flanks wander off straight", "",
     TerrainParameterType_Float, 0.4, 0.0, 1.0, "", nullptr, 0},
    {"detailStrength", "Detail", "How much ridged detail is cut into the slopes", "",
     TerrainParameterType_Float, 0.5, 0.0, 1.0, "", nullptr, 0},
    {"featureSize", "Detail size", "Ground distance between neighbouring spurs", "m",
     TerrainParameterType_Float, 700.0, 1.0, 100000.0, "", nullptr, 0},
    {"baseAltitude", "Base altitude", "The altitude the foot of the range sits at", "m",
     TerrainParameterType_Float, 0.0, -12000.0, 12000.0, "", nullptr, 0}};

struct RangePeak {
    double eastingMeters = 0.0;
    double northingMeters = 0.0;
    double radiusMeters = 0.0;
    float heightFraction = 1.0f;
};

struct RangeAxis {
    double alongEast = 0.0;
    double alongNorth = 1.0;
    double acrossEast = 1.0;
    double acrossNorth = 0.0;
};

RangeAxis axisFor(double bearingDegrees) {
    const double radians = bearingDegrees * std::numbers::pi / 180.0;
    return RangeAxis{std::sin(radians), std::cos(radians), std::cos(radians), -std::sin(radians)};
}

/**
 * Summits are spread evenly along the spine and then jittered, each from a
 * seed derived from its own index. Deriving per index rather than drawing from
 * a running generator means changing the peak count renumbers nothing: the
 * summits that were already there stay where they were.
 */
std::vector<RangePeak> spineFrom(const ShapePlacement& placement, const RangeAxis& axis,
                                 double lengthMeters, double widthMeters, int peakCount,
                                 float heightVariation, std::uint64_t seed) {
    std::vector<RangePeak> peaks;
    peaks.reserve(static_cast<std::size_t>(peakCount));
    for (int index = 0; index < peakCount; ++index) {
        std::uint64_t state = terrainSeedForLane(seed, static_cast<std::uint64_t>(index));
        const double alongUnit = (index + 0.5) / peakCount - 0.5;
        const double alongJitter =
            (terrainRandomUnitFloat(&state) - 0.5) * lengthMeters / peakCount * 0.5;
        const double acrossJitter = (terrainRandomUnitFloat(&state) - 0.5) * widthMeters * 0.5;
        const double along = alongUnit * lengthMeters + alongJitter;

        RangePeak peak;
        peak.eastingMeters = placement.centreEastingMeters + axis.alongEast * along
                             + axis.acrossEast * acrossJitter;
        peak.northingMeters = placement.centreNorthingMeters + axis.alongNorth * along
                              + axis.acrossNorth * acrossJitter;
        peak.radiusMeters = widthMeters * 0.5 * (0.7 + 0.6 * terrainRandomUnitFloat(&state));
        peak.heightFraction = 1.0f - heightVariation * terrainRandomUnitFloat(&state);
        peaks.push_back(peak);
    }
    return peaks;
}

float peakRelief(const std::vector<RangePeak>& peaks, const GroundPoint& ground,
                 float peakSharpness) {
    float highest = 0.0f;
    for (const RangePeak& peak : peaks) {
        const double distance = std::hypot(ground.eastingMeters - peak.eastingMeters,
                                           ground.northingMeters - peak.northingMeters);
        const float base =
            radialProfileValue(RadialProfile::Smooth, distance, peak.radiusMeters);
        highest = std::max(highest, std::pow(base, peakSharpness) * peak.heightFraction);
    }
    return highest;
}

/// The ridge that joins the summits, so the range reads as one landform rather
/// than as a row of separate hills.
float spineRelief(const std::vector<RangePeak>& peaks, const GroundPoint& ground,
                  double halfWidthMeters, float saddleHeight) {
    double nearest = halfWidthMeters;
    for (std::size_t index = 1; index < peaks.size(); ++index) {
        const double distance = segmentDistance(
            ground.eastingMeters, ground.northingMeters, peaks[index - 1].eastingMeters,
            peaks[index - 1].northingMeters, peaks[index].eastingMeters,
            peaks[index].northingMeters);
        nearest = std::min(nearest, distance);
    }
    return radialProfileValue(RadialProfile::Smooth, nearest, halfWidthMeters) * saddleHeight;
}

void evaluateMountainRange(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const WorldFrame frame = worldFrameFor(api, context);
    const ShapePlacement placement = placementFrom(frame, api->parameterNumber(context, 0),
                                                   api->parameterNumber(context, 1), 0.0);
    const double widthMeters = api->parameterNumber(context, 4);
    const float height = static_cast<float>(api->parameterNumber(context, 6));
    const float saddleHeight = static_cast<float>(api->parameterNumber(context, 8));
    const float peakSharpness = static_cast<float>(api->parameterNumber(context, 9));
    const std::uint64_t seed = terrainSeedForLane(
        api->randomSeed(context),
        static_cast<std::uint64_t>(api->parameterNumber(context, 10)));
    const float detailStrength = static_cast<float>(api->parameterNumber(context, 12));
    const float featureSizeMeters = static_cast<float>(api->parameterNumber(context, 13));
    const float baseAltitude = static_cast<float>(api->parameterNumber(context, 14));

    const std::vector<RangePeak> peaks = spineFrom(
        placement, axisFor(api->parameterNumber(context, 2)), api->parameterNumber(context, 3),
        widthMeters, static_cast<int>(api->parameterNumber(context, 5)),
        static_cast<float>(api->parameterNumber(context, 7)), terrainSeedForLane(seed, 4));

    DomainWarp warp;
    warp.eastingSeed = terrainSeedForLane(seed, 1);
    warp.northingSeed = terrainSeedForLane(seed, 2);
    warp.featureSizeMeters = static_cast<float>(widthMeters);
    warp.amountMeters = static_cast<float>(api->parameterNumber(context, 11) * widthMeters * 0.5);
    const std::uint64_t detailSeed = terrainSeedForLane(seed, 3);
    const double spineHalfWidth = widthMeters * 0.35;
    float* output = api->outputField(context, 0);

    forEachRowRange(api, context, frame.size, [&](int firstRow, int lastRow) {
        for (int row = firstRow; row < lastRow; ++row) {
            float* line = output + static_cast<std::size_t>(row) * frame.size;
            for (int column = 0; column < frame.size; ++column) {
                const GroundPoint ground =
                    warpedBy(warp, frame.eastingAt(column), frame.northingAt(row));
                const float shaped =
                    std::max(peakRelief(peaks, ground, peakSharpness),
                             spineRelief(peaks, ground, spineHalfWidth, saddleHeight));
                const float ridges = noise::ridged(
                    static_cast<float>(ground.eastingMeters) / featureSizeMeters,
                    static_cast<float>(ground.northingMeters) / featureSizeMeters, detailSeed,
                    ridgeOptions, 1.0f);
                line[column] = cragged(shaped, ridges, detailStrength) * height + baseAltitude;
            }
        }
    });
}

const TerrainNodeDescriptor mountainRangeDescriptor = {
    "MountainRange",
    "Terrain",
    "A chain of summits joined by a ridge, shaped for erosion to work on",
    1,
    nullptr,
    0,
    landformOutputs,
    1,
    rangeParameters,
    15,
    &evaluateMountainRange};

}  // namespace

void registerLandformNodes(const TerrainRegistryApi& api, TerrainRegistry* registry) {
    api.registerNode(registry, &mountainDescriptor);
    api.registerNode(registry, &mountainRangeDescriptor);
}

}  // namespace terrain::nodes
