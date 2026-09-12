#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>
#include <vector>

#include "node_support.h"
#include "shape_support.h"
#include "terrain/noise.h"

namespace terrain::nodes {
namespace {

const TerrainPortDescriptor landformOutputs[] = {
    {"Out", "The landform", TerrainPortType_Field, TerrainFieldSemantics_Heightfield, 0}};

/// Fills a band of rows from a function of the ground position, so a landform's
/// own arithmetic stays out of the parallel loop that calls it.
template <typename HeightAt>
void writeLandformRows(const WorldFrame& frame, int firstRow, int lastRow, float* output,
                       const HeightAt& heightAt) {
    for (int row = firstRow; row < lastRow; ++row) {
        float* line = output + static_cast<std::size_t>(row) * frame.size;
        for (int column = 0; column < frame.size; ++column) {
            line[column] = heightAt(frame.eastingAt(column), frame.northingAt(row));
        }
    }
}

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

/// The same displacement applied to a point already in hand.
GroundPoint warpedPoint(const DomainWarp& warp, const GroundPoint& point);

GroundPoint warpedBy(const DomainWarp& warp, double eastingMeters, double northingMeters) {
    const float x = static_cast<float>(eastingMeters) / warp.featureSizeMeters;
    const float y = static_cast<float>(northingMeters) / warp.featureSizeMeters;
    const float alongEast = noise::fractalPerlin(x, y, warp.eastingSeed, warp.options);
    const float alongNorth = noise::fractalPerlin(x, y, warp.northingSeed, warp.options);
    return GroundPoint{eastingMeters + alongEast * warp.amountMeters,
                       northingMeters + alongNorth * warp.amountMeters};
}

GroundPoint warpedPoint(const DomainWarp& warp, const GroundPoint& point) {
    return warpedBy(warp, point.eastingMeters, point.northingMeters);
}

/**
 * The ground point pulled radially toward a fixed ring about a centre, which
 * is how a field is read in a direction about that centre without reading it
 * along the ray as well.
 */
GroundPoint ringReadPoint(const GroundPoint& ground, const ShapePlacement& placement,
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

/* --------------------------------------------------- skeleton geometry -- */

struct RidgeSegment {
    GroundPoint first;
    GroundPoint second;
};

/// A unit direction on the ground.
struct Heading {
    double east = 0.0;
    double north = 1.0;

    Heading turned(double radians) const {
        const double cosine = std::cos(radians);
        const double sine = std::sin(radians);
        return Heading{east * cosine - north * sine, east * sine + north * cosine};
    }
};

/// The unit direction from one ground point to another.
Heading headingFrom(const GroundPoint& from, const GroundPoint& to) {
    const double east = to.eastingMeters - from.eastingMeters;
    const double north = to.northingMeters - from.northingMeters;
    const double length = std::max(std::hypot(east, north), 1.0e-9);
    return Heading{east / length, north / length};
}

GroundPoint stepFrom(const GroundPoint& origin, const Heading& heading, double distanceMeters) {
    return GroundPoint{origin.eastingMeters + heading.east * distanceMeters,
                       origin.northingMeters + heading.north * distanceMeters};
}

/// A value drawn evenly from either side of zero, out to `limit`.
double spreadFrom(std::uint64_t* state, double limit) {
    return (terrainRandomUnitFloat(state) - 0.5f) * 2.0 * limit;
}

/* ------------------------------------------------------------- Mountain -- */

/// The styles a graph may carry. Only Basic is drawn today; each of the others
/// takes its own shaping as it lands.
const TerrainEnumOption mountainStyles[] = {{"basic", "Basic", 0},
                                            {"eroded", "Eroded", 1},
                                            {"old", "Old", 2},
                                            {"alpine", "Alpine", 3},
                                            {"strata", "Strata", 4}};

const TerrainEnumOption mountainBulks[] = {
    {"low", "Low", 0}, {"medium", "Medium", 1}, {"high", "High", 2}};

const TerrainParameterDescriptor mountainParameters[] = {
    {"scale", "Scale", "Scales the whole massif, footprint and detail together", "",
     TerrainParameterType_Float, 1.0, 0.05, 4.0, "", nullptr, 0},
    {"height", "Height", "How far the summit stands above the base altitude", "m",
     TerrainParameterType_Float, 1200.0, 0.0, 12000.0, "", nullptr, 0},
    {"style", "Style", "Which kind of mountain is drawn", "", TerrainParameterType_Enum, 0.0, 0.0,
     4.0, "", mountainStyles, 5},
    {"bulk", "Bulk", "How much mass the flanks carry between the crest and the foot", "",
     TerrainParameterType_Enum, 1.0, 0.0, 2.0, "", mountainBulks, 3},
    {"reduceDetails", "Reduce details",
     "Holds back the finest ridges and leaves the larger shapes standing", "",
     TerrainParameterType_Float, 0.0, 0.0, 1.0, "", nullptr, 0},
    {"seed", "Seed", "Varies this node alone, on top of the graph seed", "",
     TerrainParameterType_Seed, 0.0, 0.0, 4294967295.0, "", nullptr, 0},
    {"centreEast", "X offset", "Where the massif sits across the world, 0 at the west edge", "",
     TerrainParameterType_Float, 0.5, 0.0, 1.0, "", nullptr, 0},
    {"centreNorth", "Y offset", "Where the massif sits up the world, 0 at the south edge", "",
     TerrainParameterType_Float, 0.5, 0.0, 1.0, "", nullptr, 0},
    {"radius", "Radius", "Ground distance from the summit out to the foot, before Scale", "m",
     TerrainParameterType_Float, 1900.0, 1.0, 100000.0, "", nullptr, 0},
    {"baseAltitude", "Base altitude", "The altitude the foot of the mountain sits at", "m",
     TerrainParameterType_Float, 0.0, -12000.0, 12000.0, "", nullptr, 0},
    {"peakSharpness", "Peak sharpness", "Higher values pull the mass down and leave a finer peak",
     "", TerrainParameterType_Float, 1.5, 0.2, 6.0, "", nullptr, 0},
    {"irregularity", "Irregularity", "How far the outline wanders off a circle", "",
     TerrainParameterType_Float, 0.35, 0.0, 1.0, "", nullptr, 0},
    {"detailStrength", "Detail", "How deep the valleys are cut between the spurs", "",
     TerrainParameterType_Float, 0.7, 0.0, 1.0, "", nullptr, 0},
    {"featureSize", "Spur spacing", "Ground distance between neighbouring spurs at the foot, before Scale",
     "m", TerrainParameterType_Float, 700.0, 1.0, 100000.0, "", nullptr, 0},
    {"spurElongation", "Spur length",
     "How far the spurs are drawn out from the summit toward the foot", "",
     TerrainParameterType_Float, 0.7, 0.0, 0.95, "", nullptr, 0}};

/**
 * How much mass the flanks carry, as a factor on the profile's exponent. A low
 * bulk leaves a slender peak over hollow flanks; a high one stands the massif
 * well out from its crest before it falls away.
 */
float bulkExponentScale(int bulk) {
    if (bulk == 0) {
        return 1.45f;
    }
    if (bulk == 2) {
        return 0.7f;
    }
    return 1.0f;
}

/// How far the ground may be bent per unit of irregularity, as a share of the
/// distance the bend is drawn over. Past about a quarter the displacement's own
/// gradient exceeds one and the field folds over itself.
constexpr double mountainWarpShare = 0.25;
constexpr double mountainWarpFeatureShare = 0.6;

/// How far the foot may stand in or out from the nominal reach at full
/// irregularity. This is what makes the mass a massif rather than a cone: one
/// flank runs far out, the next breaks off short.
constexpr float outlineWanderLimit = 0.35f;

/// Roughly how many lobes the outline carries around the mountain. Fewer and
/// the mass is an oval; many more and the foot scallops instead of leaning.
constexpr double outlineLobeCount = 3.0;

/// Gradient noise spends most of its range near zero, so the raw field would
/// leave the outline all but circular. Lifting it and letting it saturate
/// trades the rare extremes for lobes that read, with no clip to crease the
/// flank where the reach would otherwise jump.
constexpr float outlineGain = 2.5f;

constexpr noise::FractalOptions outlineOptions{2, 0.5f, 2.0f};

/// How long the summit crest is, as a share of the reach to the foot. A summit
/// is the high point of an arête rather than the tip of a cone, so the mass
/// falls away from a line and the ground leaves it at an angle on every side.
constexpr double summitCrestShare = 0.3;

/* ---------------------------------------------------------------- gullies -- */

/**
 * The gullies cut into the flanks and the spurs left standing between them,
 * drawn the way Gaea's docs describe their own Mountain: a modulated cell
 * pattern under distortion.
 *
 * The field is read in a log-polar frame about the summit, stretched along the
 * outward axis, so every feature is a streak running down the slope and the
 * streaks are as fine near the summit as they are coarse at the foot. Gradient
 * noise in that frame is folded about zero: its zero set is a connected
 * network of lines, and folding puts a V-shaped gully floor along every one
 * of them, with a rounded spur between. Two gullies that enclose a spur meet
 * again below it, which is a confluence, so the network drains rather than
 * closing into hanging bowls the way a network of folded ridges would.
 *
 * Finer octaves lay smaller gullies between the large ones, and each octave
 * only comes in once its streaks are wider than a set ground distance, so
 * the network branches as it runs out from the summit.
 */
struct GullyField {
    /// How many streaks the coarsest octave carries around the summit.
    int countAround = 12;
    /// How far a streak runs toward the foot against its width around.
    double stretch = 3.0;
    /// The outward span of the frame from the summit to the foot, in the
    /// frame's own units.
    double outwardSpan = 4.0;
    int octaveCount = 6;
    float gain = 0.45f;
    float detailRetention = 1.0f;
    /// How steeply the ground climbs out of a gully floor onto the spur.
    float gullySharpness = 2.0f;
    /// How far below the crest the coarsest gullies come to their full depth.
    /// Each finer octave comes to its depth in half that distance, so the
    /// summit stands over faces cut by chutes while the main valleys open
    /// well below it.
    double holdMeters = 2000.0;
    /// How much of every octave cuts even on the crest, so the main valleys
    /// notch it with a col where each of them begins and the chutes between
    /// them score the crest band.
    float crestCut = 0.0f;
    /// Wander of the streaks on the ground, in streak widths, and the size
    /// of that wander in streak widths.
    float wanderStreaks = 0.5f;
    float wanderFeatureStreaks = 1.5f;
    /// How much the depth of the coarsest gullies varies slowly around the
    /// mountain, so a few valleys come to dominate and the rest sit between
    /// them, and the size of that variation in streak widths.
    float depthWanderShare = 0.6f;
    float depthWanderFeatureStreaks = 2.5f;
    std::uint64_t seed = 0;
};

/// A point in the frame the gullies are read in: turns around the summit and
/// stretched distance out from it.
struct PolarPoint {
    double around = 0.0;
    double outward = 0.0;
    /// The radius of the disc the crest maps to, scaled back to the ground.
    double radiusMeters = 0.0;
    /// Ground distance from the crest.
    double fromCrestMeters = 0.0;
};

/**
 * The frame about the summit crest rather than about a point. The crest is a
 * slit in the plane, and the Joukowski map takes the outside of a slit to the
 * outside of a disc without bending angles, so streaks drawn in log-polar
 * coordinates about that disc leave the crest at right angles all along it
 * and turn radial further out.
 */
PolarPoint polarAboutCrest(const RidgeSegment& crest, const GroundPoint& ground) {
    const GroundPoint middle{(crest.first.eastingMeters + crest.second.eastingMeters) * 0.5,
                             (crest.first.northingMeters + crest.second.northingMeters) * 0.5};
    const Heading along = headingFrom(crest.first, crest.second);
    const double halfLengthMeters =
        std::hypot(crest.second.eastingMeters - crest.first.eastingMeters,
                   crest.second.northingMeters - crest.first.northingMeters)
        * 0.5;
    const double east = ground.eastingMeters - middle.eastingMeters;
    const double north = ground.northingMeters - middle.northingMeters;
    const std::complex<double> slitPoint{(east * along.east + north * along.north) / halfLengthMeters,
                                         (north * along.east - east * along.north) / halfLengthMeters};
    const std::complex<double> unit =
        slitPoint + std::sqrt(slitPoint - 1.0) * std::sqrt(slitPoint + 1.0);
    const double turns = std::arg(unit) / (2.0 * std::numbers::pi) + 0.5;
    const double radiusMeters = std::abs(unit) * halfLengthMeters;
    const double fromCrestMeters = segmentDistance(
        ground.eastingMeters, ground.northingMeters, crest.first.eastingMeters,
        crest.first.northingMeters, crest.second.eastingMeters, crest.second.northingMeters);
    return PolarPoint{turns, std::log(std::max(std::abs(unit), 1.0)), radiusMeters,
                      fromCrestMeters};
}

/// How much of an octave survives when the finest detail is held back.
/// Retention counts octaves, so holding detail back drops the smallest
/// gullies outright and leaves the larger ones at full depth.
float gullyOctaveWeight(float detailRetention, int octave, int octaveCount) {
    const float keptOctaves = std::max(detailRetention * static_cast<float>(octaveCount), 1.0f);
    return std::clamp(keptOctaves - static_cast<float>(octave), 0.0f, 1.0f);
}

/// The wander laid over the streaks is read on the ground rather than in the
/// polar frame, because a field read in the frame would not close around the
/// loop and would seam the mountain along one ray.
struct StreakWander {
    float around = 0.0f;
    float outward = 0.0f;
    float depth = 1.0f;
};

StreakWander streakWanderAt(const GroundPoint& ground, double featureMeters,
                            const GullyField& gullies, std::uint64_t seed) {
    const float x = static_cast<float>(ground.eastingMeters / featureMeters);
    const float y = static_cast<float>(ground.northingMeters / featureMeters);
    constexpr noise::FractalOptions wanderOptions{3, 0.5f, 2.0f};
    const float depthScale = gullies.wanderFeatureStreaks / gullies.depthWanderFeatureStreaks;
    StreakWander wander;
    wander.around = noise::fractalPerlin(x, y, terrainSeedForLane(seed, 101), wanderOptions)
                    * gullies.wanderStreaks;
    wander.outward = noise::fractalPerlin(x, y, terrainSeedForLane(seed, 102), wanderOptions)
                     * gullies.wanderStreaks;
    // Scaled so the deepest gully is still at most the full depth, which
    // keeps every finer octave cutting rather than filling.
    wander.depth = (1.0f
                    + noise::perlin(x * depthScale, y * depthScale, terrainSeedForLane(seed, 103))
                          * gullies.depthWanderShare)
                   / (1.0f + gullies.depthWanderShare);
    return wander;
}

/// Gradient noise is exactly zero on every lattice corner, so folding one
/// octave alone would run a gully through every corner in a regular grid. A
/// second octave at twice the frequency moves the zero set off the corners.
float streakNoise(float x, float y, int countAround, std::uint64_t seed) {
    return noise::perlinPeriodic(x, y, seed, countAround) * 0.65f
           + noise::perlinPeriodic(x * 2.0f, y * 2.0f, terrainSeedForLane(seed, 7),
                                   countAround * 2) * 0.35f;
}

/// How much of the cut, from the top, is rounded off where it meets the
/// spur back.
constexpr float backRounding = 0.3f;

/// The cut eased onto the spur back: a straight fall down into the V of the
/// floor, and a parabola over the last stretch so the back is met without a
/// crease, which would draw the edge of every spur as a line.
float easedOntoBack(float cut) {
    if (cut < backRounding) {
        return cut * cut / (2.0f * backRounding) / (1.0f - backRounding * 0.5f);
    }
    return (cut - backRounding * 0.5f) / (1.0f - backRounding * 0.5f);
}

/// One octave of gullies: 1 on a gully floor, falling to 0 on the back of the
/// spur beside it.
float gullyOctave(const PolarPoint& polar, const StreakWander& wander, int countAround,
                  const GullyField& gullies, std::uint64_t seed) {
    const double streaksPerTurn = static_cast<double>(countAround);
    const float x = static_cast<float>(polar.around * streaksPerTurn) + wander.around;
    const float y = static_cast<float>(polar.outward * streaksPerTurn
                                       / (2.0 * std::numbers::pi * gullies.stretch))
                    + wander.outward;
    const float signal = streakNoise(x, y, countAround, seed);
    const float cut = std::max(1.0f - std::abs(signal) * gullies.gullySharpness, 0.0f);
    return easedOntoBack(cut);
}

/**
 * How deep the gullies cut at a point, 0 on the crest and 1 on the floor of
 * the deepest coarsest gully. A finer octave only cuts what the one before it
 * left standing, so the total never exceeds the coarsest octave's amplitude.
 */
float gullyDepthAt(const PolarPoint& polar, const GroundPoint& ground, double streakWidthMeters,
                   const GullyField& gullies) {
    float total = 0.0f;
    float amplitude = 1.0f;
    // A finer gully is a tributary: it cuts the back of the spur above it and
    // ends where it meets the floor of the coarser gully, so each octave is
    // weighted by how far the one before it left the ground standing.
    float standing = 1.0f;
    int countAround = gullies.countAround;
    double holdMeters = gullies.holdMeters;
    double featureMeters = streakWidthMeters * gullies.wanderFeatureStreaks;
    for (int octave = 0; octave < gullies.octaveCount; ++octave) {
        const float released = smoothStep(
            static_cast<float>(std::clamp(polar.fromCrestMeters / holdMeters, 0.0, 1.0)));
        const float hold = gullies.crestCut + (1.0f - gullies.crestCut) * released;
        const float weight = amplitude * hold
                             * gullyOctaveWeight(gullies.detailRetention, octave,
                                                 gullies.octaveCount);
        const std::uint64_t octaveSeed =
            terrainSeedForLane(gullies.seed, static_cast<std::uint64_t>(octave));
        const StreakWander wander = streakWanderAt(ground, featureMeters, gullies, octaveSeed);
        const float gully =
            gullyOctave(polar, wander, countAround, gullies, octaveSeed) * wander.depth;
        total += gully * standing * weight;
        standing = 1.0f - gully;
        holdMeters *= 0.5;
        featureMeters *= 0.5;
        amplitude *= gullies.gain;
        countAround *= 2;
    }
    return total;
}

/* ----------------------------------------------------------- the massif -- */


/// How much of the mass the floor of a gully may lose at full detail. The cut
/// is a share of the mass standing there, so the gullies of a tall mountain
/// are as deep as the mountain is tall, and the relief dies away with the
/// mass into the apron at the foot.
constexpr float gullyDepthLimit = 0.44f;

/// How many streaks the coarsest octave carries around the summit at least.
constexpr int leastStreaksAround = 4;

/// How far below the crest, as a share of the reach, the main valleys come to
/// their full depth.
constexpr double summitHoldShare = 0.22;

/// Everything about a mountain that is fixed before the first gully is read.
struct MountainLayout {
    ShapePlacement placement;
    DomainWarp warp;
    RidgeSegment summitCrest;
    GullyField gullies;
    double radiusMeters = 3000.0;
    double outlineFeatureSizeMeters = 3000.0;
    float outlineWanderShare = 0.0f;
    float profileExponent = 1.6f;
    float gullyDepth = 0.0f;
    float heightMeters = 1200.0f;
    float baseAltitudeMeters = 0.0f;
    std::uint64_t outlineSeed = 0;
    std::uint64_t crestSeed = 0;
};

/**
 * How far the foot of the mountain stands from the centre in the direction of
 * a point: the nominal reach bent by a slow field read around the mountain, so
 * the outline is a massif's rather than a circle's.
 *
 * The field is read on a ring about the centre rather than at the point
 * itself, so the reach depends on the direction alone. A reach that varied
 * along a ray as well would move the foot in and out under its own slope and
 * break the flank into terraces.
 */
double reachAt(const MountainLayout& layout, const GroundPoint& ground) {
    const GroundPoint around = ringReadPoint(ground, layout.placement, layout.radiusMeters, 1.0f);
    const float featureSize = static_cast<float>(layout.outlineFeatureSizeMeters);
    const float field =
        noise::fractalPerlin(static_cast<float>(around.eastingMeters) / featureSize,
                             static_cast<float>(around.northingMeters) / featureSize,
                             layout.outlineSeed, outlineOptions);
    const float wander = std::tanh(field * outlineGain);
    return layout.radiusMeters * (1.0 + layout.outlineWanderShare * wander);
}

/// A cone's profile: 1 at its crest and 0 at its foot, steep where it leaves
/// the crest and tangent to the plane at the foot.
float coneMass(double fromCrestMeters, double reachMeters, float exponent) {
    const double unit = std::clamp(fromCrestMeters / reachMeters, 0.0, 1.0);
    return static_cast<float>(std::pow(1.0 - unit, exponent));
}

/// How far the ends of the summit arête stand below the summit, as a share of
/// the height, so the crest is a line with one high point rather than a
/// level wall.
constexpr float crestFallShare = 0.12f;

/// How far the crest rises and falls along its length between the summit and
/// its ends, and how many such swells it carries.
constexpr float crestSwellShare = 0.05f;
constexpr float crestSwellsPerLength = 1.5f;

/// 1 at the summit in the middle of the crest, falling to `1 - crestFallShare`
/// at either end and beyond it, so the mass is highest at one point and the
/// massif runs longer along the crest than across it.
float crestProfile(const RidgeSegment& crest, const GroundPoint& ground, std::uint64_t seed) {
    const Heading along = headingFrom(crest.first, crest.second);
    const double halfLengthMeters =
        std::hypot(crest.second.eastingMeters - crest.first.eastingMeters,
                   crest.second.northingMeters - crest.first.northingMeters)
        * 0.5;
    const double middleEast = (crest.first.eastingMeters + crest.second.eastingMeters) * 0.5;
    const double middleNorth = (crest.first.northingMeters + crest.second.northingMeters) * 0.5;
    const double alongShare = ((ground.eastingMeters - middleEast) * along.east
                               + (ground.northingMeters - middleNorth) * along.north)
                              / halfLengthMeters;
    const float clamped = static_cast<float>(std::clamp(alongShare, -1.0, 1.0));
    const float squared = clamped * clamped;
    // The fall levels off at the ends of the crest, since a fall still going
    // where the profile stops changing would fold a crease across the whole
    // mass on a line through each end.
    const float fall = 1.0f - (1.0f - squared) * (1.0f - squared);
    // The swell is zero at the summit so the summit keeps its height, and
    // fades out at the ends so it never lifts an end above the fall.
    const float swell = noise::perlin(clamped * crestSwellsPerLength + 0.5f, 0.5f, seed)
                        * squared * (1.0f - squared);
    return 1.0f - crestFallShare * fall + crestSwellShare * swell * 4.0f;
}

/// The mass, 1 on the summit crest and 0 at the foot.
float massAt(const MountainLayout& layout, const GroundPoint& ground, double fromCrestMeters,
             double reachMeters) {
    return coneMass(fromCrestMeters, reachMeters, layout.profileExponent)
           * crestProfile(layout.summitCrest, ground, layout.crestSeed);
}

float mountainHeightAt(const MountainLayout& layout, double eastingMeters, double northingMeters) {
    const GroundPoint ground = warpedBy(layout.warp, eastingMeters, northingMeters);
    const double fromCrestMeters = segmentDistance(
        ground.eastingMeters, ground.northingMeters, layout.summitCrest.first.eastingMeters,
        layout.summitCrest.first.northingMeters, layout.summitCrest.second.eastingMeters,
        layout.summitCrest.second.northingMeters);
    const double reachMeters = reachAt(layout, ground);
    const float mass = massAt(layout, ground, fromCrestMeters, reachMeters);
    const PolarPoint polar = polarAboutCrest(layout.summitCrest, ground);
    const double streakWidthMeters =
        2.0 * std::numbers::pi * layout.radiusMeters / layout.gullies.countAround;
    const float gullies = gullyDepthAt(polar, ground, streakWidthMeters, layout.gullies);
    return mass * (1.0f - layout.gullyDepth * gullies) * layout.heightMeters
           + layout.baseAltitudeMeters;
}

/// The slow bend that takes the straightness out of the ridge lines.
DomainWarp mountainWarpFor(double radiusMeters, float irregularity, std::uint64_t seed) {
    DomainWarp warp;
    warp.eastingSeed = terrainSeedForLane(seed, 1);
    warp.northingSeed = terrainSeedForLane(seed, 2);
    warp.featureSizeMeters = static_cast<float>(radiusMeters * mountainWarpFeatureShare);
    warp.amountMeters = static_cast<float>(irregularity * warp.featureSizeMeters
                                           * mountainWarpShare);
    return warp;
}

/// The summit crest, run through the centre on a bearing of its own.
RidgeSegment summitCrestFor(const ShapePlacement& placement, double radiusMeters,
                            std::uint64_t seed) {
    std::uint64_t state = seed;
    const double bearingRadians = terrainRandomUnitFloat(&state) * 2.0 * std::numbers::pi;
    const Heading alongCrest{std::sin(bearingRadians), std::cos(bearingRadians)};
    const GroundPoint centre{placement.centreEastingMeters, placement.centreNorthingMeters};
    const double halfLengthMeters = radiusMeters * summitCrestShare * 0.5;
    return RidgeSegment{stepFrom(centre, alongCrest, -halfLengthMeters),
                        stepFrom(centre, alongCrest, halfLengthMeters)};
}

/// How long a coarsest streak is against its width, so that at full
/// elongation one spur runs the whole flank from summit to foot and at none
/// a streak is as long as it is wide.
double streakStretchFor(const GullyField& gullies, double elongation) {
    const double streakWidth = 2.0 * std::numbers::pi / gullies.countAround;
    return std::max(elongation * gullies.outwardSpan / streakWidth, 1.0);
}

MountainLayout mountainLayoutFrom(const TerrainContextApi* api, TerrainEvaluationContext* context,
                                  const WorldFrame& frame) {
    const double scale = api->parameterNumber(context, 0);
    const std::uint64_t seed = terrainSeedForLane(
        api->randomSeed(context), static_cast<std::uint64_t>(api->parameterNumber(context, 5)));
    const int bulk = static_cast<int>(api->parameterNumber(context, 3));
    const float irregularity = static_cast<float>(api->parameterNumber(context, 11));

    MountainLayout layout;
    layout.placement = placementFrom(frame, api->parameterNumber(context, 6),
                                     api->parameterNumber(context, 7), 0.0);
    layout.radiusMeters = api->parameterNumber(context, 8) * scale;
    layout.summitCrest =
        summitCrestFor(layout.placement, layout.radiusMeters, terrainSeedForLane(seed, 5));
    layout.profileExponent =
        static_cast<float>(api->parameterNumber(context, 10)) * bulkExponentScale(bulk);
    const double spacingMeters = api->parameterNumber(context, 13) * scale;
    layout.gullies.countAround = std::max(
        static_cast<int>(std::round(2.0 * std::numbers::pi * layout.radiusMeters / spacingMeters)),
        leastStreaksAround);
    layout.gullies.outwardSpan =
        std::log(2.0 * layout.radiusMeters / (layout.radiusMeters * summitCrestShare));
    layout.gullies.stretch = streakStretchFor(layout.gullies, api->parameterNumber(context, 14));
    layout.gullies.holdMeters = layout.radiusMeters * summitHoldShare;
    layout.gullies.detailRetention = 1.0f - static_cast<float>(api->parameterNumber(context, 4));
    layout.gullies.seed = terrainSeedForLane(seed, 3);
    layout.outlineFeatureSizeMeters = layout.radiusMeters * std::numbers::pi / outlineLobeCount;
    layout.outlineWanderShare = irregularity * outlineWanderLimit;
    layout.heightMeters = static_cast<float>(api->parameterNumber(context, 1));
    layout.baseAltitudeMeters = static_cast<float>(api->parameterNumber(context, 9));
    layout.gullyDepth = static_cast<float>(api->parameterNumber(context, 12)) * gullyDepthLimit;
    layout.warp = mountainWarpFor(layout.radiusMeters, irregularity, seed);
    layout.outlineSeed = terrainSeedForLane(seed, 4);
    layout.crestSeed = terrainSeedForLane(seed, 7);
    return layout;
}

void evaluateMountain(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const WorldFrame frame = worldFrameFor(api, context);
    const MountainLayout layout = mountainLayoutFrom(api, context, frame);
    float* output = api->outputField(context, 0);

    forEachRowRange(api, context, frame.size, [&](int firstRow, int lastRow) {
        writeLandformRows(frame, firstRow, lastRow, output,
                          [&](double eastingMeters, double northingMeters) {
                              return mountainHeightAt(layout, eastingMeters, northingMeters);
                          });
    });
}

const TerrainNodeDescriptor mountainDescriptor = {
    "Mountain",
    "Terrain",
    "A single mountain: a summit with spurs and gullies radiating to the foot",
    1,
    nullptr,
    0,
    landformOutputs,
    1,
    mountainParameters,
    15,
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
    {"irregularity", "Irregularity", "How far the spine and its ridges wander off straight", "",
     TerrainParameterType_Float, 0.4, 0.0, 1.0, "", nullptr, 0},
    {"detailStrength", "Valley depth",
     "How deep the valleys are cut between the ridges, as a share of the mass above them", "",
     TerrainParameterType_Float, 0.5, 0.0, 1.0, "", nullptr, 0},
    {"featureSize", "Valley spacing",
     "Ground distance between neighbouring valleys leaving the crest", "m",
     TerrainParameterType_Float, 1900.0, 1.0, 100000.0, "", nullptr, 0},
    {"baseAltitude", "Base altitude", "The altitude the foot of the range sits at", "m",
     TerrainParameterType_Float, 0.0, -12000.0, 12000.0, "", nullptr, 0},
    {"spurElongation", "Spur length",
     "How far the spurs are drawn out from the crest toward the foot", "",
     TerrainParameterType_Float, 0.7, 0.0, 0.95, "", nullptr, 0}};

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

float smoothFall(double share) {
    const float clamped = static_cast<float>(std::clamp(share, 0.0, 1.0));
    return 1.0f - clamped * clamped * (3.0f - 2.0f * clamped);
}

/// How many octaves of summits the crest carries below the coarsest, and how
/// each finer one shrinks. Every octave folds a gradient field about zero, so
/// the summits are the points the field crosses zero at: pointed, and spaced
/// as the field happens to cross rather than at one interval.
constexpr int crestOctaveCount = 3;
constexpr float crestGain = 0.5f;
/// How much the coarsest octave's summits are spread out or bunched by the
/// octave below them, so no two passes are alike.
constexpr float crestSharpness = 1.6f;
constexpr float crestTipRounding = 0.08f;

/// The crest line's own ups and downs, 1 on a summit and 0 in the deepest
/// pass, read along the spine at one coarsest summit per spacing.
float crestReliefAlong(double alongMeters, double spacingMeters, std::uint64_t seed) {
    float pass = 0.0f;
    float amplitude = 1.0f;
    float amplitudeSum = 0.0f;
    double frequency = 1.0 / spacingMeters;
    for (int octave = 0; octave < crestOctaveCount; ++octave) {
        const float x = static_cast<float>(alongMeters * frequency);
        const float field = noise::perlin(x, 0.5f, terrainSeedForLane(seed, octave));
        // Folded with a rounded tip: a sharp fold would put a crease down the
        // whole cross-section under every summit.
        const float folded = std::sqrt(field * field + crestTipRounding * crestTipRounding)
                             - crestTipRounding;
        pass += std::min(folded * crestSharpness, 1.0f) * amplitude;
        amplitudeSum += amplitude;
        amplitude *= crestGain;
        frequency *= 2.0;
    }
    return 1.0f - pass / amplitudeSum;
}

/**
 * How high the crest stands at a position along the range: summits and the
 * passes between them, the summits themselves standing at different heights
 * along the range, and the whole tapering off past either end over one
 * summit spacing.
 */
struct CrestProfile {
    double halfLengthMeters = 6000.0;
    double spacingMeters = 2400.0;
    float saddleHeight = 0.5f;
    float heightVariation = 0.4f;
    std::uint64_t seed = 0;
};

float crestHeightAlong(const CrestProfile& profile, double alongMeters) {
    const double beyondEnd = std::abs(alongMeters) - profile.halfLengthMeters;
    const float taper = smoothFall(beyondEnd / profile.spacingMeters);
    const float relief = crestReliefAlong(alongMeters, profile.spacingMeters, profile.seed);
    // The summits' own heights vary over a few spacings, so one stretch of
    // the range stands over the next.
    const float stature =
        noise::perlin(static_cast<float>(alongMeters / (profile.spacingMeters * 3.0)), 0.5f,
                      terrainSeedForLane(profile.seed, 9))
        * 0.5f
        + 0.5f;
    const float summit = 1.0f - profile.heightVariation * stature;
    return taper * summit * (1.0f - (1.0f - profile.saddleHeight) * (1.0f - relief));
}

/**
 * The frame the range's gullies are read in: a stadium about the spine, in
 * place of the disc a mountain's slit maps to.
 *
 * Along either flank the way round is the distance along the spine, so the
 * gullies keep one spacing from end to end however long the range is; past
 * either end it turns with the angle about the end, so the gullies fan out
 * round the caps. Distance out is linear over the half width, so a streak
 * keeps one width from the crest to the foot: a flank's valleys run parallel
 * down it, and it is the finer octaves dying out between them that thins
 * the network toward the foot.
 *
 * The two flanks mirror each other: a point on the west flank reads the
 * streaks at the same place along the spine as its twin on the east flank,
 * so the field is continuous across the spine and the valley that notches
 * the crest with a col runs down from it on both sides, the way a pass
 * drains both ways.
 */
struct RangeFrame {
    GroundPoint centre;
    RangeAxis axis;
    double halfLengthMeters = 6000.0;
    /// How far out the frame reaches: one unit of distance out.
    double halfWidthMeters = 2000.0;
    /// The radius the walk turns round at either end.
    double capRadiusMeters = 300.0;

    /// How far the mirrored walk runs from the near cap's apex to the far one's.
    double walkLengthMeters() const { return 2.0 * halfLengthMeters + 2.0 * capRadiusMeters; }
};

/// Where a point stands along and across the spine.
struct SpineOffset {
    double alongMeters = 0.0;
    double acrossMeters = 0.0;
};

SpineOffset spineOffsetOf(const RangeFrame& frame, const GroundPoint& ground) {
    const double east = ground.eastingMeters - frame.centre.eastingMeters;
    const double north = ground.northingMeters - frame.centre.northingMeters;
    return SpineOffset{east * frame.axis.alongEast + north * frame.axis.alongNorth,
                       east * frame.axis.acrossEast + north * frame.axis.acrossNorth};
}

/// The distance walked along the spine to reach the point nearest to
/// `offset`, continued round either cap by the angle about the end so the
/// walk turns back on itself at each cap's apex.
double stadiumWalkTo(const RangeFrame& frame, const SpineOffset& offset) {
    const double halfLength = frame.halfLengthMeters;
    const double beyondEnd = std::abs(offset.alongMeters) - halfLength;
    if (beyondEnd <= 0.0) {
        return offset.alongMeters + halfLength;
    }
    const double turn = std::atan2(beyondEnd, std::abs(offset.acrossMeters));
    const double capWalk = frame.capRadiusMeters * std::sin(turn);
    if (offset.alongMeters > 0.0) {
        return 2.0 * halfLength + capWalk;
    }
    return -capWalk;
}

PolarPoint polarAboutSpine(const RangeFrame& frame, const GroundPoint& ground) {
    const SpineOffset offset = spineOffsetOf(frame, ground);
    const double beyondEnd =
        std::max(std::abs(offset.alongMeters) - frame.halfLengthMeters, 0.0);
    const double fromCrestMeters = std::hypot(beyondEnd, offset.acrossMeters);
    return PolarPoint{stadiumWalkTo(frame, offset) / frame.walkLengthMeters(),
                      fromCrestMeters / frame.halfWidthMeters,
                      frame.capRadiusMeters + fromCrestMeters, fromCrestMeters};
}

/// The radius the gullies fan round at either end of the spine, as a share
/// of the half width.
constexpr double rangeCapShare = 0.15;

/// How long the spine's meanders are, in summit spacings, and how far it
/// strays per unit of irregularity as a share of that length. Past about a
/// sixth the shift's own gradient exceeds one and a bend folds over.
constexpr double rangeMeanderSpacings = 2.5;
constexpr double rangeMeanderShare = 0.12;

/// How far a streak at full elongation runs, in half widths. Well past the
/// foot, because the floor of every valley is a zero line of the folded
/// field, and a zero line that turns back within the flank closes into a
/// ring round a knoll instead of running out to the foot.
constexpr double rangeStreakReachShare = 3.0;

/// How steeply the ground climbs out of a valley floor onto the spur. Low
/// enough that a spur is a ridge line rather than a rounded back, so the
/// cross-section of a flank is ridge, valley, ridge all the way along.
constexpr float rangeGullySharpness = 2.0f;

/// How much of the main valleys' depth notches the crest where they begin.
constexpr float rangeColShare = 0.2f;

/// The deepest a valley may be cut, as a share of the mass above its floor.
/// Cutting to the base leaves walls no rock stands at.
constexpr float deepestValleyShare = 0.85f;

/// How far down the flank, as a share of the half width, the main valleys
/// come to their full depth. A range's valleys open as they run out: shallow
/// notches in the crest, deep troughs whose floors run out to the base at
/// the foot while the spurs between them still stand.
constexpr double rangeValleyOpeningShare = 0.5;

/// The spurs of a range keep their height well out from the crest, so the
/// profile they fall on is close to straight where a lone mountain's is
/// concave; the sharpness parameter is scaled down to suit.
constexpr float rangeSpurExponentScale = 0.65f;

/// How far the foot may stand in or out from the nominal half width at full
/// irregularity, and how many such lobes fit round the range per half width.
constexpr float rangeFootWanderLimit = 0.35f;
constexpr double rangeFootLobesPerHalfWidth = 1.0;

/**
 * Everything about a range that is fixed before the first cell is read.
 */
struct RangeLayout {
    RangeFrame frame;
    DomainWarp warp;
    GullyField gullies;
    CrestProfile crest;
    /// The spine's wander across the range, read along it.
    double meanderFeatureMeters = 6000.0;
    double meanderMeters = 0.0;
    std::uint64_t meanderSeed = 0;
    double halfWidthMeters = 2000.0;
    double summitSpacingMeters = 2400.0;
    double streakWidthMeters = 1900.0;
    int footLobeCount = 4;
    float footWanderShare = 0.0f;
    float profileExponent = 1.6f;
    float valleyDepth = 0.0f;
    float heightMeters = 1800.0f;
    float baseAltitudeMeters = 0.0f;
    std::uint64_t footSeed = 0;
};

/**
 * How far the foot stands from the spine in the direction of a point: the
 * half width bent by a slow field read round the crest zone, so one flank
 * runs out further than the next. The field closes round the stadium, and
 * it depends only on where the point is round the range, never on how far
 * out it is, so the reach never terraces a flank.
 */
double rangeReachAt(const RangeLayout& layout, const PolarPoint& polar) {
    const float x = static_cast<float>(polar.around * layout.footLobeCount);
    const float field =
        noise::perlinPeriodic(x, 0.5f, layout.footSeed, layout.footLobeCount) * 0.65f
        + noise::perlinPeriodic(x * 2.0f, 0.5f, terrainSeedForLane(layout.footSeed, 1),
                                layout.footLobeCount * 2)
              * 0.35f;
    const float wander = std::tanh(field * outlineGain);
    return layout.halfWidthMeters * (1.0 + layout.footWanderShare * wander);
}

/**
 * The ground shifted across the range by a slow field read along it, so the
 * spine meanders. Every point at one position along shifts by the same
 * amount, so the shift never folds the ground however far the spine strays.
 */
GroundPoint meanderedBy(const RangeLayout& layout, const GroundPoint& ground) {
    const SpineOffset offset = spineOffsetOf(layout.frame, ground);
    const float field = noise::fractalPerlin(
        static_cast<float>(offset.alongMeters / layout.meanderFeatureMeters), 0.5f,
        layout.meanderSeed, noise::FractalOptions{3, 0.5f, 2.0f});
    const double shift = field * layout.meanderMeters;
    return GroundPoint{ground.eastingMeters - layout.frame.axis.acrossEast * shift,
                       ground.northingMeters - layout.frame.axis.acrossNorth * shift};
}

float rangeHeightAt(const RangeLayout& layout, double eastingMeters, double northingMeters) {
    const GroundPoint ground =
        meanderedBy(layout, warpedBy(layout.warp, eastingMeters, northingMeters));
    const PolarPoint polar = polarAboutSpine(layout.frame, ground);
    const SpineOffset offset = spineOffsetOf(layout.frame, ground);
    const float crest = crestHeightAlong(layout.crest, offset.alongMeters);
    const float mass = coneMass(polar.fromCrestMeters, rangeReachAt(layout, polar),
                                layout.profileExponent)
                       * crest;
    const float gullies =
        gullyDepthAt(polar, ground, layout.streakWidthMeters, layout.gullies);
    return mass * (1.0f - layout.valleyDepth * gullies) * layout.heightMeters
           + layout.baseAltitudeMeters;
}

RangeLayout rangeLayoutFrom(const TerrainContextApi* api, TerrainEvaluationContext* context,
                            const WorldFrame& frame) {
    const double lengthMeters = api->parameterNumber(context, 3);
    const int peakCount = static_cast<int>(api->parameterNumber(context, 5));
    const std::uint64_t seed = terrainSeedForLane(
        api->randomSeed(context), static_cast<std::uint64_t>(api->parameterNumber(context, 10)));
    const float irregularity = static_cast<float>(api->parameterNumber(context, 11));
    const ShapePlacement placement = placementFrom(frame, api->parameterNumber(context, 0),
                                                   api->parameterNumber(context, 1), 0.0);

    RangeLayout layout;
    layout.halfWidthMeters = api->parameterNumber(context, 4) * 0.5;
    layout.frame.centre = GroundPoint{placement.centreEastingMeters, placement.centreNorthingMeters};
    layout.frame.axis = axisFor(api->parameterNumber(context, 2));
    layout.frame.halfLengthMeters = lengthMeters * 0.5;
    layout.frame.halfWidthMeters = layout.halfWidthMeters;
    layout.frame.capRadiusMeters = layout.halfWidthMeters * rangeCapShare;
    layout.summitSpacingMeters = lengthMeters / peakCount;
    layout.crest.halfLengthMeters = layout.frame.halfLengthMeters;
    layout.crest.spacingMeters = layout.summitSpacingMeters;
    layout.crest.heightVariation = static_cast<float>(api->parameterNumber(context, 7));
    layout.crest.saddleHeight = static_cast<float>(api->parameterNumber(context, 8));
    layout.crest.seed = terrainSeedForLane(seed, 4);
    layout.meanderFeatureMeters = layout.summitSpacingMeters * rangeMeanderSpacings;
    layout.meanderMeters = irregularity * layout.meanderFeatureMeters * rangeMeanderShare;
    layout.meanderSeed = terrainSeedForLane(seed, 8);
    layout.profileExponent =
        static_cast<float>(api->parameterNumber(context, 9)) * rangeSpurExponentScale;
    layout.valleyDepth =
        static_cast<float>(api->parameterNumber(context, 12)) * deepestValleyShare;
    layout.heightMeters = static_cast<float>(api->parameterNumber(context, 6));
    layout.baseAltitudeMeters = static_cast<float>(api->parameterNumber(context, 14));

    const double walkMeters = layout.frame.walkLengthMeters();
    layout.streakWidthMeters = api->parameterNumber(context, 13);
    layout.gullies.countAround = std::max(
        static_cast<int>(std::round(walkMeters / layout.streakWidthMeters)),
        leastStreaksAround);
    layout.gullies.outwardSpan = rangeStreakReachShare;
    layout.gullies.stretch = streakStretchFor(layout.gullies, api->parameterNumber(context, 15));
    layout.gullies.holdMeters = layout.halfWidthMeters * rangeValleyOpeningShare;
    layout.gullies.crestCut = rangeColShare;
    layout.gullies.gullySharpness = rangeGullySharpness;
    layout.gullies.seed = terrainSeedForLane(seed, 3);

    layout.footLobeCount = std::max(
        static_cast<int>(std::round(walkMeters / layout.halfWidthMeters
                                    * rangeFootLobesPerHalfWidth)),
        2);
    layout.footWanderShare = irregularity * rangeFootWanderLimit;
    layout.footSeed = terrainSeedForLane(seed, 6);

    layout.warp = mountainWarpFor(layout.halfWidthMeters, irregularity, seed);
    return layout;
}

void evaluateMountainRange(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const WorldFrame frame = worldFrameFor(api, context);
    const RangeLayout layout = rangeLayoutFrom(api, context, frame);
    float* output = api->outputField(context, 0);

    forEachRowRange(api, context, frame.size, [&](int firstRow, int lastRow) {
        writeLandformRows(frame, firstRow, lastRow, output,
                          [&](double eastingMeters, double northingMeters) {
                              return rangeHeightAt(layout, eastingMeters, northingMeters);
                          });
    });
}

const TerrainNodeDescriptor mountainRangeDescriptor = {
    "MountainRange",
    "Terrain",
    "A chain of summits on one spine, with spurs running out to either foot and the valleys between them",
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
