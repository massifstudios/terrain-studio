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

constexpr int ridgeOctaveCount = 4;

/// Each octave doubles in frequency and halves in depth, so every network
/// carves in proportion to how far apart its own ridges are. Any shallower
/// fall-off buries the main valleys under gullies as deep as they are; any
/// steeper one fades the fine ground out before it reaches a readable size.
constexpr float ridgeLacunarity = 2.0f;
constexpr float ridgeGain = 1.0f / ridgeLacunarity;

/// How much of an octave's stretch the next finer one keeps. A range lets go
/// of it quickly, because the crumple on a distant flank of a chain is not
/// read as a gully.
constexpr float rangeElongationRelease = 0.4f;

/**
 * How much of one octave survives when the finest detail is held back.
 *
 * Retention counts octaves rather than scaling amplitude, so holding detail
 * back drops the smallest ridges outright and leaves the larger ones at full
 * depth: the surviving octaves keep the share of the network they always had,
 * so what is left is a coarser network rather than a fainter one. The coarsest
 * octave is always kept, so the network never vanishes.
 */
float octaveWeight(float detailRetention, int octave) {
    const float keptOctaves = std::max(detailRetention * ridgeOctaveCount, 1.0f);
    return std::clamp(keptOctaves - static_cast<float>(octave), 0.0f, 1.0f);
}

/**
 * The ridge network the catchments are cut from, with the stretch that draws
 * the ridges into spurs released as the octaves get finer.
 *
 * Each octave asks for its own reading position at its own share of the
 * stretch, so a caller can keep the coarse network combed down the slope while
 * letting the finest crumple settle back towards isotropy.
 */
template <typename ReadPointFor>
float craggyField(const ReadPointFor& readPointFor, float featureSizeMeters, float elongation,
                  float detailRetention, float elongationRelease, std::uint64_t seed) {
    float total = 0.0f;
    float amplitude = 1.0f;
    float amplitudeSum = 0.0f;
    float octaveScale = 1.0f / featureSizeMeters;
    float octaveElongation = elongation;

    for (int octave = 0; octave < ridgeOctaveCount; ++octave) {
        const float weight = amplitude * octaveWeight(detailRetention, octave);
        const GroundPoint read = readPointFor(octaveElongation);
        total += noise::ridgeOctave(static_cast<float>(read.eastingMeters) * octaveScale,
                                    static_cast<float>(read.northingMeters) * octaveScale,
                                    terrainSeedForLane(seed, static_cast<std::uint64_t>(octave)))
                 * weight;
        amplitudeSum += amplitude;
        amplitude *= ridgeGain;
        octaveScale *= ridgeLacunarity;
        octaveElongation *= elongationRelease;
    }
    return total / amplitudeSum;
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

/// How far along the segment between two points a third, already on the
/// segment, sits.
double segmentShare(const GroundPoint& first, const GroundPoint& second, const GroundPoint& on) {
    const double runEast = second.eastingMeters - first.eastingMeters;
    const double runNorth = second.northingMeters - first.northingMeters;
    const double lengthSquared = runEast * runEast + runNorth * runNorth;
    if (lengthSquared <= 1.0e-9) {
        return 0.0;
    }
    return std::clamp(((on.eastingMeters - first.eastingMeters) * runEast
                       + (on.northingMeters - first.northingMeters) * runNorth)
                          / lengthSquared,
                      0.0, 1.0);
}

/// The point on the segment between two others that lies closest to a point.
GroundPoint nearestOnSegment(const GroundPoint& first, const GroundPoint& second,
                             const GroundPoint& ground) {
    const double runEast = second.eastingMeters - first.eastingMeters;
    const double runNorth = second.northingMeters - first.northingMeters;
    const double lengthSquared = runEast * runEast + runNorth * runNorth;
    if (lengthSquared <= 1.0e-9) {
        return first;
    }
    const double toEast = ground.eastingMeters - first.eastingMeters;
    const double toNorth = ground.northingMeters - first.northingMeters;
    const double along = std::clamp((toEast * runEast + toNorth * runNorth) / lengthSquared, 0.0, 1.0);
    return GroundPoint{first.eastingMeters + runEast * along, first.northingMeters + runNorth * along};
}

/// A value drawn evenly from either side of zero, out to `limit`.
double spreadFrom(std::uint64_t* state, double limit) {
    return (terrainRandomUnitFloat(state) - 0.5f) * 2.0 * limit;
}

/// Either side of a line, drawn evenly.
double drawnSide(std::uint64_t* state) {
    if (terrainRandomUnitFloat(state) < 0.5f) {
        return -1.0;
    }
    return 1.0;
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
     "", TerrainParameterType_Float, 2.0, 0.2, 6.0, "", nullptr, 0},
    {"irregularity", "Irregularity", "How far the outline wanders off a circle", "",
     TerrainParameterType_Float, 0.35, 0.0, 1.0, "", nullptr, 0},
    {"detailStrength", "Detail", "How much fine ridged detail is cut into the slopes", "",
     TerrainParameterType_Float, 0.5, 0.0, 1.0, "", nullptr, 0},
    {"featureSize", "Spur spacing",
     "Ground distance between neighbouring spurs where they leave the summit, before Scale", "m",
     TerrainParameterType_Float, 300.0, 1.0, 100000.0, "", nullptr, 0},
    {"spurElongation", "Gully stretch",
     "How far the fine relief is drawn out into gullies running down the flanks", "",
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

/* ------------------------------------------------------------ ridge tree -- */

/**
 * The divides of a mountain form a tree rooted at the summit: spurs leave the
 * summit crest, fork on their way down, and the ground between any two of
 * them is a valley. Noise cannot draw this. The creases of folded gradient
 * noise close into loops, and a loop of ridge encloses a basin with no way
 * out, which no mountain has. So the coarse structure is drawn as lines and
 * the valleys are read off the distance to them.
 */
struct RidgeLine {
    std::vector<GroundPoint> points;
    /// How high the ridge stands against the mass profile at its start and
    /// at its end. Below one the ridge sits down into the flank; above one it
    /// holds the flank up. Ranking the generations this way is what lets the
    /// eye tell a main ridge from a spur and a spur from a rib.
    float standAtStart = 1.0f;
    float standAtEnd = 1.0f;
};

/// How high each generation of spur stands where it leaves its parent and
/// where it ends. Each finer generation sits lower, so the network has a
/// hierarchy rather than a thicket of equal ridges.
constexpr float spurStandAtStart[] = {1.0f, 0.9f, 0.76f};
constexpr float spurStandAtEnd[] = {0.9f, 0.84f, 0.7f};

/// One primary spur is the main ridge: it leaves the first end of the summit
/// crest and holds its height further out than any other, so the massif has
/// a spine rather than a set of equal arms.
constexpr float mainRidgeStandAtEnd = 1.15f;

/// How far past the nominal foot a spur runs, so no spur ends on the flank
/// and leaves a rounded nose behind it.
constexpr double spurEndShare = 1.25;

/// How far off the summit crest the spurs start. Just off it, so the ones
/// leaving the ends of the crest fan out from a point rather than all sharing
/// one.
constexpr double spurStartShare = 0.03;

/// How close together the spurs leave the crest, against the spur spacing.
constexpr double spurOriginSpacingShare = 1.0;

constexpr int spurRunCount = 6;

/// How far a spur may lean off straight out as it leaves, how far it may turn
/// at each step, and how far a branch leaves the spur that bore it.
constexpr double spurLeanRadians = 18.0 * std::numbers::pi / 180.0;
constexpr double spurBendRadians = 15.0 * std::numbers::pi / 180.0;
constexpr double spurBranchRadians = 38.0 * std::numbers::pi / 180.0;

/// How much of each step a spur takes the way it set out rather than the way
/// it was last heading. Spurs that only wander cross each other; this keeps
/// them fanning, and keeps a branch diverging from the spur that bore it.
constexpr double spurOutwardPull = 0.25;

/// How much a spur leaving the crest sets out straight away from the centre
/// rather than straight off the crest. Off a straight crest every spur would
/// otherwise leave parallel to its neighbours; leaning them outward fans
/// them across the flank.
constexpr double spurFanShare = 0.6;

/// How many times a spur forks on its way down, and the earliest and latest
/// step it may fork at.
constexpr int spurBranchGenerations = 2;
constexpr int spurBranchEarliestStep = 1;
constexpr int spurBranchLatestStep = 3;

/// How far a branch of each generation runs, as a share of the ground left
/// between where it leaves and the foot. A spur that forks and has both arms
/// run to the foot crosses its neighbour; a branch dies into the valley it
/// divides, and only some of them make it down.
constexpr double branchReachLow[] = {1.0, 0.5, 0.3};
constexpr double branchReachHigh[] = {1.0, 0.95, 0.55};

struct RidgeTreePlan {
    GroundPoint centre;
    RidgeSegment summitCrest;
    double radiusMeters = 3000.0;
    double spacingMeters = 300.0;
};

Heading blendedHeading(const Heading& heading, const Heading& toward, double share) {
    const double east = heading.east * (1.0 - share) + toward.east * share;
    const double north = heading.north * (1.0 - share) + toward.north * share;
    const double length = std::max(std::hypot(east, north), 1.0e-9);
    return Heading{east / length, north / length};
}

void appendSpur(const GroundPoint& origin, const Heading& outward, double reachShare,
                const RidgeTreePlan& plan, int generation, std::uint64_t seed,
                std::vector<RidgeLine>& lines);

/// One fork off a spur, leaving at the step drawn for it.
void appendSpurBranch(const RidgeLine& spur, const RidgeTreePlan& plan, int generation,
                      std::uint64_t* state, std::uint64_t seed, std::vector<RidgeLine>& lines) {
    if (generation >= spurBranchGenerations) {
        return;
    }
    const int stepSpan = spurBranchLatestStep - spurBranchEarliestStep + 1;
    const int step =
        spurBranchEarliestStep
        + std::min(static_cast<int>(terrainRandomUnitFloat(state) * stepSpan), stepSpan - 1);
    const std::size_t at = static_cast<std::size_t>(step);
    const Heading along = headingFrom(spur.points[at - 1], spur.points[at]);
    const int child = generation + 1;
    const double reachShare =
        branchReachLow[child]
        + (branchReachHigh[child] - branchReachLow[child]) * terrainRandomUnitFloat(state);
    appendSpur(spur.points[at], along.turned(drawnSide(state) * spurBranchRadians), reachShare,
               plan, child, seed, lines);
}

void appendSpur(const GroundPoint& origin, const Heading& outward, double reachShare,
                const RidgeTreePlan& plan, int generation, std::uint64_t seed,
                std::vector<RidgeLine>& lines) {
    std::uint64_t state = seed;
    const double fromCentre = std::hypot(origin.eastingMeters - plan.centre.eastingMeters,
                                         origin.northingMeters - plan.centre.northingMeters);
    const double runMeters =
        (plan.radiusMeters * spurEndShare - fromCentre) * reachShare / spurRunCount;
    if (runMeters <= 0.0) {
        return;
    }
    RidgeLine spur;
    spur.standAtStart = spurStandAtStart[generation];
    spur.standAtEnd = spurStandAtEnd[generation];
    spur.points.reserve(spurRunCount + 1);
    spur.points.push_back(origin);
    const Heading setOut = outward.turned(spreadFrom(&state, spurLeanRadians));
    Heading heading = setOut;
    for (int run = 0; run < spurRunCount; ++run) {
        heading = blendedHeading(heading.turned(spreadFrom(&state, spurBendRadians)), setOut,
                                 spurOutwardPull);
        spur.points.push_back(stepFrom(spur.points.back(), heading, runMeters));
    }
    lines.push_back(spur);
    appendSpurBranch(lines.back(), plan, generation, &state, terrainSeedForLane(seed, 1), lines);
}

/// A point on the loop drawn a fixed distance around the summit crest, and
/// the direction straight out from the crest there.
struct CrestLoopPoint {
    GroundPoint point;
    Heading outward;
};

/// The loop, walked from the first end along one side, round the second end,
/// back along the other side and round the first end.
CrestLoopPoint crestLoopAt(const RidgeSegment& crest, double offsetMeters, double arcMeters) {
    const Heading along = headingFrom(crest.first, crest.second);
    const Heading left = along.turned(std::numbers::pi * 0.5);
    const double lengthMeters =
        std::hypot(crest.second.eastingMeters - crest.first.eastingMeters,
                   crest.second.northingMeters - crest.first.northingMeters);
    const double capMeters = std::numbers::pi * offsetMeters;

    if (arcMeters < lengthMeters) {
        return CrestLoopPoint{stepFrom(stepFrom(crest.first, along, arcMeters), left, offsetMeters),
                              left};
    }
    if (arcMeters < lengthMeters + capMeters) {
        const Heading outward = left.turned(-(arcMeters - lengthMeters) / offsetMeters);
        return CrestLoopPoint{stepFrom(crest.second, outward, offsetMeters), outward};
    }
    if (arcMeters < 2.0 * lengthMeters + capMeters) {
        const double back = arcMeters - lengthMeters - capMeters;
        const Heading right{-left.east, -left.north};
        return CrestLoopPoint{stepFrom(stepFrom(crest.second, along, -back), right, offsetMeters),
                              right};
    }
    const double round = arcMeters - 2.0 * lengthMeters - capMeters;
    const Heading outward = Heading{-left.east, -left.north}.turned(-round / offsetMeters);
    return CrestLoopPoint{stepFrom(crest.first, outward, offsetMeters), outward};
}

std::vector<RidgeLine> ridgeTreeFrom(const RidgeTreePlan& plan, std::uint64_t seed) {
    std::vector<RidgeLine> lines;
    lines.push_back(RidgeLine{{plan.summitCrest.first, plan.summitCrest.second}});

    const double offsetMeters = plan.radiusMeters * spurStartShare;
    const double lengthMeters =
        std::hypot(plan.summitCrest.second.eastingMeters - plan.summitCrest.first.eastingMeters,
                   plan.summitCrest.second.northingMeters - plan.summitCrest.first.northingMeters);
    const double loopMeters = 2.0 * lengthMeters + 2.0 * std::numbers::pi * offsetMeters;
    const double originSpacing = plan.spacingMeters * spurOriginSpacingShare;
    const int spurCount = std::max(static_cast<int>(std::round(loopMeters / originSpacing)), 3);
    const double stepMeters = loopMeters / spurCount;

    std::uint64_t state = terrainSeedForLane(seed, 0);
    const double phaseMeters = terrainRandomUnitFloat(&state) * stepMeters;
    for (int index = 0; index < spurCount; ++index) {
        const CrestLoopPoint origin =
            crestLoopAt(plan.summitCrest, offsetMeters, phaseMeters + stepMeters * index);
        const Heading fanned = blendedHeading(origin.outward, headingFrom(plan.centre, origin.point),
                                             spurFanShare);
        const std::size_t before = lines.size();
        appendSpur(origin.point, fanned, branchReachLow[0], plan, 0,
                   terrainSeedForLane(seed, static_cast<std::uint64_t>(index) + 1), lines);
        if (index == 0 && lines.size() > before) {
            lines[before].standAtEnd = mainRidgeStandAtEnd;
        }
    }
    return lines;
}

/* ------------------------------------------------------ the valley field -- */

/// Where the nearest ridge line stands from a point, and the next nearest
/// after it. Two are needed: a valley is the ground between two ridges, and
/// its floor lies where they are equally far.
struct NearestRidges {
    double firstMeters = 1.0e30;
    double secondMeters = 1.0e30;
    /// How high the nearest ridge stands where it comes closest.
    float stand = 1.0f;
    /// The stand of every ridge weighted by nearness, which is what the
    /// valley floor between two of them inherits. Taking the floor from the
    /// nearest ridge alone steps it wherever the nearest ridge changes.
    float floorStand = 1.0f;
    /// How far along the nearest ridge its closest point sits.
    double alongFirstMeters = 0.0;
};

/// Where a ridge line comes closest to a point: the point, how far, and how
/// high the ridge stands there.
struct RidgeApproach {
    GroundPoint point;
    double distanceMeters = 1.0e30;
    /// How far along the ridge from its start the nearest point sits.
    double alongMeters = 0.0;
    float stand = 1.0f;
};

RidgeApproach ridgeApproach(const RidgeLine& line, const GroundPoint& ground) {
    RidgeApproach best;
    double walkedMeters = 0.0;
    for (std::size_t index = 1; index < line.points.size(); ++index) {
        const GroundPoint& from = line.points[index - 1];
        const GroundPoint& to = line.points[index];
        const double segmentMeters = std::hypot(to.eastingMeters - from.eastingMeters,
                                                to.northingMeters - from.northingMeters);
        const double distance =
            segmentDistance(ground.eastingMeters, ground.northingMeters, from.eastingMeters,
                            from.northingMeters, to.eastingMeters, to.northingMeters);
        if (distance < best.distanceMeters) {
            best.distanceMeters = distance;
            best.point = nearestOnSegment(from, to, ground);
            best.alongMeters = walkedMeters + segmentShare(from, to, best.point) * segmentMeters;
        }
        walkedMeters += segmentMeters;
    }
    const double alongLine = best.alongMeters / std::max(walkedMeters, 1.0e-9);
    best.stand =
        line.standAtStart + (line.standAtEnd - line.standAtStart) * static_cast<float>(alongLine);
    return best;
}

/// The power the second distance is taken as a soft minimum at. A hard second
/// minimum creases the flank wherever the second nearest ridge changes; the
/// soft one blends the ridges beyond the nearest into one far side of the
/// valley.
constexpr double secondRidgePower = 4.0;

NearestRidges nearestRidges(const std::vector<RidgeLine>& lines, const GroundPoint& ground) {
    std::vector<RidgeApproach> approaches;
    approaches.reserve(lines.size());
    std::size_t nearestIndex = 0;
    for (const RidgeLine& line : lines) {
        approaches.push_back(ridgeApproach(line, ground));
        if (approaches.back().distanceMeters < approaches[nearestIndex].distanceMeters) {
            nearestIndex = approaches.size() - 1;
        }
    }
    double inverseSum = 0.0;
    double weightSum = 0.0;
    double standSum = 0.0;
    for (std::size_t index = 0; index < approaches.size(); ++index) {
        const double weight =
            std::pow(std::max(approaches[index].distanceMeters, 1.0e-3), -secondRidgePower);
        weightSum += weight;
        standSum += weight * approaches[index].stand;
        if (index != nearestIndex) {
            inverseSum += weight;
        }
    }
    NearestRidges nearest;
    nearest.firstMeters = approaches[nearestIndex].distanceMeters;
    nearest.secondMeters = std::pow(inverseSum, -1.0 / secondRidgePower);
    nearest.stand = approaches[nearestIndex].stand;
    nearest.floorStand = static_cast<float>(standSum / weightSum);
    nearest.alongFirstMeters = approaches[nearestIndex].alongMeters;
    return nearest;
}

/// How wide a valley has to be before it is cut to full depth, against the
/// spur spacing. Where a branch has just left its parent the two ridges are
/// a few metres apart and the ground between them is a valley head, not a
/// gorge.
constexpr double valleyFullWidthShare = 1.0;

/// How broad the rounded top of a ridge is, as a share of the spur spacing,
/// at the summit and at the foot. Below the rounding the flank runs straight
/// into the V of the floor. High on the mountain the spurs are arêtes; low
/// down they are broad-backed.
constexpr double summitCrestWidthShare = 0.06;
constexpr double footCrestWidthShare = 0.4;

/// The drop from a ridge crest at a distance from it: level on the crest,
/// rounded over the crest width, straight beyond it.
double crestDrop(double fromCrestMeters, double crestWidthMeters) {
    return std::hypot(fromCrestMeters, crestWidthMeters) - crestWidthMeters;
}

/// How much of the mass the floor of a valley between two spurs loses. The
/// cut is a share of the mass standing there, so the valleys of a tall
/// mountain are as deep as the mountain is tall, and the relief dies away
/// with the mass into the apron at the foot.
constexpr float valleyDepthShare = 0.45f;

/// What the ridge tree leaves of the mass at a point: the nearest ridge's own
/// stand on its crest, the valley floor between it and the next ridge over,
/// and the wall between.
float treeRelief(const NearestRidges& nearest, double spacingMeters, float mass) {
    const double span = nearest.firstMeters + nearest.secondMeters;
    if (span <= 1.0e-9) {
        return nearest.stand;
    }
    // The soft second distance can dip under the first on the floor itself;
    // that ground is the floor, not a trench in it.
    const double floorMeters = std::max(span * 0.5, nearest.firstMeters);
    const double crestWidthMeters =
        spacingMeters
        * (footCrestWidthShare
           + (summitCrestWidthShare - footCrestWidthShare) * std::clamp(mass, 0.0f, 1.0f));
    const float wall = 1.0f
                       - static_cast<float>(crestDrop(nearest.firstMeters, crestWidthMeters)
                                            / crestDrop(floorMeters, crestWidthMeters));
    const float width = static_cast<float>(
        std::clamp(span / (spacingMeters * valleyFullWidthShare), 0.0, 1.0));
    const float floor = nearest.floorStand * (1.0f - valleyDepthShare * width);
    return floor + (nearest.stand - floor) * wall;
}

/* ------------------------------------------------------------ flank cells -- */

/**
 * The fine relief on a flank: a field of cells read in the frame of the ridge
 * above it, where one axis runs along the ridge and the other straight down
 * from it. Each cell is a cone, a peak at its own point falling to a V along
 * every edge, so the cells carve the flank into facets with gullies between
 * them. Read in the ridge's own frame and stretched down the flank, the
 * gullies leave the ridge and run down it, and where the nearest ridge
 * changes, on the valley floor, the gullies from either side meet.
 */
struct FlankCells {
    /// Size of the coarsest cells, along the ridge.
    double cellMeters = 180.0;
    int octaveCount = 5;
    float gain = 0.5f;
    /// How far the cells are drawn out down the flank against their width
    /// along the ridge. 0 leaves them round; near 1 they are streaks.
    float stretch = 0.7f;
    float detailRetention = 1.0f;
    /// Octaves finer than the grid can hold are left out, so the finest ground
    /// is drawn by the grid rather than aliased across it.
    float finestCellMeters = 1.0f;
    std::uint64_t seed = 0;
};

/// How far either side of the summit crest the cells hold their full value,
/// as a share of the coarsest cell, so the fine relief leaves the summit a
/// summit.
constexpr float crestHoldOfCell = 0.5f;

/// 1 on the summit crest, 0 half a cell off it.
float crestHold(double fromCrestMeters, double cellMeters) {
    const float across = static_cast<float>(fromCrestMeters / (cellMeters * crestHoldOfCell));
    return 1.0f - smoothStep(std::clamp(across, 0.0f, 1.0f));
}

/// How much of an octave survives when the finest detail is held back.
/// Retention counts octaves, so holding detail back drops the smallest cells
/// outright and leaves the larger ones at full depth.
float cellOctaveWeight(float detailRetention, int octave, int octaveCount) {
    const float keptOctaves = std::max(detailRetention * static_cast<float>(octaveCount), 1.0f);
    return std::clamp(keptOctaves - static_cast<float>(octave), 0.0f, 1.0f);
}

/// 1 on a cell's own point, 0 on its edges.
float flankCells(double alongRidgeMeters, double fromRidgeMeters, double fromCrestMeters,
                 const FlankCells& cells) {
    const double downMeters = fromRidgeMeters * (1.0 - cells.stretch);
    float total = 0.0f;
    float weightSum = 0.0f;
    float amplitude = 1.0f;
    double cellMeters = cells.cellMeters;
    for (int octave = 0; octave < cells.octaveCount; ++octave) {
        if (cellMeters < cells.finestCellMeters) {
            break;
        }
        const float weight =
            amplitude * cellOctaveWeight(cells.detailRetention, octave, cells.octaveCount);
        const noise::VoronoiSample sample = noise::voronoi(
            static_cast<float>(alongRidgeMeters / cellMeters),
            static_cast<float>(downMeters / cellMeters),
            terrainSeedForLane(cells.seed, static_cast<std::uint64_t>(octave)), 1.0f);
        total += std::min(sample.nearestDistance, 1.0f) * weight;
        // Normalised over every octave the grid can hold, held back or not,
        // so holding detail back leaves smoother ground rather than the same
        // relief drawn coarser.
        weightSum += amplitude;
        amplitude *= cells.gain;
        cellMeters *= 0.5;
    }
    if (weightSum <= 0.0f) {
        return 1.0f;
    }
    const float hold = crestHold(fromCrestMeters, cells.cellMeters);
    return (1.0f - total / weightSum) * (1.0f - hold) + hold;
}

/* ----------------------------------------------------------- the massif -- */

/// How much of what is left the flank cells may cut at full detail.
constexpr float fineCarveLimit = 0.5f;

/// How much of that cut the foot keeps against the summit. The upper faces
/// are striated with gullies; the lower flanks are smoother.
constexpr float gullyFloorShare = 0.3f;

/// How big the coarsest flank cells are against the spur spacing.
constexpr double cellOfSpurSpacing = 1.0;

/// The shortest wavelength worth drawing, in grid cells.
constexpr double finestOctaveCells = 2.0;


/// Everything about a mountain that is fixed before the first cell is read.
struct MountainLayout {
    ShapePlacement placement;
    DomainWarp warp;
    RidgeSegment summitCrest;
    std::vector<RidgeLine> ridgeTree;
    double radiusMeters = 3000.0;
    double spacingMeters = 500.0;
    double outlineFeatureSizeMeters = 3000.0;
    float outlineWanderShare = 0.0f;
    float profileExponent = 1.6f;
    float fineCarve = 0.0f;
    FlankCells cells;
    float heightMeters = 1200.0f;
    float baseAltitudeMeters = 0.0f;
    std::uint64_t outlineSeed = 0;
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

/**
 * The mass, 1 on the summit crest and 0 at the foot: steep where it leaves
 * the crest and tangent to the plane at the foot, so the flank meets the
 * ground through a footslope rather than at an angle.
 */
float massAt(const MountainLayout& layout, const GroundPoint& ground, double fromCrestMeters) {
    const double unit = std::clamp(fromCrestMeters / reachAt(layout, ground), 0.0, 1.0);
    return static_cast<float>(std::pow(1.0 - unit, layout.profileExponent));
}

float mountainHeightAt(const MountainLayout& layout, double eastingMeters, double northingMeters) {
    const GroundPoint ground = warpedBy(layout.warp, eastingMeters, northingMeters);
    const double fromCrestMeters = segmentDistance(
        ground.eastingMeters, ground.northingMeters, layout.summitCrest.first.eastingMeters,
        layout.summitCrest.first.northingMeters, layout.summitCrest.second.eastingMeters,
        layout.summitCrest.second.northingMeters);
    const float mass = massAt(layout, ground, fromCrestMeters);
    const NearestRidges nearest = nearestRidges(layout.ridgeTree, ground);
    const float relief = treeRelief(nearest, layout.spacingMeters, mass);

    const float fine = flankCells(nearest.alongFirstMeters, nearest.firstMeters, fromCrestMeters,
                                  layout.cells);

    const float fineCarve = layout.fineCarve * (gullyFloorShare + (1.0f - gullyFloorShare) * mass);
    const float carved = relief * (1.0f - fineCarve * (1.0f - fine));
    return mass * carved * layout.heightMeters + layout.baseAltitudeMeters;
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
    layout.spacingMeters = api->parameterNumber(context, 13) * scale;
    layout.summitCrest =
        summitCrestFor(layout.placement, layout.radiusMeters, terrainSeedForLane(seed, 5));
    layout.ridgeTree = ridgeTreeFrom(
        RidgeTreePlan{GroundPoint{layout.placement.centreEastingMeters,
                                  layout.placement.centreNorthingMeters},
                      layout.summitCrest, layout.radiusMeters, layout.spacingMeters},
        terrainSeedForLane(seed, 6));
    layout.profileExponent =
        static_cast<float>(api->parameterNumber(context, 10)) * bulkExponentScale(bulk);
    layout.cells.cellMeters = layout.spacingMeters * cellOfSpurSpacing;
    layout.cells.stretch = static_cast<float>(api->parameterNumber(context, 14));
    layout.cells.detailRetention = 1.0f - static_cast<float>(api->parameterNumber(context, 4));
    layout.cells.finestCellMeters = static_cast<float>(frame.cellSizeMeters * finestOctaveCells);
    layout.cells.seed = terrainSeedForLane(seed, 3);
    layout.outlineFeatureSizeMeters = layout.radiusMeters * std::numbers::pi / outlineLobeCount;
    layout.outlineWanderShare = irregularity * outlineWanderLimit;
    layout.heightMeters = static_cast<float>(api->parameterNumber(context, 1));
    layout.baseAltitudeMeters = static_cast<float>(api->parameterNumber(context, 9));
    layout.fineCarve = static_cast<float>(api->parameterNumber(context, 12)) * fineCarveLimit;
    layout.warp = mountainWarpFor(layout.radiusMeters, irregularity, seed);
    layout.outlineSeed = terrainSeedForLane(seed, 4);
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
    "A single mountain: a summit crest with a tree of spurs running off it to the foot",
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
    {"featureSize", "Detail size", "Ground distance between neighbouring gullies on a flank", "m",
     TerrainParameterType_Float, 1900.0, 1.0, 100000.0, "", nullptr, 0},
    {"baseAltitude", "Base altitude", "The altitude the foot of the range sits at", "m",
     TerrainParameterType_Float, 0.0, -12000.0, 12000.0, "", nullptr, 0},
    {"spurElongation", "Gully stretch",
     "How far the fine relief is drawn out into gullies running down the flanks", "",
     TerrainParameterType_Float, 0.7, 0.0, 0.95, "", nullptr, 0}};

struct RangePeak {
    double eastingMeters = 0.0;
    double northingMeters = 0.0;
    /// Where the summit sits along the range's axis, measured from the centre.
    double alongMeters = 0.0;
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

        RangePeak peak;
        peak.alongMeters = alongUnit * lengthMeters + alongJitter;
        peak.eastingMeters = placement.centreEastingMeters + axis.alongEast * peak.alongMeters
                             + axis.acrossEast * acrossJitter;
        peak.northingMeters = placement.centreNorthingMeters + axis.alongNorth * peak.alongMeters
                              + axis.acrossNorth * acrossJitter;
        peak.heightFraction = 1.0f - heightVariation * terrainRandomUnitFloat(&state);
        peaks.push_back(peak);
    }
    return peaks;
}

GroundPoint pointOf(const RangePeak& peak) {
    return GroundPoint{peak.eastingMeters, peak.northingMeters};
}

GroundPoint midpointOf(const RangePeak& first, const RangePeak& second) {
    return GroundPoint{(first.eastingMeters + second.eastingMeters) * 0.5,
                       (first.northingMeters + second.northingMeters) * 0.5};
}

std::vector<RidgeSegment> spineSegmentsFrom(const std::vector<RangePeak>& peaks) {
    std::vector<RidgeSegment> segments;
    if (peaks.size() == 1) {
        segments.push_back(RidgeSegment{pointOf(peaks.front()), pointOf(peaks.front())});
        return segments;
    }
    for (std::size_t index = 1; index < peaks.size(); ++index) {
        segments.push_back(RidgeSegment{pointOf(peaks[index - 1]), pointOf(peaks[index])});
    }
    return segments;
}

/// How far a transverse ridge may lean from square across the spine, either way.
constexpr double branchLeanRadians = 40.0 * std::numbers::pi / 180.0;
/// How sharply a transverse ridge may bend at its knee.
constexpr double branchBendRadians = 30.0 * std::numbers::pi / 180.0;

/**
 * One transverse ridge leaving the spine towards a flank: two straight runs
 * with a bend between them, reaching roughly to the foot of the range.
 */
void appendBranch(const GroundPoint& origin, const Heading& outward, double halfWidthMeters,
                  std::uint64_t seed, std::vector<RidgeSegment>& segments) {
    std::uint64_t state = seed;
    const double lean = (terrainRandomUnitFloat(&state) - 0.5) * 2.0 * branchLeanRadians;
    const double reach = halfWidthMeters * (0.7 + 0.6 * terrainRandomUnitFloat(&state));
    const double bend = (terrainRandomUnitFloat(&state) - 0.5) * 2.0 * branchBendRadians;
    const double kneeShare = 0.4 + 0.3 * terrainRandomUnitFloat(&state);

    const Heading firstHeading = outward.turned(lean);
    const GroundPoint knee = stepFrom(origin, firstHeading, reach * kneeShare);
    const GroundPoint tip = stepFrom(knee, firstHeading.turned(bend), reach * (1.0 - kneeShare));
    segments.push_back(RidgeSegment{origin, knee});
    segments.push_back(RidgeSegment{knee, tip});
}

/**
 * The transverse ridges that make a range a massif rather than a wall: one
 * pair leaves each summit and one pair each pass, to either flank. The
 * valleys draining the flanks are the gaps between them.
 */
std::vector<RidgeSegment> branchSegmentsFrom(const std::vector<RangePeak>& peaks,
                                             const RangeAxis& axis, double halfWidthMeters,
                                             std::uint64_t seed) {
    std::vector<RidgeSegment> segments;
    const Heading eastFlank{axis.acrossEast, axis.acrossNorth};
    const Heading westFlank{-axis.acrossEast, -axis.acrossNorth};
    const std::size_t originCount = peaks.size() * 2 - 1;

    for (std::size_t originIndex = 0; originIndex < originCount; ++originIndex) {
        const std::size_t peakIndex = originIndex / 2;
        GroundPoint origin = pointOf(peaks[peakIndex]);
        if (originIndex % 2 == 1) {
            origin = midpointOf(peaks[peakIndex], peaks[peakIndex + 1]);
        }
        const std::uint64_t originSeed = terrainSeedForLane(seed, originIndex);
        appendBranch(origin, eastFlank, halfWidthMeters, terrainSeedForLane(originSeed, 0),
                     segments);
        appendBranch(origin, westFlank, halfWidthMeters, terrainSeedForLane(originSeed, 1),
                     segments);
    }
    return segments;
}

double nearestSegmentDistance(const std::vector<RidgeSegment>& segments,
                              const GroundPoint& ground) {
    double nearest = 1.0e30;
    for (const RidgeSegment& segment : segments) {
        nearest = std::min(nearest, segmentDistance(ground.eastingMeters, ground.northingMeters,
                                                    segment.first.eastingMeters,
                                                    segment.first.northingMeters,
                                                    segment.second.eastingMeters,
                                                    segment.second.northingMeters));
    }
    return nearest;
}

float smoothFall(double share) {
    const float clamped = static_cast<float>(std::clamp(share, 0.0, 1.0));
    return 1.0f - clamped * clamped * (3.0f - 2.0f * clamped);
}

/**
 * How high the crest stands at a position along the range: the summits'
 * own heights, dipping to the saddle height in each pass and tapering off
 * beyond the end summits over one summit spacing.
 */
float crestHeightAlong(const std::vector<RangePeak>& peaks, double alongMeters,
                       float saddleHeight, double spacingMeters) {
    const RangePeak& first = peaks.front();
    const RangePeak& last = peaks.back();
    if (alongMeters <= first.alongMeters) {
        return first.heightFraction * smoothFall((first.alongMeters - alongMeters) / spacingMeters);
    }
    if (alongMeters >= last.alongMeters) {
        return last.heightFraction * smoothFall((alongMeters - last.alongMeters) / spacingMeters);
    }

    std::size_t index = 1;
    while (peaks[index].alongMeters < alongMeters) {
        ++index;
    }
    const RangePeak& before = peaks[index - 1];
    const RangePeak& after = peaks[index];
    const double share = (alongMeters - before.alongMeters)
                         / std::max(after.alongMeters - before.alongMeters, 1.0e-6);
    const float crest = static_cast<float>(before.heightFraction
                                           + (after.heightFraction - before.heightFraction) * share);
    const float dip = static_cast<float>(std::sin(share * std::numbers::pi));
    return crest * (1.0f - (1.0f - saddleHeight) * dip * dip);
}

/**
 * The range's answer to `ringReadPoint`: the ground point pulled toward the
 * spine across the range rather than toward a summit. Damping the across-spine
 * coordinate stretches the noise the same way, so its features become gullies
 * running down the flanks instead of crumple laid over the whole chain.
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

/// How much of the ridge structure the transverse ridges set, against the
/// fine gullies laid over them.
constexpr float transverseShare = 0.7f;

/// The deepest a valley may be cut, as a share of the mass above its floor.
/// Cutting to the base leaves walls no rock stands at.
constexpr float deepestValleyShare = 0.85f;

/// How far the ground may be bent per unit of irregularity, as a share of the
/// distance the bend is drawn over. Past about a quarter the displacement's own
/// gradient exceeds one and the field folds over itself, which stretches one
/// side of every fold into a smear and stacks the other into a cliff.
constexpr double rangeWarpShare = 0.2;

/**
 * Everything about a range that is fixed before the first cell is read. The
 * per-cell work is a handful of distance queries against these.
 */
struct RangeLayout {
    ShapePlacement placement;
    RangeAxis axis;
    std::vector<RangePeak> peaks;
    std::vector<RidgeSegment> spine;
    std::vector<RidgeSegment> branches;
    double halfWidthMeters = 0.0;
    double summitSpacingMeters = 0.0;
    /// How far from the nearest ridge line the ground is still that ridge's
    /// flank rather than the valley floor between two of them.
    double valleyReachMeters = 0.0;
    float saddleHeight = 0.5f;
    float peakSharpness = 1.6f;
};

/**
 * The mass of the range at a point: the crest height at that position along
 * the axis, falling straight to the foot across the spine, concave where the
 * peaks are sharpened.
 */
float rangeMass(const RangeLayout& layout, const GroundPoint& ground) {
    const double east = ground.eastingMeters - layout.placement.centreEastingMeters;
    const double north = ground.northingMeters - layout.placement.centreNorthingMeters;
    const double along = east * layout.axis.alongEast + north * layout.axis.alongNorth;
    const float crest =
        crestHeightAlong(layout.peaks, along, layout.saddleHeight, layout.summitSpacingMeters);
    const float across = radialProfileValue(RadialProfile::Linear,
                                            nearestSegmentDistance(layout.spine, ground),
                                            layout.halfWidthMeters);
    return std::pow(across, layout.peakSharpness) * crest;
}

/**
 * 1 on a ridge line, 0 on the valley floor between two of them. The fall is
 * steepest at the crest and flattens into the floor, so ridges stand sharp
 * over broad valleys rather than rolling over into narrow gullies.
 */
float transverseRidges(const RangeLayout& layout, const GroundPoint& ground) {
    const double toRidge = std::min(nearestSegmentDistance(layout.spine, ground),
                                    nearestSegmentDistance(layout.branches, ground));
    const float acrossValley =
        static_cast<float>(std::clamp(toRidge / layout.valleyReachMeters, 0.0, 1.0));
    return (1.0f - acrossValley) * (1.0f - acrossValley);
}

/**
 * The valleys are cut at full depth right up to the crest and released only at
 * the summits themselves, so the tallest summit still reaches the height it
 * was given while every flank below it is ridge and valley all the way down.
 */
float carvedRange(float mass, float ridges, float valleyDepth) {
    const float summitFade = std::clamp((1.0f - mass) * 8.0f, 0.0f, 1.0f);
    return mass * (1.0f - valleyDepth * summitFade * (1.0f - ridges));
}

void evaluateMountainRange(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const WorldFrame frame = worldFrameFor(api, context);
    const double lengthMeters = api->parameterNumber(context, 3);
    const double widthMeters = api->parameterNumber(context, 4);
    const int peakCount = static_cast<int>(api->parameterNumber(context, 5));
    const float height = static_cast<float>(api->parameterNumber(context, 6));
    const std::uint64_t seed = terrainSeedForLane(
        api->randomSeed(context),
        static_cast<std::uint64_t>(api->parameterNumber(context, 10)));
    const float valleyDepth =
        static_cast<float>(api->parameterNumber(context, 12)) * deepestValleyShare;
    const float featureSizeMeters = static_cast<float>(api->parameterNumber(context, 13));
    const float baseAltitude = static_cast<float>(api->parameterNumber(context, 14));
    const float spurElongation = static_cast<float>(api->parameterNumber(context, 15));

    RangeLayout layout;
    layout.placement = placementFrom(frame, api->parameterNumber(context, 0),
                                     api->parameterNumber(context, 1), 0.0);
    layout.axis = axisFor(api->parameterNumber(context, 2));
    layout.peaks = spineFrom(layout.placement, layout.axis, lengthMeters, widthMeters, peakCount,
                             static_cast<float>(api->parameterNumber(context, 7)),
                             terrainSeedForLane(seed, 4));
    layout.spine = spineSegmentsFrom(layout.peaks);
    layout.halfWidthMeters = widthMeters * 0.5;
    layout.branches = branchSegmentsFrom(layout.peaks, layout.axis, layout.halfWidthMeters,
                                         terrainSeedForLane(seed, 5));
    layout.summitSpacingMeters = lengthMeters / peakCount;
    layout.valleyReachMeters = layout.summitSpacingMeters * 0.3;
    layout.saddleHeight = static_cast<float>(api->parameterNumber(context, 8));
    layout.peakSharpness = static_cast<float>(api->parameterNumber(context, 9));

    // The bend is drawn at the summit spacing rather than the width, so a
    // long range meanders between its summits instead of swaying as one.
    DomainWarp warp;
    warp.eastingSeed = terrainSeedForLane(seed, 1);
    warp.northingSeed = terrainSeedForLane(seed, 2);
    warp.featureSizeMeters = static_cast<float>(layout.summitSpacingMeters);
    warp.amountMeters = static_cast<float>(api->parameterNumber(context, 11)
                                           * layout.summitSpacingMeters * rangeWarpShare);
    const std::uint64_t detailSeed = terrainSeedForLane(seed, 3);
    float* output = api->outputField(context, 0);

    forEachRowRange(api, context, frame.size, [&](int firstRow, int lastRow) {
        for (int row = firstRow; row < lastRow; ++row) {
            float* line = output + static_cast<std::size_t>(row) * frame.size;
            for (int column = 0; column < frame.size; ++column) {
                const GroundPoint ground =
                    warpedBy(warp, frame.eastingAt(column), frame.northingAt(row));
                const auto readPointFor = [&](float elongation) {
                    return ribReadPoint(ground, layout.placement, layout.axis, elongation);
                };
                const float gullies =
                    craggyField(readPointFor, featureSizeMeters, spurElongation, 1.0f,
                                rangeElongationRelease, detailSeed);
                const float ridges = transverseRidges(layout, ground) * transverseShare
                                     + gullies * (1.0f - transverseShare);
                line[column] =
                    carvedRange(rangeMass(layout, ground), ridges, valleyDepth) * height
                    + baseAltitude;
            }
        }
    });
}

const TerrainNodeDescriptor mountainRangeDescriptor = {
    "MountainRange",
    "Terrain",
    "A chain of summits on one spine, with transverse ridges and the valleys between them",
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
