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
    {"featureSize", "Spur spacing",
     "Ground distance between neighbouring major spurs at the foot, before Scale", "m",
     TerrainParameterType_Float, 1400.0, 1.0, 100000.0, "", nullptr, 0},
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
/// The bend is drawn over the whole reach rather than over a fraction of it,
/// so it curves the major spurs along their length instead of only ruffling
/// the ground they stand on.
constexpr double mountainWarpFeatureShare = 1.0;

/// How far the foot may stand in or out from the nominal reach at full
/// irregularity. This is what makes the mass a massif rather than a cone: one
/// flank runs far out, the next breaks off short.
constexpr float outlineWanderLimit = 0.45f;

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
/// Short, so the top reads as a peak on a ridge rather than as a bar.
constexpr double summitCrestShare = 0.12;

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
    /// How far from the crest the coarsest octave's streaks have merged into
    /// the ground below them and stop cutting; each finer octave merges that
    /// much sooner. This is what thins the network downslope, so a ring drawn
    /// far out crosses fewer and wider valleys than one drawn close in.
    /// Zero leaves every octave cutting all the way to the foot.
    double coarsestMergeMeters = 0.0;
    /// The narrowest a streak may be on the ground before its octave fades
    /// out. Streaks in a frame about the summit thin toward it without bound,
    /// and unbounded thinning draws the flanks as fur instead of as rock.
    /// Zero leaves every octave cutting however fine it becomes.
    double finestStreakMeters = 0.0;
    std::uint64_t seed = 0;
};

/// A point in the frame the gullies are read in: turns around the summit and
/// stretched distance out from it.
struct PolarPoint {
    double around = 0.0;
    double outward = 0.0;
    /// How far a walk once around the frame covers on the ground where this
    /// point stands, which is what turns a streak count into a ground width.
    double aroundSpanMeters = 0.0;
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
    // The map is the inverse of z = (w + 1/w) / 2, so a turn about the disc
    // covers pi * halfLength * |w - 1/w| on the ground: two pi r far out, and
    // closing to nothing against the crest, where the whole slit is one turn.
    const double aroundSpanMeters =
        std::numbers::pi * halfLengthMeters * std::abs(unit - 1.0 / unit);
    const double fromCrestMeters = segmentDistance(
        ground.eastingMeters, ground.northingMeters, crest.first.eastingMeters,
        crest.first.northingMeters, crest.second.eastingMeters, crest.second.northingMeters);
    return PolarPoint{turns, std::log(std::max(std::abs(unit), 1.0)), aroundSpanMeters,
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
    // Two octaves: a third would ruffle the edge of a streak at a finer scale
    // than the streak itself, which draws the flank as fur rather than rock.
    constexpr noise::FractalOptions wanderOptions{2, 0.5f, 2.0f};
    const float depthScale = gullies.wanderFeatureStreaks / gullies.depthWanderFeatureStreaks;
    StreakWander wander;
    wander.around = noise::fractalPerlin(x, y, terrainSeedForLane(seed, 101), wanderOptions)
                    * gullies.wanderStreaks;
    // A step along the frame's outward axis covers `stretch` times the ground
    // a step around it does, so the outward wander is divided by the stretch
    // to bend a streak by the same distance either way. Left undivided it
    // would slide the pattern up and down the slope by whole flank lengths.
    wander.outward = noise::fractalPerlin(x, y, terrainSeedForLane(seed, 102), wanderOptions)
                     * gullies.wanderStreaks / static_cast<float>(gullies.stretch);
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
    return noise::perlinPeriodic(x, y, seed, countAround) * 0.82f
           + noise::perlinPeriodic(x * 2.0f, y * 2.0f, terrainSeedForLane(seed, 7),
                                   countAround * 2) * 0.18f;
}

/// How much of the cut, from the top, is rounded off where it meets the
/// spur back.
constexpr float backRounding = 0.12f;

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

/// How much sooner each finer octave merges into the network below it than the
/// octave above it does. Below two a finer generation outlives half its
/// parent's reach, so the drainage stays as dense at the foot as it is in the
/// headwaters and no valley ever comes to dominate.
constexpr double gullyMergeRate = 1.25;

/// How far an octave has merged away: 1 well inside its reach, 0 past it,
/// faded over the outer half so a tributary shallows into the spur back it
/// runs down rather than ending at a step.
float octaveReaches(double fromCrestMeters, double mergeMeters) {
    if (mergeMeters <= 0.0) {
        return 1.0f;
    }
    const double left = (mergeMeters - fromCrestMeters) / (mergeMeters * 0.5);
    return smoothStep(static_cast<float>(std::clamp(left, 0.0, 1.0)));
}

/// How far an octave has come in: 0 where its streaks are narrower than the
/// finest gully the massif carries, 1 once they are twice that wide.
float octaveWidens(double streakMeters, double finestStreakMeters) {
    if (finestStreakMeters <= 0.0) {
        return 1.0f;
    }
    const double widths = streakMeters / finestStreakMeters - 1.0;
    return smoothStep(static_cast<float>(std::clamp(widths, 0.0, 1.0)));
}

/// How many gullies one streak carries. Gradient noise crosses zero about
/// twice per lattice period along any line, and the fold puts a gully floor
/// along every crossing, so a lattice laid out at one streak per spur draws
/// twice as many valleys as it has streaks.
constexpr int gulliesPerStreak = 2;

/// The ground distance between neighbouring gullies of an octave where a point
/// stands.
double gullySpacingAt(const PolarPoint& polar, int countAround) {
    return polar.aroundSpanMeters / (countAround * gulliesPerStreak);
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
    double mergeMeters = gullies.coarsestMergeMeters;
    double featureMeters = streakWidthMeters * gullies.wanderFeatureStreaks;
    for (int octave = 0; octave < gullies.octaveCount; ++octave) {
        const float released = smoothStep(
            static_cast<float>(std::clamp(polar.fromCrestMeters / holdMeters, 0.0, 1.0)));
        const float hold = gullies.crestCut + (1.0f - gullies.crestCut) * released;
        const float present = octaveReaches(polar.fromCrestMeters, mergeMeters)
                              * octaveWidens(gullySpacingAt(polar, countAround),
                                             gullies.finestStreakMeters);
        const float weight = amplitude * hold * present
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
        mergeMeters /= gullyMergeRate;
        featureMeters *= 0.5;
        amplitude *= gullies.gain;
        countAround *= 2;
    }
    return total;
}

/* ----------------------------------------------------------- the massif -- */

/// How many streaks the coarsest octave carries around the summit at least.
constexpr int leastStreaksAround = 4;

/* ------------------------------------------------------ ridge skeleton -- */

/**
 * The major ridges of the massif: a few main ridges leaving the summit and
 * bending as they run down toward the foot, and a shorter spur branching off
 * each of them part way down. A cone hung off the summit alone draws every
 * valley into one point; the ridges give the massif the few great spurs and
 * the bays between them that the gullies are then cut into.
 *
 * Each ridge holds the ground up as though the summit were reached along it:
 * the distance the mass falls over is the distance run along the ridge plus
 * a multiple of the distance off it, so a ridge stands as a spur with steeper
 * flanks to either side, and nowhere stands higher than the cone it lies in.
 */
constexpr int ridgeSegmentCount = 4;
constexpr int fewestMainRidges = 3;
constexpr int mostMainRidges = 5;
constexpr int mostRidges = mostMainRidges * 2;

struct RidgeLine {
    std::array<GroundPoint, ridgeSegmentCount + 1> points{};
    /// How far along the ridges from the summit the line's first point lies.
    double startMeters = 0.0;
    std::uint64_t seed = 0;
};

struct RidgeSkeleton {
    std::array<RidgeLine, mostRidges> lines{};
    int lineCount = 0;
};

/// Everything about a mountain that is fixed before the first cell is read.
struct MountainLayout {
    ShapePlacement placement;
    DomainWarp warp;
    RidgeSegment summitCrest;
    RidgeSkeleton ridges;
    double radiusMeters = 3000.0;
    double outlineFeatureSizeMeters = 3000.0;
    float outlineWanderShare = 0.0f;
    float profileExponent = 1.6f;
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

/// How far out the rock core of a massif stands, as a share of the reach, and
/// how much of the height is carried instead by the apron of shed ground that
/// runs on past that core to the foot. The apron's own profile is flatter than
/// any rock slope, so a massif meets the plain across a long low-angle skirt
/// rather than dropping its spurs straight onto it.
constexpr double coreReachShare = 0.93;
constexpr float apronHeightShare = 0.14f;
constexpr float apronExponent = 2.0f;
constexpr double coreEaseShare = 0.3;

/// A massif's profile: a steep rock core standing over a low-angle apron, 1 on
/// the crest and 0 at the foot, tangent to the plane where each of the two
/// comes to its own end.
float massifMass(double fromCrestMeters, double reachMeters, float exponent) {
    // The core eases onto the apron over the last stretch of its reach rather
    // than ending at a crease in its slope, which a hillshade draws as a ring
    // around the whole foot.
    const double coreLeft = 1.0 - fromCrestMeters / (reachMeters * coreReachShare);
    const float coreEase =
        smoothStep(static_cast<float>(std::clamp(coreLeft / coreEaseShare, 0.0, 1.0)));
    const float core =
        coneMass(fromCrestMeters, reachMeters * coreReachShare, exponent) * coreEase;
    const float apron = coneMass(fromCrestMeters, reachMeters, apronExponent);
    return core * (1.0f - apronHeightShare) + apron * apronHeightShare;
}

/// How far the summit stands above the rest of the crest, as a share of the
/// height, and over what distance from the summit, as a share of the reach,
/// that fall is spent. It is read from the summit point rather than along the
/// crest, so it falls alike on every side and lays no band across the massif.
constexpr float crestFallShare = 0.14f;
constexpr double crestFallReachShare = 0.5;

/// How deep the cols along the crest are cut below the line from the summit to
/// its ends, how many of them it carries, and how far out from the crest they
/// still shape the ground, in crest half lengths.
///
/// The cols depend on where a point stands along the crest, so anything drawn
/// into them runs right across the massif on a line: they have to be smooth,
/// and they have to die out within a crest length of the crest, or the
/// summit's own relief creases both flanks to the foot.
constexpr float crestColShare = 0.08f;
constexpr float crestColsPerLength = 2.2f;
constexpr double crestColReachShare = 1.0;

/// 1 at the summit in the middle of the crest, falling away from it on every
/// side, with shallow cols notched into the crest either side of the summit.
float crestProfile(const RidgeSegment& crest, const GroundPoint& ground, double fromCrestMeters,
                   double fallReachMeters, std::uint64_t seed) {
    const Heading along = headingFrom(crest.first, crest.second);
    const double halfLengthMeters =
        std::hypot(crest.second.eastingMeters - crest.first.eastingMeters,
                   crest.second.northingMeters - crest.first.northingMeters)
        * 0.5;
    const double fromMiddleEast =
        ground.eastingMeters - (crest.first.eastingMeters + crest.second.eastingMeters) * 0.5;
    const double fromMiddleNorth =
        ground.northingMeters - (crest.first.northingMeters + crest.second.northingMeters) * 0.5;
    const float fall = smoothStep(static_cast<float>(
        std::clamp(std::hypot(fromMiddleEast, fromMiddleNorth) / fallReachMeters, 0.0, 1.0)));
    const double alongShare =
        (fromMiddleEast * along.east + fromMiddleNorth * along.north) / halfLengthMeters;
    const float clamped = static_cast<float>(std::clamp(alongShare, -1.0, 1.0));
    // The cols close to nothing at the summit, so the summit is the one point
    // on the crest that keeps the whole height and nothing stands above it,
    // and to nothing a crest length out, so they stay a feature of the crest.
    const float nearCrest = 1.0f - smoothStep(static_cast<float>(std::clamp(
                                       fromCrestMeters / (halfLengthMeters * crestColReachShare),
                                       0.0, 1.0)));
    const float cols = (0.5f + 0.5f * noise::perlin(clamped * crestColsPerLength + 0.5f, 0.5f, seed))
                       * clamped * clamped * nearCrest;
    return 1.0f - crestFallShare * fall - crestColShare * cols;
}

/// How far a main ridge runs, as a share of the reach, before its length is
/// varied, and how far its branch runs.
constexpr double mainRidgeReachShare = 0.85;
constexpr double branchRidgeReachShare = 0.45;

/// How far a ridge may turn at each of its bends.
constexpr double ridgeBendRadians = 0.55;

/// How far off a main ridge's line its branch leaves, either side.
constexpr double branchLeastAngleRadians = 0.55;
constexpr double branchMostAngleRadians = 1.0;

/// How much faster the ground falls off a ridge sideways than along it.
constexpr double ridgeFlankSteepness = 2.5;

/// How wide a ridge's crest is rounded, as a share of the radius. A knife-edge
/// crest turns the fall over within a cell, and every gully octave that fades
/// out against it leaves its own line along the crest.
constexpr double ridgeCrestRoundingShare = 0.04;

/// How much of the cone's height the ground between the ridges keeps.
constexpr float betweenRidgesShare = 0.8f;

/// How far shoulders stand above and cols sink below a ridge's even fall, as
/// a share of the mass there, how far apart they come along the ridge as a
/// share of the reach, and how far below the summit they begin.
constexpr float shoulderShare = 0.07f;
constexpr double shoulderSpacingShare = 0.3;
constexpr double shoulderOnsetShare = 0.15;

/// How far the rounding reaches where two ridges, or a ridge and the ground
/// between them, meet, in shares of the height. A hard meeting creases the
/// massif and turns its fall over at once along the crease.
constexpr float ridgeMeetingRounding = 0.05f;

RidgeLine ridgeLineFrom(const GroundPoint& start, double startMeters, double bearingRadians,
                        double lengthMeters, std::uint64_t seed) {
    std::uint64_t state = seed;
    RidgeLine line;
    line.startMeters = startMeters;
    line.seed = terrainSeedForLane(seed, 1);
    line.points[0] = start;
    Heading heading{std::sin(bearingRadians), std::cos(bearingRadians)};
    const double stepMeters = lengthMeters / ridgeSegmentCount;
    for (int point = 1; point <= ridgeSegmentCount; ++point) {
        heading = heading.turned(spreadFrom(&state, ridgeBendRadians));
        line.points[point] = stepFrom(line.points[point - 1], heading, stepMeters);
    }
    return line;
}

/// The branch of a main ridge: leaving it at one of its inner bends, to one
/// side, on a bearing turned off the main ridge's own.
RidgeLine branchRidgeFrom(const RidgeLine& main, double radiusMeters, std::uint64_t seed) {
    std::uint64_t state = seed;
    const int fork = 1 + static_cast<int>(terrainRandomUnitFloat(&state) * 2.0f);
    const Heading along = headingFrom(main.points[fork], main.points[fork + 1]);
    double turnRadians = branchLeastAngleRadians
                         + terrainRandomUnitFloat(&state)
                               * (branchMostAngleRadians - branchLeastAngleRadians);
    if (terrainRandomUnitFloat(&state) < 0.5f) {
        turnRadians = -turnRadians;
    }
    const Heading branch = along.turned(turnRadians);
    const double stepMeters = std::hypot(main.points[1].eastingMeters - main.points[0].eastingMeters,
                                         main.points[1].northingMeters - main.points[0].northingMeters);
    const double lengthMeters =
        radiusMeters * branchRidgeReachShare * (0.75 + 0.5 * terrainRandomUnitFloat(&state));
    return ridgeLineFrom(main.points[fork], main.startMeters + stepMeters * fork,
                         std::atan2(branch.east, branch.north), lengthMeters,
                         terrainSeedForLane(seed, 2));
}

RidgeSkeleton ridgeSkeletonFor(const ShapePlacement& placement, double radiusMeters,
                               std::uint64_t seed) {
    std::uint64_t state = seed;
    const int mainCount =
        fewestMainRidges
        + static_cast<int>(terrainRandomUnitFloat(&state) * (mostMainRidges - fewestMainRidges + 1));
    const double spacingRadians = 2.0 * std::numbers::pi / mainCount;
    const double firstBearingRadians = terrainRandomUnitFloat(&state) * spacingRadians;
    const GroundPoint summit{placement.centreEastingMeters, placement.centreNorthingMeters};
    RidgeSkeleton skeleton;
    for (int ridge = 0; ridge < std::min(mainCount, mostMainRidges); ++ridge) {
        const double bearingRadians = firstBearingRadians + ridge * spacingRadians
                                      + spreadFrom(&state, spacingRadians * 0.3);
        const double lengthMeters =
            radiusMeters * mainRidgeReachShare * (0.8 + 0.4 * terrainRandomUnitFloat(&state));
        const std::uint64_t ridgeSeed = terrainSeedForLane(seed, static_cast<std::uint64_t>(ridge) + 1);
        const RidgeLine main = ridgeLineFrom(summit, 0.0, bearingRadians, lengthMeters, ridgeSeed);
        skeleton.lines[skeleton.lineCount++] = main;
        skeleton.lines[skeleton.lineCount++] =
            branchRidgeFrom(main, radiusMeters, terrainSeedForLane(ridgeSeed, 3));
    }
    return skeleton;
}

/// The larger of two heights, rounded over `rounding` where they meet.
float smoothMaximum(float first, float second, float rounding) {
    const float blend = std::clamp(0.5f + 0.5f * (first - second) / rounding, 0.0f, 1.0f);
    return second + (first - second) * blend + rounding * blend * (1.0f - blend);
}

/// How far a ridge stands above or below its even fall where a point lies
/// along it: shoulders and cols, none close under the summit.
float shoulderAt(const RidgeLine& line, double alongMeters, double radiusMeters) {
    const float onset = smoothStep(static_cast<float>(
        std::clamp(alongMeters / (radiusMeters * shoulderOnsetShare), 0.0, 1.0)));
    const float bumps = noise::perlin(
        static_cast<float>(alongMeters / (radiusMeters * shoulderSpacingShare)), 0.5f, line.seed);
    return 1.0f + shoulderShare * bumps * onset;
}

/**
 * How high a ridge holds the ground at a point: the highest of what each of
 * its segments holds, each falling from the summit along the ridge to where
 * the point leaves the segment and then off it, steeper to the side. Taking
 * the highest over every segment, rather than reading the nearest segment,
 * keeps the height continuous on the inside of a bend, where the nearest
 * segment changes from one point to the next.
 */
float ridgeHeightAt(const RidgeLine& line, const GroundPoint& ground, double reachMeters,
                    float exponent, double radiusMeters) {
    float highest = 0.0f;
    double runMeters = line.startMeters;
    const double roundingMeters = radiusMeters * ridgeCrestRoundingShare;
    for (int segment = 0; segment < ridgeSegmentCount; ++segment) {
        const GroundPoint& first = line.points[segment];
        const GroundPoint& second = line.points[segment + 1];
        const double east = second.eastingMeters - first.eastingMeters;
        const double north = second.northingMeters - first.northingMeters;
        const double lengthMeters = std::max(std::hypot(east, north), 1.0e-9);
        const double along = std::clamp(((ground.eastingMeters - first.eastingMeters) * east
                                         + (ground.northingMeters - first.northingMeters) * north)
                                            / (lengthMeters * lengthMeters),
                                        0.0, 1.0);
        const double offMeters =
            std::hypot(ground.eastingMeters - first.eastingMeters - east * along,
                       ground.northingMeters - first.northingMeters - north * along);
        const double alongMeters = runMeters + along * lengthMeters;
        const double roundedOffMeters =
            std::hypot(offMeters, roundingMeters) - roundingMeters;
        const float held =
            massifMass(alongMeters + roundedOffMeters * ridgeFlankSteepness, reachMeters, exponent)
            * shoulderAt(line, alongMeters, radiusMeters);
        highest = std::max(highest, held);
        runMeters += lengthMeters;
    }
    return highest;
}

double fromCrestAt(const MountainLayout& layout, const GroundPoint& ground) {
    return segmentDistance(ground.eastingMeters, ground.northingMeters,
                           layout.summitCrest.first.eastingMeters,
                           layout.summitCrest.first.northingMeters,
                           layout.summitCrest.second.eastingMeters,
                           layout.summitCrest.second.northingMeters);
}

/// The smooth massif before any gully is cut, 1 on the summit and 0 at the
/// foot.
float massifAt(const MountainLayout& layout, double eastingMeters, double northingMeters) {
    const GroundPoint ground = warpedBy(layout.warp, eastingMeters, northingMeters);
    const double fromCrestMeters = fromCrestAt(layout, ground);
    const double reachMeters = reachAt(layout, ground);
    const float cone = massifMass(fromCrestMeters, reachMeters, layout.profileExponent)
                       * crestProfile(layout.summitCrest, ground, fromCrestMeters,
                                      layout.radiusMeters * crestFallReachShare, layout.crestSeed);
    float height = cone * betweenRidgesShare;
    for (int line = 0; line < layout.ridges.lineCount; ++line) {
        const float ridge = ridgeHeightAt(layout.ridges.lines[line], ground, reachMeters,
                                          layout.profileExponent, layout.radiusMeters);
        height = smoothMaximum(height, ridge, ridgeMeetingRounding);
    }
    // The ridges meet the summit under the crest's own profile, so nothing
    // stands above the summit.
    return std::min(height, cone);
}

/// The massif's slope where a point stands, read across a given distance, in
/// shares of the mountain's height per share of its radius, so a flank of the
/// mountain's own mean steepness has a slope of about one whatever its size.
/// Also how fast the direction of the fall turns there, in radians per metre:
/// slowly down an open flank, fast across a crest or about the summit.
struct SlopeReading {
    float slopeEast = 0.0f;
    float slopeNorth = 0.0f;
    float turnPerMeter = 0.0f;
};

/// The fall of the massif from a nine-point stencil: its gradient, and the
/// rate the gradient's direction turns, the curvature across the fall over the
/// square of the slope.
SlopeReading massifSlopeAt(const MountainLayout& layout, double eastingMeters,
                           double northingMeters, float centre, double stepMeters) {
    const auto at = [&](double eastSteps, double northSteps) {
        return static_cast<double>(massifAt(layout, eastingMeters + eastSteps * stepMeters,
                                            northingMeters + northSteps * stepMeters));
    };
    const double east = at(1.0, 0.0);
    const double west = at(-1.0, 0.0);
    const double north = at(0.0, 1.0);
    const double south = at(0.0, -1.0);
    const double twist =
        (at(1.0, 1.0) - at(-1.0, 1.0) - at(1.0, -1.0) + at(-1.0, -1.0)) / (4.0 * stepMeters * stepMeters);
    const double gradientEast = (east - west) / (2.0 * stepMeters);
    const double gradientNorth = (north - south) / (2.0 * stepMeters);
    const double curvatureEast = (east - 2.0 * centre + west) / (stepMeters * stepMeters);
    const double curvatureNorth = (north - 2.0 * centre + south) / (stepMeters * stepMeters);
    const double turnEast = -curvatureEast * gradientNorth + twist * gradientEast;
    const double turnNorth = -twist * gradientNorth + curvatureNorth * gradientEast;
    const double slopeSquared =
        std::max(gradientEast * gradientEast + gradientNorth * gradientNorth, 1.0e-18);
    return SlopeReading{static_cast<float>(gradientEast * layout.radiusMeters),
                        static_cast<float>(gradientNorth * layout.radiusMeters),
                        static_cast<float>(std::hypot(turnEast, turnNorth) / slopeSquared)};
}

/* ------------------------------------------------------- erosion filter -- */

/**
 * Gullies cut into the massif, after Rune Skovbo Johansen's erosion filter.
 *
 * Each octave lays stripes along the slope as it stands once the octaves
 * above it are cut, so the gullies of one octave run down the walls of the
 * last and leave them at an angle, which is what makes a drainage network
 * branch. Every octave fades toward a target height rather than adding to
 * the last: where the slope is gentle, and on the crests and floors the
 * octave above left behind, the finer octaves give way to that target, so a
 * spur keeps a clean crest, a gully keeps a clean V floor, and only the walls
 * between carry the finer gullies. The target starts at the massif's own
 * altitude, -1 at the foot and 1 on the summit, which is what keeps the
 * summit a peak and the foot a plain.
 *
 * All of it is read in shares of the height and the radius, so a mountain
 * twice as tall is cut exactly twice as deep.
 */
struct ErosionFilter {
    /// The spacing of the coarsest gullies on the ground.
    double coarsestSpacingMeters = 1400.0;
    /// The narrowest spacing an octave may have before it fades out.
    double finestSpacingMeters = 10.0;
    double radiusMeters = 1900.0;
    int octaveCount = 1;
    float detailRetention = 1.0f;
    /// How steep the coarsest walls are taken to be when they steer the
    /// octave below, in the massif's slope units.
    float wallSlope = 1.0f;
    /// How steep the massif is taken to be when steering the stripes. Against
    /// a steeper massif the walls of the octave above turn the next octave
    /// less, so the gullies run straighter down the flank.
    float steeringSlope = 2.0f;
    std::uint64_t seed = 0;
};

constexpr int mostGullyOctaves = 10;

/// The massif as the filter reads it: its height, the slope the gullies come
/// in on, and its slope read across each octave's own width.
struct MassifReading {
    float height = 0.0f;
    float slope = 0.0f;
    std::array<SlopeReading, mostGullyOctaves> octaves{};
};

/// How far a gully's own walls turn the octave below it, as a share of the
/// wall slope. Against the steering slope this sets the angle a tributary
/// leaves its parent at; too little and the tributaries run alongside their
/// parents as parallel streaks instead of branching.
constexpr float gullySteeringShare = 1.0f;

/// The massif slope at which the gullies come in fully. Gentler ground keeps
/// to its fade target, so the plain and the apron stay uncut.
constexpr float gullyOnsetSlope = 0.35f;

/// How far up a gully wall the next octave comes in fully, in the wall's own
/// climb from 0 on crest and floor to 1 halfway up.
constexpr float wallOnsetClimb = 0.6f;

/// How much of an octave's amplitude the next finer one keeps. At one half a
/// finer gully, half the width, would stand as steep as its parent, and the
/// walls of every octave would stack into a surface steeper than rock holds;
/// a little under keeps each generation gentler than the last.
constexpr float gullyOctaveGain = 0.4f;

/// How strongly the octaves above hold back the ones below. Above one, a
/// finer octave needs the walls of every coarser one to make room for it.
constexpr float detailHoldBack = 1.5f;

float easeOut(float share) {
    const float clamped = std::clamp(share, 0.0f, 1.0f);
    return 1.0f - (1.0f - clamped) * (1.0f - clamped);
}

/// The running state of the filter from one octave to the next.
struct GullyCut {
    float fadeTarget = 0.0f;
    float mask = 0.0f;
    /// The turn the walls cut so far give the octaves below.
    float wallEast = 0.0f;
    float wallNorth = 0.0f;
    float cut = 0.0f;
    float magnitude = 0.0f;
    /// What the magnitude would be with every octave fully present, so a
    /// place only the finer octaves reach is cut only as deep as they are.
    float fullMagnitude = 0.0f;
    /// The amplitude and wall slope of the octave about to be cut.
    float amplitude = 0.0f;
    float wallSlope = 0.0f;
};

GullyCut startingCut(const ErosionFilter& filter, const MassifReading& massif) {
    GullyCut start;
    start.fadeTarget = std::clamp(massif.height * 2.0f - 1.0f, -1.0f, 1.0f);
    start.mask = easeOut(massif.slope / gullyOnsetSlope);
    start.amplitude = 1.0f;
    start.wallSlope = filter.wallSlope;
    return start;
}

/// How far from a cell's centre its stripes are read, in stripe spacings. A
/// fall that turns by an angle across a spacing shifts the stripes' phase by
/// up to this many turns of that angle.
constexpr float stripeReachSpacings = 1.25f;

/// Where the fall turns fast against an octave's width, its stripes can no
/// longer follow it: across a crest of the massif or close about the summit
/// they would scramble into speckle. Between these two shares of a stripe's
/// own phase advance per spacing, the octave fades out.
constexpr float turnFadeStart = 0.6f;
constexpr float turnFadeEnd = 1.5f;

/// How much of an octave is drawn: none once its spacing is under the finest
/// the massif carries, all of it at twice that, only as many octaves as the
/// retained detail allows, and none where the fall turns faster than its
/// stripes can follow.
float octavePresence(const ErosionFilter& filter, int octave, double spacingMeters,
                     const SlopeReading& slope) {
    const float keptOctaves =
        std::max(filter.detailRetention * static_cast<float>(filter.octaveCount), 1.0f);
    const float kept = std::clamp(keptOctaves - static_cast<float>(octave), 0.0f, 1.0f);
    const float widened = static_cast<float>(
        std::clamp(spacingMeters / filter.finestSpacingMeters - 1.0, 0.0, 1.0));
    const float turning =
        slope.turnPerMeter * static_cast<float>(spacingMeters) * stripeReachSpacings;
    const float followed =
        1.0f - std::clamp((turning - turnFadeStart) / (turnFadeEnd - turnFadeStart), 0.0f, 1.0f);
    return kept * smoothStep(widened) * smoothStep(followed);
}

/// The direction an octave's stripes run: down the massif as read across its
/// own width, at the steering slope, turned by the walls cut above it.
Heading octaveSteering(const ErosionFilter& filter, const GullyCut& cut,
                       const SlopeReading& slope) {
    const float slopeLength = std::max(std::hypot(slope.slopeEast, slope.slopeNorth), 1.0e-6f);
    const float east = slope.slopeEast / slopeLength * filter.steeringSlope + cut.wallEast;
    const float north = slope.slopeNorth / slopeLength * filter.steeringSlope + cut.wallNorth;
    const float length = std::max(std::hypot(east, north), 1.0e-6f);
    return Heading{east / length, north / length};
}

/// An octave that is absent where a point stands keeps to its fade target
/// there, which is where a present octave faded all the way out would leave
/// it, so the depth does not jump where an octave fades in.
void keepToTarget(GullyCut& cut) {
    cut.cut += cut.fadeTarget * cut.amplitude;
    cut.magnitude += cut.amplitude;
}

void cutOctave(GullyCut& cut, const ErosionFilter& filter, const GroundPoint& ground, int octave,
               double spacingMeters, const SlopeReading& slope, float presence) {
    const Heading steering = octaveSteering(filter, cut, slope);
    const noise::StripeSample stripes = noise::stripes(
        static_cast<float>(ground.eastingMeters / spacingMeters),
        static_cast<float>(ground.northingMeters / spacingMeters),
        static_cast<float>(steering.east), static_cast<float>(steering.north), 1.0f,
        terrainSeedForLane(filter.seed, static_cast<std::uint64_t>(octave)));
    const float faded = cut.fadeTarget + (stripes.wave - cut.fadeTarget) * cut.mask * presence;
    cut.cut += faded * cut.amplitude;
    cut.magnitude += cut.amplitude;
    cut.fadeTarget = faded;
    // The next octave is steered by these walls as though each were straight,
    // so the tributaries run straight off them rather than curling. An octave
    // only partly present steers and holds back the ones below it only partly,
    // or the place it fades in would draw a seam through every finer octave.
    const float wallSide =
        std::copysign(gullySteeringShare * cut.wallSlope * presence, stripes.climb);
    const float acrossLength = std::max(
        std::hypot(stripes.phaseGradientX, stripes.phaseGradientY), 1.0e-6f);
    cut.wallEast -= wallSide * stripes.phaseGradientX / acrossLength;
    cut.wallNorth -= wallSide * stripes.phaseGradientY / acrossLength;
    const float wallMask = easeOut(std::abs(stripes.climb) / wallOnsetClimb);
    const float heldMask = (1.0f - std::pow(1.0f - cut.mask, detailHoldBack)) * wallMask;
    cut.mask += (heldMask - cut.mask) * presence;
}

/// How deep the gullies cut at a point, 0 where every octave stands at the
/// top of its wave, as the summit does, and 1 on the floor of the deepest.
float gullyCutAt(const ErosionFilter& filter, const GroundPoint& ground,
                 const MassifReading& massif) {
    GullyCut cut = startingCut(filter, massif);
    double spacingMeters = filter.coarsestSpacingMeters;
    for (int octave = 0; octave < filter.octaveCount; ++octave) {
        const SlopeReading& slope = massif.octaves[octave];
        const float presence = octavePresence(filter, octave, spacingMeters, slope);
        if (presence > 0.0f) {
            cutOctave(cut, filter, ground, octave, spacingMeters, slope, presence);
        } else {
            keepToTarget(cut);
        }
        spacingMeters *= 0.5;
        cut.fullMagnitude += cut.amplitude;
        cut.amplitude *= gullyOctaveGain;
        cut.wallSlope *= gullyOctaveGain * 2.0f;
    }
    return (cut.magnitude - cut.cut) / (2.0f * cut.fullMagnitude);
}

/* ------------------------------------------------------------- mountain -- */

/// How steep the coarsest gully walls are taken to be when they steer the
/// octave below, in the massif's slope units, where a flank of the mountain's
/// mean steepness is one.
constexpr float steeringWallSlope = 1.0f;

/// How steep the massif is taken to be when steering the gullies, at no spur
/// length and at full spur length.
constexpr float shortSpurSteeringSlope = 0.4f;
constexpr float longSpurSteeringSlope = 1.6f;

/// The narrowest gully a massif carries, as a share of its reach. Anything
/// finer has no room to hold a floor and two walls, so it reads as a crinkle
/// laid over the rock rather than as drainage cut into it.
constexpr double finestGullyShare = 1.0 / 400.0;

/// The narrowest gully the grid can hold, in cells. A gully narrower than this
/// is sampled below its own width and comes out as speckle.
constexpr double finestGullyCells = 4.0;


/// How much of the massif the floor of the deepest gully loses at full
/// detail. A share of the mass standing there rather than a depth, so the
/// valleys shallow out into the apron with the mass and never reach the plain
/// inside the footprint.
constexpr float gullyDepthLimit = 0.45f;

struct Mountain {
    MountainLayout layout;
    ErosionFilter erosion;
    float gullyDepth = 0.0f;
};

/// How far across, as a share of an octave's spacing, the massif is read for
/// that octave, and the least distance it is ever read across, as a share of
/// the radius. Read across a quarter of the stripes' width, the slope an
/// octave steers by carries no turn finer than the octave can follow.
constexpr double octaveStencilShare = 0.25;
constexpr double finestStencilShare = 0.01;

MassifReading readMassif(const Mountain& mountain, double eastingMeters, double northingMeters) {
    const MountainLayout& layout = mountain.layout;
    MassifReading reading;
    reading.height = massifAt(layout, eastingMeters, northingMeters);
    const double finestStepMeters = layout.radiusMeters * finestStencilShare;
    const SlopeReading finest =
        massifSlopeAt(layout, eastingMeters, northingMeters, reading.height, finestStepMeters);
    reading.slope = std::hypot(finest.slopeEast, finest.slopeNorth);
    double spacingMeters = mountain.erosion.coarsestSpacingMeters;
    for (int octave = 0; octave < mountain.erosion.octaveCount; ++octave) {
        const double stepMeters = spacingMeters * octaveStencilShare;
        reading.octaves[octave] = finest;
        if (stepMeters > finestStepMeters) {
            reading.octaves[octave] =
                massifSlopeAt(layout, eastingMeters, northingMeters, reading.height, stepMeters);
        }
        spacingMeters *= 0.5;
    }
    return reading;
}

float mountainHeightAt(const Mountain& mountain, double eastingMeters, double northingMeters) {
    const MassifReading massif = readMassif(mountain, eastingMeters, northingMeters);
    const float cut =
        gullyCutAt(mountain.erosion, GroundPoint{eastingMeters, northingMeters}, massif);
    const float height = massif.height * (1.0f - mountain.gullyDepth * cut);
    return height * mountain.layout.heightMeters + mountain.layout.baseAltitudeMeters;
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
                                  const WorldFrame& frame, std::uint64_t seed) {
    const double scale = api->parameterNumber(context, 0);
    const int bulk = static_cast<int>(api->parameterNumber(context, 3));
    const float irregularity = static_cast<float>(api->parameterNumber(context, 11));

    MountainLayout layout;
    layout.placement = placementFrom(frame, api->parameterNumber(context, 6),
                                     api->parameterNumber(context, 7), 0.0);
    layout.radiusMeters = api->parameterNumber(context, 8) * scale;
    layout.summitCrest =
        summitCrestFor(layout.placement, layout.radiusMeters, terrainSeedForLane(seed, 5));
    layout.ridges =
        ridgeSkeletonFor(layout.placement, layout.radiusMeters, terrainSeedForLane(seed, 6));
    layout.profileExponent =
        static_cast<float>(api->parameterNumber(context, 10)) * bulkExponentScale(bulk);
    layout.outlineFeatureSizeMeters = layout.radiusMeters * std::numbers::pi / outlineLobeCount;
    layout.outlineWanderShare = irregularity * outlineWanderLimit;
    layout.heightMeters = static_cast<float>(api->parameterNumber(context, 1));
    layout.baseAltitudeMeters = static_cast<float>(api->parameterNumber(context, 9));
    layout.warp = mountainWarpFor(layout.radiusMeters, irregularity, seed);
    layout.outlineSeed = terrainSeedForLane(seed, 4);
    layout.crestSeed = terrainSeedForLane(seed, 7);
    return layout;
}

ErosionFilter mountainErosionFrom(const TerrainContextApi* api, TerrainEvaluationContext* context,
                                  const WorldFrame& frame, const MountainLayout& layout,
                                  std::uint64_t seed) {
    const double scale = api->parameterNumber(context, 0);
    const float spurLength = static_cast<float>(api->parameterNumber(context, 14));
    ErosionFilter erosion;
    erosion.coarsestSpacingMeters = api->parameterNumber(context, 13) * scale;
    erosion.finestSpacingMeters = std::max(layout.radiusMeters * finestGullyShare,
                                           frame.cellSizeMeters * finestGullyCells);
    erosion.radiusMeters = layout.radiusMeters;
    erosion.octaveCount = mostGullyOctaves;
    erosion.detailRetention = 1.0f - static_cast<float>(api->parameterNumber(context, 4));
    erosion.wallSlope = steeringWallSlope;
    erosion.steeringSlope =
        shortSpurSteeringSlope + (longSpurSteeringSlope - shortSpurSteeringSlope) * spurLength;
    erosion.seed = terrainSeedForLane(seed, 3);
    return erosion;
}

void evaluateMountain(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const WorldFrame frame = worldFrameFor(api, context);
    const std::uint64_t seed = terrainSeedForLane(
        api->randomSeed(context), static_cast<std::uint64_t>(api->parameterNumber(context, 5)));
    Mountain mountain;
    mountain.layout = mountainLayoutFrom(api, context, frame, seed);
    mountain.erosion = mountainErosionFrom(api, context, frame, mountain.layout, seed);
    mountain.gullyDepth = static_cast<float>(api->parameterNumber(context, 12)) * gullyDepthLimit;
    float* output = api->outputField(context, 0);

    forEachRowRange(api, context, frame.size, [&](int firstRow, int lastRow) {
        writeLandformRows(frame, firstRow, lastRow, output,
                          [&](double eastingMeters, double northingMeters) {
                              return mountainHeightAt(mountain, eastingMeters, northingMeters);
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
                      fromCrestMeters / frame.halfWidthMeters, frame.walkLengthMeters(),
                      fromCrestMeters};
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
