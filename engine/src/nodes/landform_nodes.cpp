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
     "m", TerrainParameterType_Float, 1400.0, 1.0, 100000.0, "", nullptr, 0},
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
constexpr double summitCrestShare = 0.2;

/* ------------------------------------------------------------ spur cells -- */

/**
 * The spurs and the valleys between them, drawn as Gaea's docs describe their
 * own Mountain: a modulated Voronoi pattern under distortion.
 *
 * The cells are laid out around the summit in a log-polar frame, so every
 * cell is a wedge pointing at the summit and the cells are as fine near the
 * summit as they are coarse at the foot. Each cell is a cone falling away
 * from its own point, stretched along the wedge: a spur with a rounded back,
 * meeting its neighbours in the V of a gully along every cell edge. The frame
 * is read around the loop with a whole number of cells, so it closes without
 * a seam. Finer octaves lay smaller spurs and gullies over the large ones.
 */
struct SpurCells {
    /// How many cells the coarsest octave carries around the summit.
    int countAround = 12;
    /// How far a cell runs toward the foot against its width around.
    double stretch = 3.0;
    /// The radius the log-polar frame is read from, so cells near the summit
    /// stay about this size rather than shrinking without end.
    double innerMeters = 100.0;
    int octaveCount = 5;
    float gain = 0.3f;
    /// How much smaller than a cell the ground held clear around the summit
    /// is.
    float summitHoldSharpness = 4.0f;
    /// How sharply the ground leaves a ridge. Below one the crest is a knife
    /// edge over steep upper flanks that ease into the valley.
    float ridgeSharpness = 0.75f;
    float detailRetention = 1.0f;
    /// How much the depth of the valleys varies slowly around the mountain,
    /// so some spurs stand well clear of their neighbours and others barely
    /// show.
    float depthWanderShare = 0.5f;
    float depthWanderFeatureCells = 2.5f;
    /// Wander of the cell edges, in cells, and the size of that wander.
    float edgeWanderCells = 0.35f;
    float edgeWanderFeatureCells = 1.5f;
    std::uint64_t seed = 0;
};

/// A point in the frame the cells are read in: turns around the summit and
/// stretched distance out from it.
struct PolarPoint {
    double around = 0.0;
    double outward = 0.0;
};

PolarPoint polarAbout(const GroundPoint& centre, const GroundPoint& ground, double innerMeters) {
    const double east = ground.eastingMeters - centre.eastingMeters;
    const double north = ground.northingMeters - centre.northingMeters;
    const double turns = std::atan2(north, east) / (2.0 * std::numbers::pi) + 0.5;
    return PolarPoint{turns, std::log(std::hypot(east, north) / innerMeters + 1.0)};
}

/// How much of an octave survives when the finest detail is held back.
/// Retention counts octaves, so holding detail back drops the smallest cells
/// outright and leaves the larger ones at full depth.
float cellOctaveWeight(float detailRetention, int octave, int octaveCount) {
    const float keptOctaves = std::max(detailRetention * static_cast<float>(octaveCount), 1.0f);
    return std::clamp(keptOctaves - static_cast<float>(octave), 0.0f, 1.0f);
}

/// The wander laid over the cells is read on the ground rather than in the
/// polar frame, because a field read in the frame would not close around the
/// loop and would seam the mountain along one ray.
struct CellWander {
    float edgeEast = 0.0f;
    float edgeNorth = 0.0f;
    float depth = 0.0f;
};

CellWander cellWanderAt(const GroundPoint& ground, double featureMeters, const SpurCells& cells,
                        std::uint64_t seed) {
    const float x = static_cast<float>(ground.eastingMeters / featureMeters);
    const float y = static_cast<float>(ground.northingMeters / featureMeters);
    CellWander wander;
    wander.edgeEast = noise::perlin(x, y, terrainSeedForLane(seed, 101)) * cells.edgeWanderCells;
    wander.edgeNorth = noise::perlin(x, y, terrainSeedForLane(seed, 102)) * cells.edgeWanderCells;
    wander.depth = noise::perlin(x / cells.depthWanderFeatureCells, y / cells.depthWanderFeatureCells,
                                 terrainSeedForLane(seed, 103));
    return wander;
}

/// One octave of cells: 0 on a cell edge, rising toward the cell's own point.
float spurCellOctave(const PolarPoint& polar, const CellWander& wander, int countAround,
                     const SpurCells& cells, std::uint64_t seed) {
    // One turn around is `countAround` cells; the outward axis is scaled so a
    // cell is `stretch` times as long as it is wide.
    const double cellsPerTurn = static_cast<double>(countAround);
    const float x = static_cast<float>(polar.around * cellsPerTurn);
    const float y =
        static_cast<float>(polar.outward * cellsPerTurn / (2.0 * std::numbers::pi * cells.stretch));
    const noise::VoronoiSample sample = noise::voronoiPeriodic(
        x + wander.edgeEast, y + wander.edgeNorth, seed, 1.0f, countAround);
    // Ridges run along the cell edges, where the two nearest points are
    // equally far, and the ground falls away from them into the cell: a
    // network of arêtes meeting at every junction, with a valley down every
    // cell between them. The summit itself is held clear, so it is where the
    // ridges meet rather than the floor of whichever cell it fell in.
    const float intoCell = std::min(sample.secondDistance - sample.nearestDistance, 1.0f);
    const float toSummit = static_cast<float>(polar.outward * cellsPerTurn
                                              / (2.0 * std::numbers::pi * cells.stretch))
                           * cells.summitHoldSharpness;
    const float valley =
        std::pow(intoCell, cells.ridgeSharpness) * smoothStep(std::min(toSummit, 1.0f));
    return valley * (1.0f + wander.depth * cells.depthWanderShare);
}

float spurCells(const PolarPoint& polar, const GroundPoint& ground, double radiusMeters,
                const SpurCells& cells) {
    float total = 0.0f;
    float weightSum = 0.0f;
    float amplitude = 1.0f;
    int countAround = cells.countAround;
    // A cell of the coarsest octave is about this wide at the foot; the
    // wander of each octave is read at its own cell size.
    double featureMeters = 2.0 * std::numbers::pi * radiusMeters / countAround
                           * cells.edgeWanderFeatureCells;
    for (int octave = 0; octave < cells.octaveCount; ++octave) {
        const float weight =
            amplitude * cellOctaveWeight(cells.detailRetention, octave, cells.octaveCount);
        const std::uint64_t octaveSeed =
            terrainSeedForLane(cells.seed, static_cast<std::uint64_t>(octave));
        const CellWander wander = cellWanderAt(ground, featureMeters, cells, octaveSeed);
        total += spurCellOctave(polar, wander, countAround, cells, octaveSeed) * weight;
        featureMeters *= 0.5;
        weightSum += amplitude;
        amplitude *= cells.gain;
        countAround *= 2;
    }
    return total / weightSum;
}

/* ----------------------------------------------------------- the massif -- */

/// How much of the mass the floor of a valley between two spurs may lose at
/// full detail. The cut is a share of the mass standing there, so the valleys
/// of a tall mountain are as deep as the mountain is tall, and the relief
/// dies away with the mass into the apron at the foot.
constexpr float valleyDepthLimit = 0.8f;

/// How many cells the coarsest octave carries around the summit per unit of
/// foot perimeter over spur spacing.
constexpr int leastCellsAround = 4;

/// Everything about a mountain that is fixed before the first cell is read.
struct MountainLayout {
    ShapePlacement placement;
    DomainWarp warp;
    RidgeSegment summitCrest;
    SpurCells cells;
    double radiusMeters = 3000.0;
    double outlineFeatureSizeMeters = 3000.0;
    float outlineWanderShare = 0.0f;
    float profileExponent = 1.6f;
    float valleyDepth = 0.0f;
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
    const GroundPoint centre{layout.placement.centreEastingMeters,
                             layout.placement.centreNorthingMeters};
    const float valleys = spurCells(polarAbout(centre, ground, layout.cells.innerMeters), ground,
                                    layout.radiusMeters, layout.cells);
    return mass * (1.0f - layout.valleyDepth * valleys) * layout.heightMeters
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
    layout.cells.countAround = std::max(
        static_cast<int>(std::round(2.0 * std::numbers::pi * layout.radiusMeters / spacingMeters)),
        leastCellsAround);
    layout.cells.stretch = 1.0 / (1.0 - api->parameterNumber(context, 14));
    layout.cells.innerMeters = spacingMeters * 0.1;
    layout.cells.detailRetention = 1.0f - static_cast<float>(api->parameterNumber(context, 4));
    layout.cells.seed = terrainSeedForLane(seed, 3);
    layout.outlineFeatureSizeMeters = layout.radiusMeters * std::numbers::pi / outlineLobeCount;
    layout.outlineWanderShare = irregularity * outlineWanderLimit;
    layout.heightMeters = static_cast<float>(api->parameterNumber(context, 1));
    layout.baseAltitudeMeters = static_cast<float>(api->parameterNumber(context, 9));
    layout.valleyDepth = static_cast<float>(api->parameterNumber(context, 12)) * valleyDepthLimit;
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
    "A single mountain: a summit with spurs and valleys radiating to the foot",
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
