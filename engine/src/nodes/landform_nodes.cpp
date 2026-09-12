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

/**
 * The catchments, cut out from under the mass and left alone over the last
 * stretch below the summit so the peak stays a single peak.
 *
 * The fade is deliberately narrow. Relief in a real range does not shrink as
 * you climb; it runs at full depth right up to the crest and stops only at the
 * summit itself. Fading it across the whole flank instead is what turns a
 * mountain into a smooth dome wearing a crumpled skirt.
 *
 * `carveDepth` is the deepest share of the landform's height the catchments may
 * take. It is held at that depth over the whole flank and eased off only where
 * there is no longer the mass to take it: a gully at the foot of a mountain is
 * cut as deeply as one halfway up, and scaling the carve by the mass instead
 * leaves the lower half of a massif that fills its tile smooth.
 */
/// How quickly the catchments reach full depth below the summit, per unit of
/// mass given up. The summit itself has to stay one summit; a hand's breadth
/// below it the flank is already ridge and valley.
constexpr float summitReleaseRate = 16.0f;

/// The mass at which the catchments reach their full depth. Easing off below
/// it keeps the outwash at the foot from being cut as deeply as the flank
/// above it, and keeps any cut from reaching below the plane.
constexpr float carveHoldShare = 0.35f;

float cragged(float shaped, float ridges, float carveDepth) {
    const float summitFade = std::clamp((1.0f - shaped) * summitReleaseRate, 0.0f, 1.0f);
    const float footFade =
        std::clamp(shaped / std::max(carveHoldShare, carveDepth), 0.0f, 1.0f);
    return shaped - carveDepth * summitFade * footFade * (1.0f - ridges);
}

/**
 * How deep the catchments are cut, as a share of the distance between the
 * divides on either side of them.
 *
 * A valley carved as deep as the mountain is tall, across ground a few hundred
 * metres wide, leaves walls no rock would stand at, and a field of such walls
 * reads as spikes however the ridges themselves are shaped. Relief in real
 * mountains runs at a fraction of the spacing between the divides, so the
 * depth has to follow the detail size rather than the height.
 */
constexpr float reliefToSpacing = 0.5f;

constexpr int ridgeOctaveCount = 4;

/// Each octave doubles in frequency and halves in depth, so every network
/// carves in proportion to how far apart its own ridges are. Any shallower
/// fall-off buries the main valleys under gullies as deep as they are; any
/// steeper one fades the fine ground out before it reaches a readable size.
constexpr float ridgeLacunarity = 2.0f;
constexpr float ridgeGain = 1.0f / ridgeLacunarity;

/// How much of an octave's stretch the next finer one keeps. A massif's
/// gullies run downslope at every size, so a mountain hands its stretch nearly
/// intact to the finest octave; a range lets go of it faster, because the
/// crumple on a distant flank of a chain is not read as a gully.
constexpr float mountainElongationRelease = 0.85f;
constexpr float rangeElongationRelease = 0.4f;

/// A carve depth in the [0, 1] the shaping profile works in.
float carveDepthFor(float detailStrength, float featureSizeMeters, float heightMeters) {
    if (heightMeters < 1.0f) {
        return 0.0f;
    }
    return detailStrength * featureSizeMeters * reliefToSpacing / heightMeters;
}

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

/// The ground point pulled toward an anchor. Against a line anchor this
/// compresses only the distance from that line, which stretches whatever is
/// read from it along the direction the point leaves the line by.
GroundPoint pulledToward(const GroundPoint& anchor, const GroundPoint& ground, float elongation) {
    const double kept = 1.0 - elongation;
    return GroundPoint{anchor.eastingMeters + (ground.eastingMeters - anchor.eastingMeters) * kept,
                       anchor.northingMeters
                           + (ground.northingMeters - anchor.northingMeters) * kept};
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
     TerrainParameterType_Float, 3000.0, 1.0, 100000.0, "", nullptr, 0},
    {"baseAltitude", "Base altitude", "The altitude the foot of the mountain sits at", "m",
     TerrainParameterType_Float, 0.0, -12000.0, 12000.0, "", nullptr, 0},
    {"peakSharpness", "Peak sharpness", "Higher values pull the mass down and leave a finer peak",
     "", TerrainParameterType_Float, 1.6, 0.2, 6.0, "", nullptr, 0},
    {"irregularity", "Irregularity", "How far the outline wanders off a circle", "",
     TerrainParameterType_Float, 0.35, 0.0, 1.0, "", nullptr, 0},
    {"detailStrength", "Detail", "How much ridged detail is cut into the slopes", "",
     TerrainParameterType_Float, 0.5, 0.0, 1.0, "", nullptr, 0},
    {"featureSize", "Detail size", "Ground distance between neighbouring spurs, before Scale", "m",
     TerrainParameterType_Float, 1200.0, 1.0, 100000.0, "", nullptr, 0},
    {"spurElongation", "Spur length",
     "How far the detail is drawn out into spurs running from the summit to the foot", "",
     TerrainParameterType_Float, 0.6, 0.0, 0.95, "", nullptr, 0}};

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
/// gradient exceeds one and the field folds over itself. The bend is what takes
/// the straightness out of the skeleton's ridge lines, so it is drawn over a
/// distance half again the mountain's own and can afford to be generous.
constexpr double mountainWarpShare = 0.25;
constexpr double mountainWarpFeatureShare = 1.5;

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

/* -------------------------------------------------------- crest skeleton -- */

/**
 * How far below its own ridge the ground has fallen at a distance from it, as
 * a share of the mountain's height: steep where it leaves the crest and
 * tangent to the plane at the end of the run, so the flank meets the foot
 * through a footslope rather than at an angle.
 */
struct DescentProfile {
    double runMeters = 1.0;
    float exponent = 1.6f;
};

float descentAt(double distanceMeters, const DescentProfile& descent) {
    const double unit = std::clamp(distanceMeters / descent.runMeters, 0.0, 1.0);
    return 1.0f - static_cast<float>(std::pow(1.0 - unit, descent.exponent));
}

/**
 * How much of that descent a ridge standing below the summit takes, against
 * how much follows its own height.
 *
 * At zero a ridge's flank is its height times a profile, and the tallest ridge
 * then swallows every shorter one beside it: the massif reads as one dome with
 * creases in it. At one every ridge falls at the same rate whatever height it
 * stands at, which cuts a true valley between neighbours but leaves the low
 * spurs at the foot with no reach at all. Most of the way to one keeps both.
 */
constexpr float descentHeightMix = 0.7f;

float descentRateFor(float crestHeight) {
    return descentHeightMix + (1.0f - descentHeightMix) * std::max(crestHeight, 0.0f);
}

/**
 * How a ridge's flank falls: a short steep drop into the valley beside it,
 * over the mountain's own long fall towards the foot.
 *
 * One scale cannot do both jobs. A fall drawn over the whole reach to the foot
 * gives up only a few per cent of the height over the few hundred metres
 * between one ridge and the next, so the valleys between the spurs come out as
 * creases in a dome; a fall drawn over the spacing alone reaches the plane a
 * few hundred metres from every ridge and leaves holes wherever the ridges are
 * far apart. Summing the two puts a valley of real depth beside every ridge
 * and still carries the mass out to the foot.
 */
struct FlankProfile {
    DescentProfile local;
    DescentProfile broad;
    float localShare = 0.3f;
};

float flankFall(double distanceMeters, const FlankProfile& flank) {
    return flank.localShare * descentAt(distanceMeters, flank.local)
           + (1.0f - flank.localShare) * descentAt(distanceMeters, flank.broad);
}

/// One halving of the bracket a ridge's reach is known to lie in.
struct ReachBracket {
    double lowMeters = 0.0;
    double highMeters = 0.0;
};

ReachBracket halvedTowardReach(const ReachBracket& bracket, const FlankProfile& flank,
                               float fallen) {
    const double middleMeters = (bracket.lowMeters + bracket.highMeters) * 0.5;
    if (flankFall(middleMeters, flank) < fallen) {
        return ReachBracket{middleMeters, bracket.highMeters};
    }
    return ReachBracket{bracket.lowMeters, middleMeters};
}

/// How far from a ridge standing at `crestHeight` its own mass reaches before
/// it meets the plane. The two-scale fall has no inverse in closed form, so the
/// answer is bracketed in a fixed number of halvings — fixed, because a count
/// that followed the numbers would make the skeleton depend on them.
constexpr int reachHalvingCount = 28;

double flankReach(float crestHeight, const FlankProfile& flank) {
    if (crestHeight <= 0.0f) {
        return 0.0;
    }
    const float fallen = crestHeight / descentRateFor(crestHeight);
    if (fallen >= 1.0f) {
        return flank.broad.runMeters;
    }
    ReachBracket bracket{0.0, flank.broad.runMeters};
    for (int halving = 0; halving < reachHalvingCount; ++halving) {
        bracket = halvedTowardReach(bracket, flank, fallen);
    }
    return bracket.highMeters;
}

/**
 * A mountain is not a mass falling away from a point. It is a crest with
 * summits along it and ridges branching off it, and every slope on it belongs
 * to one of those ridges. Hanging the mass off a tree of ridge lines rather
 * than off a centre is what puts a summit crest, a saddle, a shoulder and a
 * spur on the flank; a radial profile can only ever draw a dome.
 */
struct CrestSegment {
    RidgeSegment line;
    /// Height at each end, as a share of the mountain's own height.
    float firstHeight = 1.0f;
    float secondHeight = 1.0f;
    /// How far this ridge's mass reaches before it meets the plane, so ground
    /// beyond it can be passed over without paying for the profile.
    double reachMeters = 0.0;
};

/// A point on the skeleton: where the ridge line is, how high it stands there,
/// and how far from the summit along the ridges it sits — which is what says
/// how far it has already descended.
struct RidgePoint {
    GroundPoint point;
    float height = 1.0f;
    double alongMeters = 0.0;
};

/// What drawing a skeleton needs to know about the mountain it belongs to.
struct SkeletonPlan {
    double radiusMeters = 3000.0;
    double summitShoulderMeters = 200.0;
    /// How the ridge lines themselves lose height as they run, which is the
    /// mountain's own long fall and not the drop into the valley beside them.
    DescentProfile descent;
    FlankProfile flank;
};

constexpr int crestNodeCount = 6;

/// Where each crest node sits along the crest, as a share of its length
/// measured from the summit. The summit sits at zero and the crest runs far
/// one way and short the other, because a mountain has a main ridge and a
/// shoulder rather than two arms of equal reach.
constexpr double crestNodeAlongShares[crestNodeCount] = {-0.34, -0.05, 0.05, 0.32, 0.66, 1.0};

/// The crest's end-to-end length, as a share of the reach to the foot. The
/// main ridge runs the length of the massif; anything shorter leaves a dome
/// with a wrinkle on top of it.
constexpr double crestLengthShare = 0.95;

/// How far a crest node may wander off the crest's own line, as a share of how
/// far along the crest it sits. Drawn against its own distance rather than
/// against the whole length, the crest curves away from the summit instead of
/// zigzagging across it.
constexpr double crestWanderShare = 0.25;

/// How far the summit holds its height along the crest, as a share of the
/// detail size either way. A summit is the high point of an arête rather than
/// the tip of a cone: it is a block of rock of the detail's own size, and a
/// taller mountain gets a higher summit rather than a broader one.
constexpr double summitShoulderOfFeature = 0.16;

/// How much of a ridge's fall is taken in the valley beside it rather than on
/// the way to the foot, and over what distance. The run is what sets how far
/// apart two ridges have to be before the ground between them reads as a
/// valley rather than as a crease.
constexpr float flankLocalShare = 0.12f;
constexpr double flankLocalRunShare = 0.26;

/// How wide a ridge's own crest is, as a share of the detail size. A ridge top
/// is not a knife edge: it holds its height over a few tens of metres before
/// the flank starts. That width belongs to the rock rather than to the massif,
/// so a mountain twice the size does not get ridges twice as broad.
constexpr double ridgeCrestOfFeature = 0.04;

/// How far a gully wanders off the comb, and over what distance, both against
/// the spacing between gullies. A comb with nothing laid over it draws the
/// detail into straight parallel streaks; the wander is what makes them
/// gullies that find their own way down.
constexpr float meanderShare = 0.55f;
constexpr float meanderFeatureShare = 2.5f;

/// How much the crest's descent varies from node to node, which is what leaves
/// subsidiary highs and saddles along the main ridge.
constexpr float crestSaddleRelief = 0.35f;

/**
 * How much of the descent a ridge line takes as it runs, against the open
 * ground falling away on either side of it. Below one the ridge stands proud,
 * and that stand is the relief of the valley beside it. The main crest keeps
 * the most and each finer generation of spur keeps less, so the massif reads
 * as one mountain rather than a thicket of equal ridges.
 */
constexpr float crestStandShare = 0.62f;
constexpr float spurStandShares[] = {0.68f, 0.80f};
constexpr int branchDepthLimit = 1;

/// A ridge gives up its stand as it nears the foot: one that kept it all the
/// way out would end as a wall standing over the plane instead of dying into
/// it.
float standShareAt(float share, double alongMeters, double radiusMeters) {
    const double toFoot = std::clamp(alongMeters / radiusMeters, 0.0, 1.0);
    return share + (1.0f - share) * static_cast<float>(toFoot * toFoot);
}

RidgePoint ridgeStepTo(const RidgePoint& from, const Heading& heading, double runMeters,
                       const SkeletonPlan& plan, float standShare) {
    const double alongMeters = from.alongMeters + runMeters;
    const float fallen =
        descentAt(alongMeters, plan.descent) - descentAt(from.alongMeters, plan.descent);
    const float share = standShareAt(standShare, alongMeters, plan.radiusMeters);
    return RidgePoint{stepFrom(from.point, heading, runMeters), from.height - fallen * share,
                      alongMeters};
}

void appendRidge(const RidgePoint& first, const RidgePoint& second, const SkeletonPlan& plan,
                 std::vector<CrestSegment>& segments) {
    const float tallest = std::max(first.height, second.height);
    segments.push_back(CrestSegment{RidgeSegment{first.point, second.point}, first.height,
                                    second.height, flankReach(tallest, plan.flank)});
}

/// How many spurs leave every crest node to either flank. A flank wants
/// several: two ridges on a mountain is a starfish, a dozen is a mountain.
constexpr int spursPerFlank = 4;

/// How far a spur runs, as a share of the ground left between where it leaves
/// its parent and the foot.
constexpr double spurReachLow = 0.45;
constexpr double spurReachHigh = 0.95;

/// How far a spur may lean off the way it was sent, how far it may turn again
/// at its knee, and how far the next generation leaves the spur it branches
/// from.
constexpr double spurLeanRadians = 38.0 * std::numbers::pi / 180.0;
constexpr double spurBendRadians = 22.0 * std::numbers::pi / 180.0;
constexpr double spurBranchRadians = 42.0 * std::numbers::pi / 180.0;

/// The shortest run worth drawing, as a share of the reach to the foot.
constexpr double shortestSpurShare = 0.08;

/// How many runs a spur is walked in. Two leave a ridge that reads as two
/// straight lines meeting at an elbow, and the ground either side of them as
/// flat facets; more runs let the ridge curve and the ground curve with it.
constexpr int spurRunCount = 3;

/**
 * One spur running off the ridge that bore it: a walk of several runs losing
 * height all the way, with a spur of the next generation leaving two of its
 * nodes to opposite sides.
 *
 * Branching is what a mountain's drainage is. The ground between a spur and
 * the one beside it is a valley; the ground between a spur and its own branch
 * is a tributary of that valley, and the two meet where the ridges do.
 */
void appendSpur(const RidgePoint& origin, const Heading& outward, const SkeletonPlan& plan,
                int depth, std::uint64_t seed, std::vector<CrestSegment>& segments);

/// The ridge line itself: a walk of several runs, each turned a little off the
/// last and each lower than the last.
std::vector<RidgePoint> walkSpur(const RidgePoint& origin, Heading heading, double reachMeters,
                                 const SkeletonPlan& plan, float standShare, std::uint64_t* state,
                                 std::vector<CrestSegment>& segments) {
    std::vector<RidgePoint> nodes;
    nodes.reserve(spurRunCount + 1);
    nodes.push_back(origin);
    for (int run = 0; run < spurRunCount; ++run) {
        const RidgePoint next =
            ridgeStepTo(nodes.back(), heading, reachMeters / spurRunCount, plan, standShare);
        appendRidge(nodes.back(), next, plan, segments);
        nodes.push_back(next);
        heading = heading.turned(spreadFrom(state, spurBendRadians));
    }
    return nodes;
}

/// A spur of the next generation, leaving one node of this one to the side it
/// is given.
void appendSpurBranch(const std::vector<RidgePoint>& nodes, std::size_t index, double turnRadians,
                      const SkeletonPlan& plan, int depth, std::uint64_t seed,
                      std::vector<CrestSegment>& segments) {
    const Heading along = headingFrom(nodes[index - 1].point, nodes[index].point);
    appendSpur(nodes[index], along.turned(turnRadians), plan, depth + 1, seed, segments);
}

void appendSpur(const RidgePoint& origin, const Heading& outward, const SkeletonPlan& plan,
                int depth, std::uint64_t seed, std::vector<CrestSegment>& segments) {
    const double remainingMeters = plan.radiusMeters - origin.alongMeters;
    if (depth > branchDepthLimit || origin.height <= 0.0f
        || remainingMeters < plan.radiusMeters * shortestSpurShare) {
        return;
    }
    std::uint64_t state = seed;
    const double reachMeters =
        remainingMeters
        * (spurReachLow + (spurReachHigh - spurReachLow) * terrainRandomUnitFloat(&state));
    const Heading start = outward.turned(spreadFrom(&state, spurLeanRadians));
    const std::vector<RidgePoint> nodes = walkSpur(origin, start, reachMeters, plan,
                                                   spurStandShares[depth], &state, segments);
    appendSpurBranch(nodes, 1, spurBranchRadians, plan, depth, terrainSeedForLane(seed, 1),
                     segments);
    appendSpurBranch(nodes, 2, -spurBranchRadians, plan, depth, terrainSeedForLane(seed, 2),
                     segments);
}

/**
 * One node of the summit crest: stepped off the crest's own line and standing
 * at a height of its own, so the ridge bends and carries highs and saddles
 * rather than running straight and level.
 *
 * Every node is drawn from a seed derived from its own index, so a node keeps
 * its place however the rest of the skeleton changes.
 */
RidgePoint crestNodeAt(const GroundPoint& summit, const Heading& alongCrest,
                       const Heading& acrossCrest, double lengthMeters, const SkeletonPlan& plan,
                       int index, std::uint64_t seed) {
    std::uint64_t state = seed;
    const double alongMeters = crestNodeAlongShares[index] * lengthMeters;
    const double acrossMeters = spreadFrom(&state, crestWanderShare * std::abs(alongMeters));
    const double fromSummitMeters =
        std::max(std::abs(alongMeters) - plan.summitShoulderMeters, 0.0);
    const float saddle = 1.0f + static_cast<float>(spreadFrom(&state, crestSaddleRelief));
    const float share = standShareAt(crestStandShare * saddle, fromSummitMeters, plan.radiusMeters);

    RidgePoint node;
    node.point = stepFrom(stepFrom(summit, alongCrest, alongMeters), acrossCrest, acrossMeters);
    node.height = 1.0f - descentAt(fromSummitMeters, plan.descent) * share;
    node.alongMeters = fromSummitMeters;
    return node;
}

struct Crest {
    std::vector<RidgePoint> nodes;
    Heading acrossCrest;
};

Crest crestFrom(const ShapePlacement& placement, const SkeletonPlan& plan, std::uint64_t seed) {
    std::uint64_t axisState = terrainSeedForLane(seed, 0);
    const double bearingRadians = terrainRandomUnitFloat(&axisState) * 2.0 * std::numbers::pi;
    const Heading alongCrest{std::sin(bearingRadians), std::cos(bearingRadians)};
    const GroundPoint summit{placement.centreEastingMeters, placement.centreNorthingMeters};
    const double lengthMeters = plan.radiusMeters * crestLengthShare;

    Crest crest;
    crest.acrossCrest = alongCrest.turned(std::numbers::pi * 0.5);
    crest.nodes.reserve(crestNodeCount);
    for (int index = 0; index < crestNodeCount; ++index) {
        crest.nodes.push_back(crestNodeAt(summit, alongCrest, crest.acrossCrest, lengthMeters, plan,
                                          index, terrainSeedForLane(seed, index + 1)));
    }
    return crest;
}

void appendCrestLine(const Crest& crest, const SkeletonPlan& plan,
                     std::vector<CrestSegment>& segments) {
    for (std::size_t index = 1; index < crest.nodes.size(); ++index) {
        appendRidge(crest.nodes[index - 1], crest.nodes[index], plan, segments);
    }
}

void appendFlankSpurs(const RidgePoint& node, const Heading& outward, const SkeletonPlan& plan,
                      std::uint64_t seed, std::vector<CrestSegment>& segments) {
    for (int index = 0; index < spursPerFlank; ++index) {
        appendSpur(node, outward, plan, 0, terrainSeedForLane(seed, index), segments);
    }
}

/// Spurs leave every crest node to both flanks, so the massif does not comb
/// all its ridges one way.
void appendCrestSpurs(const Crest& crest, const SkeletonPlan& plan, std::uint64_t seed,
                      std::vector<CrestSegment>& segments) {
    const Heading backCrest{-crest.acrossCrest.east, -crest.acrossCrest.north};
    for (std::size_t index = 0; index < crest.nodes.size(); ++index) {
        const std::uint64_t nodeSeed = terrainSeedForLane(seed, index);
        appendFlankSpurs(crest.nodes[index], crest.acrossCrest, plan,
                         terrainSeedForLane(nodeSeed, 0), segments);
        appendFlankSpurs(crest.nodes[index], backCrest, plan, terrainSeedForLane(nodeSeed, 1),
                         segments);
    }
}

std::vector<CrestSegment> mountainSkeletonFrom(const ShapePlacement& placement,
                                               const SkeletonPlan& plan, std::uint64_t seed) {
    const Crest crest = crestFrom(placement, plan, terrainSeedForLane(seed, 0));
    std::vector<CrestSegment> segments;
    appendCrestLine(crest, plan, segments);
    appendCrestSpurs(crest, plan, terrainSeedForLane(seed, 1), segments);
    return segments;
}

/// Where the outline starts to trim the massif, as a share of the reach. The
/// skeleton draws the mountain; this only keeps it inside the foot the outline
/// sets, and fades rather than cuts so no spur ends on a step.
constexpr double rimTrimStart = 0.85;

/// Where a ridge line comes closest to a point, and how far along the line that
/// is.
struct NearestOnRidge {
    GroundPoint point;
    double distanceMeters = 0.0;
    double alongShare = 0.0;
};

NearestOnRidge nearestOnRidge(const RidgeSegment& segment, const GroundPoint& ground) {
    const double runEast = segment.second.eastingMeters - segment.first.eastingMeters;
    const double runNorth = segment.second.northingMeters - segment.first.northingMeters;
    const double lengthSquared = runEast * runEast + runNorth * runNorth;
    const double toEast = ground.eastingMeters - segment.first.eastingMeters;
    const double toNorth = ground.northingMeters - segment.first.northingMeters;
    double along = 0.0;
    if (lengthSquared > 1.0e-9) {
        along = std::clamp((toEast * runEast + toNorth * runNorth) / lengthSquared, 0.0, 1.0);
    }
    const GroundPoint nearest{segment.first.eastingMeters + runEast * along,
                              segment.first.northingMeters + runNorth * along};
    return NearestOnRidge{nearest,
                          std::hypot(ground.eastingMeters - nearest.eastingMeters,
                                     ground.northingMeters - nearest.northingMeters),
                          along};
}

/**
 * The weighted mean of the points the ridges stand above a cell at, which is
 * the anchor the fine detail is combed away from.
 *
 * The fall line of a mountainside runs away from the ridge above it, not away
 * from the peak: a summit anchor rakes every gully into a pinwheel, and an
 * anchor on the main crest alone brushes the whole massif one way. Weighting
 * steeply enough that the nearest ridge all but decides the anchor puts the
 * comb on the ridge the ground actually belongs to, and it does so without a
 * seam: between two ridges the weights even out, the anchor closes on the
 * point itself and the stretch fades to nothing, which is right — a valley
 * floor has no fall line to comb along.
 */
struct FallAnchor {
    double eastingMeters = 0.0;
    double northingMeters = 0.0;
    double weightSum = 0.0;

    void add(const GroundPoint& ridgePoint, double distanceMeters, double softeningMeters) {
        const double squared =
            distanceMeters * distanceMeters + softeningMeters * softeningMeters;
        const double weight = 1.0 / (squared * squared);
        eastingMeters += ridgePoint.eastingMeters * weight;
        northingMeters += ridgePoint.northingMeters * weight;
        weightSum += weight;
    }

    GroundPoint pointOr(const GroundPoint& fallback) const {
        if (weightSum <= 0.0) {
            return fallback;
        }
        return GroundPoint{eastingMeters / weightSum, northingMeters / weightSum};
    }
};

/* ----------------------------------------------------------- the massif -- */

/// Everything about a mountain that is fixed before the first cell is read.
struct MountainLayout {
    ShapePlacement placement;
    DomainWarp warp;
    std::vector<CrestSegment> skeleton;
    DomainWarp meander;
    FlankProfile flank;
    double radiusMeters = 3000.0;
    double ridgeCrestMeters = 36.0;
    double outlineFeatureSizeMeters = 3000.0;
    float outlineWanderShare = 0.0f;
    float carveDepth = 0.0f;
    float featureSizeMeters = 1200.0f;
    float spurElongation = 0.6f;
    float detailRetention = 1.0f;
    float heightMeters = 1200.0f;
    float baseAltitudeMeters = 0.0f;
    std::uint64_t outlineSeed = 0;
    std::uint64_t detailSeed = 0;
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

/// What one ridge of the skeleton holds up at a point.
float ridgeMass(const CrestSegment& segment, const NearestOnRidge& nearest,
                const FlankProfile& flank, double crestMeters) {
    const double fromCrestMeters = std::max(nearest.distanceMeters - crestMeters, 0.0);
    if (fromCrestMeters >= segment.reachMeters) {
        return 0.0f;
    }
    const float crestHeight = static_cast<float>(
        segment.firstHeight + (segment.secondHeight - segment.firstHeight) * nearest.alongShare);
    return crestHeight - flankFall(fromCrestMeters, flank) * descentRateFor(crestHeight);
}

/// 1 well inside the massif, 0 at the foot the outline draws.
float rimTrim(double distanceMeters, double envelopeMeters) {
    const double unit = distanceMeters / std::max(envelopeMeters, 1.0e-6);
    const double past = (unit - rimTrimStart) / (1.0 - rimTrimStart);
    return 1.0f - smoothStep(static_cast<float>(std::clamp(past, 0.0, 1.0)));
}

/// What the skeleton says about a cell: the mass standing there, and the point
/// the ground there falls away from.
struct SkeletonSample {
    float mass = 0.0f;
    GroundPoint fallAnchor;
};

/**
 * The tallest mass any ridge holds up at a point, kept inside the outline.
 *
 * Where two ridges meet, the taller one wins and the ground between them falls
 * away from both — which is a valley, and the reason a skeleton draws a massif
 * where a radial profile draws a dome. Because every ridge falls at much the
 * same rate whatever height it stands at, the seam between two of them is a
 * crease rather than a rounding: a V that deepens and widens as the ridges
 * either side of it draw apart downslope.
 */
SkeletonSample skeletonSampleAt(const MountainLayout& layout, const GroundPoint& ground) {
    FallAnchor anchor;
    float mass = 0.0f;
    for (const CrestSegment& segment : layout.skeleton) {
        const NearestOnRidge nearest = nearestOnRidge(segment.line, ground);
        mass = std::max(mass, ridgeMass(segment, nearest, layout.flank, layout.ridgeCrestMeters));
        anchor.add(nearest.point, nearest.distanceMeters, layout.ridgeCrestMeters);
    }
    const LocalPoint point =
        layout.placement.localPoint(ground.eastingMeters, ground.northingMeters);
    const float trimmed =
        std::max(mass, 0.0f) * rimTrim(point.distanceFromCentre(), reachAt(layout, ground));
    return SkeletonSample{trimmed, anchor.pointOr(ground)};
}

float mountainHeightAt(const MountainLayout& layout, double eastingMeters, double northingMeters) {
    const GroundPoint ground = warpedBy(layout.warp, eastingMeters, northingMeters);
    const SkeletonSample skeleton = skeletonSampleAt(layout, ground);
    const auto readPointFor = [&](float elongation) {
        return warpedPoint(layout.meander,
                           pulledToward(skeleton.fallAnchor, ground, elongation));
    };
    const float ridges =
        craggyField(readPointFor, layout.featureSizeMeters, layout.spurElongation,
                    layout.detailRetention, mountainElongationRelease, layout.detailSeed);
    return cragged(skeleton.mass, ridges, layout.carveDepth) * layout.heightMeters
           + layout.baseAltitudeMeters;
}

FlankProfile flankProfileFor(double radiusMeters, float exponent) {
    return FlankProfile{DescentProfile{radiusMeters * flankLocalRunShare, exponent},
                        DescentProfile{radiusMeters, exponent}, flankLocalShare};
}

/// The slow bend that takes the straightness out of the skeleton's ridge lines.
DomainWarp mountainWarpFor(double radiusMeters, float irregularity, std::uint64_t seed) {
    DomainWarp warp;
    warp.eastingSeed = terrainSeedForLane(seed, 1);
    warp.northingSeed = terrainSeedForLane(seed, 2);
    warp.featureSizeMeters = static_cast<float>(radiusMeters * mountainWarpFeatureShare);
    warp.amountMeters = static_cast<float>(irregularity * warp.featureSizeMeters
                                           * mountainWarpShare);
    return warp;
}

/// The wander laid over the comb, which is what keeps the gullies from coming
/// out as straight parallel streaks.
DomainWarp gullyMeanderFor(float featureSizeMeters, std::uint64_t seed) {
    DomainWarp meander;
    meander.eastingSeed = terrainSeedForLane(seed, 6);
    meander.northingSeed = terrainSeedForLane(seed, 7);
    meander.featureSizeMeters = featureSizeMeters * meanderFeatureShare;
    meander.amountMeters = featureSizeMeters * meanderShare;
    meander.options = noise::FractalOptions{2, 0.5f, 2.0f};
    return meander;
}

MountainLayout mountainLayoutFrom(const TerrainContextApi* api, TerrainEvaluationContext* context,
                                  const WorldFrame& frame) {
    const double scale = api->parameterNumber(context, 0);
    const std::uint64_t seed = terrainSeedForLane(
        api->randomSeed(context), static_cast<std::uint64_t>(api->parameterNumber(context, 5)));
    const int bulk = static_cast<int>(api->parameterNumber(context, 3));
    const float irregularity = static_cast<float>(api->parameterNumber(context, 11));
    const float exponent =
        static_cast<float>(api->parameterNumber(context, 10)) * bulkExponentScale(bulk);

    MountainLayout layout;
    layout.placement = placementFrom(frame, api->parameterNumber(context, 6),
                                     api->parameterNumber(context, 7), 0.0);
    layout.radiusMeters = api->parameterNumber(context, 8) * scale;
    layout.featureSizeMeters = static_cast<float>(api->parameterNumber(context, 13) * scale);
    layout.ridgeCrestMeters = layout.featureSizeMeters * ridgeCrestOfFeature;
    layout.flank = flankProfileFor(layout.radiusMeters, exponent);
    layout.skeleton = mountainSkeletonFrom(
        layout.placement,
        SkeletonPlan{layout.radiusMeters, layout.featureSizeMeters * summitShoulderOfFeature,
                     layout.flank.broad, layout.flank},
        terrainSeedForLane(seed, 5));
    layout.outlineFeatureSizeMeters = layout.radiusMeters * std::numbers::pi / outlineLobeCount;
    layout.outlineWanderShare = irregularity * outlineWanderLimit;
    layout.heightMeters = static_cast<float>(api->parameterNumber(context, 1));
    layout.baseAltitudeMeters = static_cast<float>(api->parameterNumber(context, 9));
    layout.spurElongation = static_cast<float>(api->parameterNumber(context, 14));
    layout.detailRetention = 1.0f - static_cast<float>(api->parameterNumber(context, 4));
    layout.carveDepth = carveDepthFor(static_cast<float>(api->parameterNumber(context, 12)),
                                      layout.featureSizeMeters, layout.heightMeters);
    layout.warp = mountainWarpFor(layout.radiusMeters, irregularity, seed);
    layout.meander = gullyMeanderFor(layout.featureSizeMeters, seed);
    layout.outlineSeed = terrainSeedForLane(seed, 4);
    layout.detailSeed = terrainSeedForLane(seed, 3);
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
    "A single mountain: a summit crest with ridges running off it to the foot",
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
     TerrainParameterType_Float, 1600.0, 1.0, 100000.0, "", nullptr, 0},
    {"baseAltitude", "Base altitude", "The altitude the foot of the range sits at", "m",
     TerrainParameterType_Float, 0.0, -12000.0, 12000.0, "", nullptr, 0},
    {"spurElongation", "Spur length",
     "How far the fine detail is drawn out into gullies running down the flanks", "",
     TerrainParameterType_Float, 0.6, 0.0, 0.95, "", nullptr, 0}};

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
