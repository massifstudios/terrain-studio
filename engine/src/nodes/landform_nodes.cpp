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

/// Crags, cut into the flanks and faded out as the shape nears its own
/// summit, so the slopes break up while the peak stays a single peak.
float cragged(float shaped, float ridges, float detailStrength) {
    const float flank = 1.0f - shaped * shaped;
    return shaped * (1.0f - detailStrength * flank * (1.0f - ridges));
}

const noise::FractalOptions spurOptions{3, 0.5f, 2.0f};

/**
 * The rock between the spurs. It falls off faster than 1/f, because a 1/f
 * spectrum is equally rough at every scale and buries a landform under bumps
 * of every size at once.
 */
const noise::FractalOptions rockOptions{3, 0.4f, 2.0f};

float ridgesAt(const GroundPoint& point, float featureSizeMeters, std::uint64_t seed,
               const noise::FractalOptions& options) {
    return noise::ridged(static_cast<float>(point.eastingMeters) / featureSizeMeters,
                         static_cast<float>(point.northingMeters) / featureSizeMeters, seed,
                         options, 1.0f);
}

/**
 * The ridged field the crags are cut from: coarse ridges read from a domain
 * stretched down the slope, and finer ones read from the ground itself.
 *
 * Stretching every scale at once combs the whole surface into strands. Spurs
 * are directional only at the large scale; the rock between them is not, and
 * it is that isotropic band which keeps the slope from looking brushed.
 */
float craggyField(const GroundPoint& stretched, const GroundPoint& ground,
                  float featureSizeMeters, std::uint64_t seed) {
    const float spurs =
        ridgesAt(stretched, featureSizeMeters, terrainSeedForLane(seed, 1), spurOptions);
    const float rock =
        ridgesAt(ground, featureSizeMeters * 0.35f, terrainSeedForLane(seed, 2), rockOptions);
    return spurs * 0.65f + rock * 0.35f;
}

/**
 * Where the ridged detail is read from around a summit: the ground point
 * pulled radially toward a fixed ring about the centre.
 *
 * Damping how fast the reading position moves outward, while leaving its
 * motion around the centre alone, stretches every noise feature along the
 * slope. That is what makes a spur: a ridge that runs from the summit down to
 * the foot, with a gully beside it. Because the ring keeps a finite radius,
 * the spurs converge on the peak in a bounded number rather than collapsing
 * into infinite frequency there.
 */
GroundPoint spurReadPoint(const GroundPoint& ground, const ShapePlacement& placement,
                          double radiusMeters, float elongation) {
    const double east = ground.eastingMeters - placement.centreEastingMeters;
    const double north = ground.northingMeters - placement.centreNorthingMeters;
    const double distance = std::hypot(east, north);
    if (distance < 1.0e-6) {
        return GroundPoint{placement.centreEastingMeters, placement.centreNorthingMeters};
    }
    const double ringRadiusMeters = radiusMeters * 0.5;
    const double pulled = distance * (1.0 - elongation) + ringRadiusMeters * elongation;
    return GroundPoint{placement.centreEastingMeters + east * pulled / distance,
                       placement.centreNorthingMeters + north * pulled / distance};
}

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
     TerrainParameterType_Float, 0.0, -12000.0, 12000.0, "", nullptr, 0},
    {"spurElongation", "Spur length",
     "How far the detail is drawn out into spurs running from the summit to the foot", "",
     TerrainParameterType_Float, 0.6, 0.0, 0.95, "", nullptr, 0}};

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
    const float spurElongation = static_cast<float>(api->parameterNumber(context, 10));

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
                const float ridges = craggyField(
                    spurReadPoint(ground, placement, radiusMeters, spurElongation), ground,
                    featureSizeMeters, detailSeed);
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
    11,
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
     TerrainParameterType_Float, 0.0, -12000.0, 12000.0, "", nullptr, 0},
    {"spurElongation", "Spur length",
     "How far the detail is drawn out into ribs running down the flanks of the spine", "",
     TerrainParameterType_Float, 0.6, 0.0, 0.95, "", nullptr, 0}};

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

/**
 * The range's answer to `spurReadPoint`: the ground point pulled toward the
 * spine across the range rather than toward a summit. Damping the across-spine
 * coordinate stretches the noise the same way, so its features become ribs
 * running down the flanks and over the crest, instead of crumple laid over the
 * whole chain.
 */
GroundPoint ribReadPoint(const GroundPoint& ground, const ShapePlacement& placement,
                         const RangeAxis& axis, float elongation) {
    const double east = ground.eastingMeters - placement.centreEastingMeters;
    const double north = ground.northingMeters - placement.centreNorthingMeters;
    const double along = east * axis.alongEast + north * axis.alongNorth;
    const double across =
        (east * axis.acrossEast + north * axis.acrossNorth) * (1.0 - elongation);
    return GroundPoint{placement.centreEastingMeters + axis.alongEast * along
                           + axis.acrossEast * across,
                       placement.centreNorthingMeters + axis.alongNorth * along
                           + axis.acrossNorth * across};
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
    const float spurElongation = static_cast<float>(api->parameterNumber(context, 15));

    const RangeAxis axis = axisFor(api->parameterNumber(context, 2));
    const std::vector<RangePeak> peaks = spineFrom(
        placement, axis, api->parameterNumber(context, 3), widthMeters,
        static_cast<int>(api->parameterNumber(context, 5)),
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
                const float ridges =
                    craggyField(ribReadPoint(ground, placement, axis, spurElongation), ground,
                                featureSizeMeters, detailSeed);
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
    16,
    &evaluateMountainRange};

}  // namespace

void registerLandformNodes(const TerrainRegistryApi& api, TerrainRegistry* registry) {
    api.registerNode(registry, &mountainDescriptor);
    api.registerNode(registry, &mountainRangeDescriptor);
}

}  // namespace terrain::nodes
