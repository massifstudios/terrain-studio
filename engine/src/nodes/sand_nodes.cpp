#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <vector>

#include "node_support.h"
#include "shape_support.h"
#include "terrain/noise.h"

namespace terrain::nodes {
namespace {

struct GroundPoint {
    double eastingMeters = 0.0;
    double northingMeters = 0.0;
};

/* ------------------------------------------------------------ wind frame -- */

/**
 * The ground measured along and across a prevailing wind. `direction` is the
 * bearing the wind blows towards — 0 north, 90 east — so distance along the
 * wind grows downwind, and a dune's short face is the one at the far end of a
 * cycle.
 */
struct WindFrame {
    double downwindEast = 0.0;
    double downwindNorth = 1.0;

    double alongWindMeters(const GroundPoint& ground) const {
        return ground.eastingMeters * downwindEast + ground.northingMeters * downwindNorth;
    }

    double acrossWindMeters(const GroundPoint& ground) const {
        return ground.eastingMeters * downwindNorth - ground.northingMeters * downwindEast;
    }
};

WindFrame windFrameFor(double directionDegrees) {
    const double radians = directionDegrees * std::numbers::pi / 180.0;
    return WindFrame{std::sin(radians), std::cos(radians)};
}

/* --------------------------------------------------------- crest profile -- */

/// Fractional part in [0, 1), upwind of the origin as well as downwind.
float cyclePhase(double distanceMeters, double wavelengthMeters) {
    const double cycles = distanceMeters / wavelengthMeters;
    return static_cast<float>(cycles - std::floor(cycles));
}

/// 0 in the trough, 1 on the crest, with the crest standing `leeShare` of a
/// cycle before the end so the short, steep face is the downwind one.
float asymmetricRamp(float phase, float leeShare) {
    const float windwardShare = 1.0f - leeShare;
    if (phase < windwardShare) {
        return phase / windwardShare;
    }
    return (1.0f - phase) / leeShare;
}

/// How a crest is finished off. Rounding the ramp leaves the crest where it
/// stands, so a dune keeps its asymmetry however soft it is asked to be; a
/// power on the ramp pulls the mass off the flanks and leaves a knife edge.
struct CrestShaping {
    float softness = 0.0f;
    float sharpness = 0.0f;
};

constexpr float crestSharpnessRange = 3.0f;

float shapedCrest(float ramp, const CrestShaping& shaping) {
    const float clamped = std::clamp(ramp, 0.0f, 1.0f);
    const float rounded = clamped + (smoothStep(clamped) - clamped) * shaping.softness;
    return std::pow(rounded, 1.0f + shaping.sharpness * crestSharpnessRange);
}

/* ------------------------------------------------------------ phase warp -- */

/**
 * A displacement of where each cell reads the pattern from, from two
 * decorrelated octaves of fBm. It bends the crest lines rather than roughening
 * the surface, which is what keeps a dune field from reading as printed.
 *
 * The amount stays well under the feature size it is drawn over. Past about a
 * third the displacement's own gradient exceeds one and the field folds back
 * across itself, which shows up as crests crossing each other.
 */
struct PhaseWarp {
    std::uint64_t eastingSeed = 0;
    std::uint64_t northingSeed = 0;
    float featureSizeMeters = 1.0f;
    float amountMeters = 0.0f;
    noise::FractalOptions options{4, 0.5f, 2.0f};
};

GroundPoint warpedPoint(const PhaseWarp& warp, const GroundPoint& ground) {
    if (warp.amountMeters <= 0.0f) {
        return ground;
    }
    const float x = static_cast<float>(ground.eastingMeters) / warp.featureSizeMeters;
    const float y = static_cast<float>(ground.northingMeters) / warp.featureSizeMeters;
    const float alongEast = noise::fractalPerlin(x, y, warp.eastingSeed, warp.options);
    const float alongNorth = noise::fractalPerlin(x, y, warp.northingSeed, warp.options);
    return GroundPoint{ground.eastingMeters + alongEast * warp.amountMeters,
                       ground.northingMeters + alongNorth * warp.amountMeters};
}

/// Fractal noise in [-1, 1] read from a ground point at a given feature size.
float noiseAt(const GroundPoint& ground, float featureSizeMeters, std::uint64_t seed,
              const noise::FractalOptions& options) {
    const float x = static_cast<float>(ground.eastingMeters) / featureSizeMeters;
    const float y = static_cast<float>(ground.northingMeters) / featureSizeMeters;
    return noise::fractalPerlin(x, y, seed, options);
}

/// The seed parameter is a variation dial: it mixes with the per-node seed the
/// graph already derived, so rolling it changes one node and nothing else.
std::uint64_t seedFrom(const TerrainContextApi* api, TerrainEvaluationContext* context,
                       int parameterIndex) {
    return terrainSeedForLane(
        api->randomSeed(context),
        static_cast<std::uint64_t>(api->parameterNumber(context, parameterIndex)));
}

/* ------------------------------------------------------------------ Sand -- */

const TerrainPortDescriptor sandInputs[] = {
    {"In", "The surface the sand is laid over", TerrainPortType_Field,
     TerrainFieldSemantics_Heightfield, 0},
    {"Mask", "Scales the effect per cell", TerrainPortType_Field, TerrainFieldSemantics_Mask, 1}};

const TerrainPortDescriptor sandOutputs[] = {
    {"Out", "The surface with the sand laid over it", TerrainPortType_Field,
     TerrainFieldSemantics_Heightfield, 0}};

const TerrainParameterDescriptor sandParameters[] = {
    {"scale", "Scale", "Ground distance from one ripple crest to the next", "m",
     TerrainParameterType_Float, 40.0, 1.0, 5000.0, "", nullptr, 0},
    {"direction", "Direction", "Compass bearing the wind blows towards; 0 is north", "deg",
     TerrainParameterType_Float, 45.0, 0.0, 360.0, "", nullptr, 0},
    {"chaos", "Chaos", "How far the ripple lines bend and break up", "",
     TerrainParameterType_Float, 0.3, 0.0, 1.0, "", nullptr, 0},
    {"softness", "Softness", "0 leaves a sharp slip face, 1 rolls the ripples into waves", "",
     TerrainParameterType_Float, 0.4, 0.0, 1.0, "", nullptr, 0},
    {"height", "Height", "Rise from the trough of a ripple to its crest", "m",
     TerrainParameterType_Float, 1.5, 0.0, 500.0, "", nullptr, 0},
    {"warpByTerrain", "Warp by terrain", "Bends the ripple lines along the ground's contours", "",
     TerrainParameterType_Bool, 1.0, 0.0, 1.0, "", nullptr, 0},
    {"secondaryScale", "Secondary ripples",
     "Strength of a finer ripple set crossing the main one at a slight angle", "",
     TerrainParameterType_Float, 0.3, 0.0, 1.0, "", nullptr, 0},
    {"seed", "Seed", "Varies this node alone, on top of the graph seed", "",
     TerrainParameterType_Seed, 0.0, 0.0, 4294967295.0, "", nullptr, 0}};

/// How sharp a ripple's slip face is at each end of the softness dial. At 0.5
/// the crest sits in the middle of the cycle and the ripple is a plain wave.
constexpr float sandSharpLeeShare = 0.18f;

/// Warp amount as a share of the ripple wavelength, and the distance the warp
/// is drawn over as a multiple of it.
constexpr float sandChaosShare = 0.75f;
constexpr float sandChaosFeatureShare = 3.0f;

/// The finer set is a fifth of the wavelength and crosses the main one by this
/// much, which is what turns parallel lines into the braid blown sand carries.
constexpr double sandSecondaryWavelengthShare = 0.2;
constexpr double sandSecondaryTurnDegrees = 22.0;

/// Ripples thin out and gather again over this many wavelengths, and never
/// fade below this share of their full height.
constexpr float sandFadeFeatureShare = 12.0f;
constexpr float sandFadeFloor = 0.35f;

/// How far the ground's own altitude may push the ripple phase, in cycles.
constexpr double sandTerrainPhaseCycles = 1.5;

struct SandPattern {
    WindFrame wind;
    WindFrame secondaryWind;
    PhaseWarp warp;
    double wavelengthMeters = 40.0;
    double secondaryWavelengthMeters = 8.0;
    float secondaryStrength = 0.3f;
    float leeShare = sandSharpLeeShare;
    float softness = 0.4f;
    float fadeFeatureSizeMeters = 480.0f;
    std::uint64_t fadeSeed = 0;
};

/// One ripple set, as a rise of [-0.5, 0.5] about the mean surface.
float rippleAt(const WindFrame& wind, const GroundPoint& ground, double wavelengthMeters,
               double phaseOffsetMeters, float leeShare, float softness) {
    const float phase =
        cyclePhase(wind.alongWindMeters(ground) + phaseOffsetMeters, wavelengthMeters);
    const float ramp = asymmetricRamp(phase, leeShare);
    return ramp + (smoothStep(ramp) - ramp) * softness - 0.5f;
}

/// Where the ripples stand tallest, in [sandFadeFloor, 1].
float sandFadeAt(const SandPattern& pattern, const GroundPoint& ground) {
    const float swell = noiseAt(ground, pattern.fadeFeatureSizeMeters, pattern.fadeSeed,
                                noise::FractalOptions{3, 0.5f, 2.0f});
    return sandFadeFloor + (1.0f - sandFadeFloor) * smoothStep(0.5f + 0.5f * swell);
}

/// The two ripple sets summed and renormalised back into [-0.5, 0.5], then
/// faded so a sand field thins out and gathers again across the ground.
float sandPatternAt(const SandPattern& pattern, const GroundPoint& ground,
                    double phaseOffsetMeters) {
    const GroundPoint bent = warpedPoint(pattern.warp, ground);
    const float primary = rippleAt(pattern.wind, bent, pattern.wavelengthMeters, phaseOffsetMeters,
                                   pattern.leeShare, pattern.softness);
    const float secondary =
        rippleAt(pattern.secondaryWind, bent, pattern.secondaryWavelengthMeters, phaseOffsetMeters,
                 pattern.leeShare, pattern.softness);
    const float summed = primary + secondary * pattern.secondaryStrength;
    return summed / (1.0f + pattern.secondaryStrength) * sandFadeAt(pattern, ground);
}

SandPattern sandPatternFrom(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const double directionDegrees = api->parameterNumber(context, 1);
    const std::uint64_t seed = seedFrom(api, context, 7);
    SandPattern pattern;
    pattern.wavelengthMeters = std::max(api->parameterNumber(context, 0), 1.0);
    pattern.secondaryWavelengthMeters = pattern.wavelengthMeters * sandSecondaryWavelengthShare;
    pattern.wind = windFrameFor(directionDegrees);
    pattern.secondaryWind = windFrameFor(directionDegrees + sandSecondaryTurnDegrees);
    pattern.secondaryStrength = static_cast<float>(api->parameterNumber(context, 6));
    pattern.softness = static_cast<float>(api->parameterNumber(context, 3));
    pattern.leeShare = sandSharpLeeShare + pattern.softness * (0.5f - sandSharpLeeShare);
    pattern.warp.eastingSeed = terrainSeedForLane(seed, 1);
    pattern.warp.northingSeed = terrainSeedForLane(seed, 2);
    pattern.warp.featureSizeMeters =
        static_cast<float>(pattern.wavelengthMeters * sandChaosFeatureShare);
    pattern.warp.amountMeters = static_cast<float>(api->parameterNumber(context, 2))
                                * static_cast<float>(pattern.wavelengthMeters) * sandChaosShare;
    pattern.fadeSeed = terrainSeedForLane(seed, 3);
    pattern.fadeFeatureSizeMeters =
        static_cast<float>(pattern.wavelengthMeters) * sandFadeFeatureShare;
    return pattern;
}

struct SandLayer {
    SandPattern pattern;
    FieldRange groundRange;
    float heightMeters = 1.5f;
    bool followsTerrain = true;
};

/**
 * How far the ground the ripples lie on pushes their phase. Offsetting by the
 * altitude itself turns the crest lines along the ground's contours, which is
 * what a ripple field does on a hillside; a gradient would only tilt the lines
 * locally and would change with the cell size.
 */
double terrainPhaseOffsetMeters(const SandLayer& layer, float groundHeightMeters) {
    if (!layer.followsTerrain) {
        return 0.0;
    }
    const double acrossRange = layer.groundRange.normalise(groundHeightMeters) * 2.0 - 1.0;
    return acrossRange * layer.pattern.wavelengthMeters * sandTerrainPhaseCycles;
}

float sandReliefAt(const SandLayer& layer, const GroundPoint& ground, float groundHeightMeters) {
    const double phaseOffset = terrainPhaseOffsetMeters(layer, groundHeightMeters);
    return sandPatternAt(layer.pattern, ground, phaseOffset) * layer.heightMeters;
}

void writeSandRows(const WorldFrame& frame, const SandLayer& layer, const float* ground,
                   int firstRow, int lastRow, float* destination) {
    for (int row = firstRow; row < lastRow; ++row) {
        for (int column = 0; column < frame.size; ++column) {
            const std::size_t index = static_cast<std::size_t>(row) * frame.size + column;
            const GroundPoint point{frame.eastingAt(column), frame.northingAt(row)};
            destination[index] = ground[index] + sandReliefAt(layer, point, ground[index]);
        }
    }
}

void evaluateSand(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const WorldFrame frame = worldFrameFor(api, context);
    const std::size_t count = cellCount(api, context);
    const float* ground = api->inputField(context, 0);

    SandLayer layer;
    layer.pattern = sandPatternFrom(api, context);
    layer.heightMeters = static_cast<float>(api->parameterNumber(context, 4));
    layer.followsTerrain = api->parameterNumber(context, 5) >= 0.5;
    layer.groundRange = fieldRange(ground, count);

    std::vector<float> sanded(count);
    forEachRowRange(api, context, frame.size, [&](int firstRow, int lastRow) {
        writeSandRows(frame, layer, ground, firstRow, lastRow, sanded.data());
    });
    blendThroughMask(ground, sanded.data(), api->inputField(context, 1),
                     api->outputField(context, 0), count);
}

const TerrainNodeDescriptor sandDescriptor = {
    "Sand",
    "Filter",
    "Wind-blown ripples and small dunes laid over a surface already in place",
    1,
    sandInputs,
    2,
    sandOutputs,
    1,
    sandParameters,
    8,
    &evaluateSand};

/* --------------------------------------------------------------- dunes -- */

enum class DuneType { Transverse = 0, Barchan = 1, Star = 2, Linear = 3 };

const TerrainEnumOption duneTypes[] = {{"transverse", "Transverse", 0},
                                       {"barchan", "Barchan", 1},
                                       {"star", "Star", 2},
                                       {"linear", "Linear", 3}};

const TerrainEnumOption dunePresences[] = {
    {"none", "None", 0}, {"low", "Low", 1}, {"medium", "Medium", 2}, {"high", "High", 3}};

/// How far the crest lines wander, as a share of the dune spacing, at each
/// step of the Chaos dial.
float chaosShare(int presence) {
    constexpr float shares[] = {0.0f, 0.12f, 0.28f, 0.55f};
    return shares[std::clamp(presence, 0, 3)];
}

/// How much height the low-frequency swell may take away, at each step of the
/// Undulation dial.
float undulationShare(int presence) {
    constexpr float shares[] = {0.0f, 0.2f, 0.4f, 0.65f};
    return shares[std::clamp(presence, 0, 3)];
}

/* ------------------------------------------------------------- dune site -- */

/**
 * One dune's own place and shape, drawn from the grid cell it belongs to, so
 * a dune never moves when a neighbour changes and every worker that reaches
 * the cell draws the same one.
 *
 * Places are in units of the dune spacing, on the wind's own axes.
 */
struct DuneSite {
    double alongWind = 0.0;
    double acrossWind = 0.0;
    float relativeSize = 1.0f;
    float relativeHeight = 1.0f;
    float turnRadians = 0.0f;
    int armCount = 4;
};

std::uint64_t duneCellSeed(std::uint64_t seed, int alongCell, int acrossCell) {
    const auto along = static_cast<std::uint64_t>(static_cast<std::int64_t>(alongCell));
    const auto across = static_cast<std::uint64_t>(static_cast<std::int64_t>(acrossCell));
    return terrainSeedForLane(terrainSeedForLane(seed, along), across ^ 0xA5A5A5A5A5A5A5A5ull);
}

DuneSite duneSiteFor(std::uint64_t seed, int alongCell, int acrossCell) {
    std::uint64_t state = duneCellSeed(seed, alongCell, acrossCell);
    DuneSite site;
    site.alongWind = alongCell + 0.15 + 0.7 * terrainRandomUnitFloat(&state);
    site.acrossWind = acrossCell + 0.15 + 0.7 * terrainRandomUnitFloat(&state);
    site.relativeSize = 0.6f + 0.4f * terrainRandomUnitFloat(&state);
    site.relativeHeight = 0.6f + 0.4f * terrainRandomUnitFloat(&state);
    site.turnRadians = terrainRandomUnitFloat(&state) * 2.0f * std::numbers::pi_v<float>;
    site.armCount = 3 + static_cast<int>(terrainRandomUnitFloat(&state) * 3.0f);
    return site;
}

/**
 * The tallest of the nine dunes whose cells can reach this point. Reading only
 * the nearest site would cut a crescent off along the cell wall it happens to
 * cross, and the cut would run straight where nothing on a dune field does.
 */
template <typename ShapeAtSite>
float tallestNearbyDune(std::uint64_t seed, double alongCells, double acrossCells,
                        const ShapeAtSite& shapeAtSite) {
    const int alongCentre = static_cast<int>(std::floor(alongCells));
    const int acrossCentre = static_cast<int>(std::floor(acrossCells));
    float tallest = 0.0f;
    for (int alongStep = -1; alongStep <= 1; ++alongStep) {
        for (int acrossStep = -1; acrossStep <= 1; ++acrossStep) {
            const DuneSite site =
                duneSiteFor(seed, alongCentre + alongStep, acrossCentre + acrossStep);
            tallest = std::max(tallest, shapeAtSite(site));
        }
    }
    return tallest;
}

/* --------------------------------------------------------------- barchan -- */

/// Where the crest arcs, and how far the two faces run, as shares of the
/// spacing. The windward ramp is the long one; the slip face is short.
constexpr double barchanCrestRadius = 0.3;
constexpr double barchanWindwardRun = 0.8;
constexpr double barchanLeeRun = 0.28;

/// How far round from the upwind toe the crest survives. Past a right angle
/// the arc is trailing downwind, which is what makes the horns.
constexpr float barchanHornSpanDegrees = 100.0f;
constexpr float barchanHornFalloffDegrees = 45.0f;

/// Across the crest, as a ramp of [0, 1]: positive is the windward side,
/// outside the arc, negative the slip face inside it.
float barchanRamp(double acrossCrest) {
    if (acrossCrest >= 0.0) {
        return static_cast<float>(1.0 - acrossCrest / barchanWindwardRun);
    }
    return static_cast<float>(1.0 + acrossCrest / barchanLeeRun);
}

/// How far round the arc a point lies, measured from the upwind toe.
float barchanArcDegrees(double alongFromSite, double distance) {
    if (distance < 1.0e-9) {
        return 0.0f;
    }
    const double cosine = std::clamp(-alongFromSite / distance, -1.0, 1.0);
    return static_cast<float>(std::acos(cosine) * 180.0 / std::numbers::pi);
}

/**
 * A crescent: a rounded windward toe, a crest arcing across the wind, and two
 * horns trailing downwind. The slip face is the inside of the arc, where the
 * wind spills over the crest and drops its load.
 */
float barchanShapeAt(const DuneSite& site, double alongCells, double acrossCells,
                     const CrestShaping& shaping) {
    const double along = alongCells - site.alongWind;
    const double across = acrossCells - site.acrossWind;
    const double distance = std::hypot(along, across);
    const double crestRadius = barchanCrestRadius * site.relativeSize;
    const float ridge = shapedCrest(barchanRamp((distance - crestRadius) / crestRadius), shaping);
    const float horns = bandWeight(barchanArcDegrees(along, distance), 0.0f,
                                   barchanHornSpanDegrees, barchanHornFalloffDegrees);
    return ridge * horns * site.relativeHeight;
}

/* ------------------------------------------------------------------ star -- */

/// How far a star dune's arms reach, as a share of the spacing, and how much
/// of its height the central mass carries before the arms are added.
constexpr double starReach = 0.48;
constexpr float starCoreShare = 0.35f;

/// A central mass with three to five arms running out from it: what a dune
/// becomes where the wind turns through the year instead of holding one way.
float starShapeAt(const DuneSite& site, double alongCells, double acrossCells,
                  const CrestShaping& shaping) {
    const double along = alongCells - site.alongWind;
    const double across = acrossCells - site.acrossWind;
    const double reach = starReach * site.relativeSize;
    const double distance = std::hypot(along, across);
    if (distance >= reach) {
        return 0.0f;
    }
    const float mass = smoothStep(static_cast<float>(1.0 - distance / reach));
    const double angle = std::atan2(across, along) - site.turnRadians;
    const float arms = shapedCrest(
        static_cast<float>(std::abs(std::cos(site.armCount * angle * 0.5))), shaping);
    return mass * (starCoreShare + (1.0f - starCoreShare) * arms) * site.relativeHeight;
}

/* --------------------------------------------------------------- DuneSea -- */

const TerrainPortDescriptor duneSeaInputs[] = {
    {"Mask", "Scales the dune height per cell", TerrainPortType_Field,
     TerrainFieldSemantics_Mask, 1}};

const TerrainPortDescriptor duneSeaOutputs[] = {
    {"Out", "The dune field", TerrainPortType_Field, TerrainFieldSemantics_Heightfield, 0}};

const TerrainParameterDescriptor duneSeaParameters[] = {
    {"scale", "Scale", "Ground distance from one dune to the next", "m",
     TerrainParameterType_Float, 600.0, 10.0, 20000.0, "", nullptr, 0},
    {"duneType", "Dune type", "Which kind of dune the wind has built", "",
     TerrainParameterType_Enum, 0.0, 0.0, 3.0, "", duneTypes, 4},
    {"chaos", "Chaos", "How far the dunes wander off the wind's own lines", "",
     TerrainParameterType_Enum, 2.0, 0.0, 3.0, "", dunePresences, 4},
    {"undulation", "Undulation", "How much the field rises into banks and settles again", "",
     TerrainParameterType_Enum, 1.0, 0.0, 3.0, "", dunePresences, 4},
    {"direction", "Direction", "Compass bearing the wind blows towards; 0 is north", "deg",
     TerrainParameterType_Float, 45.0, 0.0, 360.0, "", nullptr, 0},
    {"height", "Height", "Rise from the interdune floor to the tallest crest", "m",
     TerrainParameterType_Float, 60.0, 0.0, 2000.0, "", nullptr, 0},
    {"softness", "Softness", "Rounds the crest off instead of leaving it kinked", "",
     TerrainParameterType_Float, 0.4, 0.0, 1.0, "", nullptr, 0},
    {"sharpness", "Sharpness", "Pulls the mass off the flanks and leaves a knife-edged crest", "",
     TerrainParameterType_Float, 0.5, 0.0, 1.0, "", nullptr, 0},
    {"floor", "Floor", "Altitude the sand between the dunes sits at", "m",
     TerrainParameterType_Float, 0.0, -12000.0, 12000.0, "", nullptr, 0},
    {"seed", "Seed", "Varies this node alone, on top of the graph seed", "",
     TerrainParameterType_Seed, 0.0, 0.0, 4294967295.0, "", nullptr, 0}};

/// The crest of a transverse dune sits this share of a cycle before the end of
/// it, which is what leaves the slip face on the lee side. A linear dune runs
/// with the wind rather than across it and is symmetric about its spine.
constexpr float transverseLeeShare = 0.18f;
constexpr float linearLeeShare = 0.5f;

/// The distance the crest lines are bent over, as a multiple of the spacing,
/// and the distance the height swell is drawn over.
constexpr float duneChaosFeatureShare = 2.0f;
constexpr float duneUndulationFeatureShare = 5.0f;

struct DuneField {
    DuneType type = DuneType::Transverse;
    WindFrame wind;
    PhaseWarp warp;
    CrestShaping shaping;
    double spacingMeters = 600.0;
    float heightMeters = 60.0f;
    float floorMeters = 0.0f;
    float undulationDepth = 0.0f;
    float undulationFeatureSizeMeters = 3000.0f;
    std::uint64_t undulationSeed = 0;
    std::uint64_t siteSeed = 0;
};

/// A run of parallel crests, transverse to the wind or along it, in [0, 1].
float wavefrontShapeAt(const DuneField& field, double distanceMeters, float leeShare) {
    const float phase = cyclePhase(distanceMeters, field.spacingMeters);
    return shapedCrest(asymmetricRamp(phase, leeShare), field.shaping);
}

/// Isolated dunes standing on a flat floor at jittered cellular sites.
float cellularShapeAt(const DuneField& field, const GroundPoint& ground) {
    const double alongCells = field.wind.alongWindMeters(ground) / field.spacingMeters;
    const double acrossCells = field.wind.acrossWindMeters(ground) / field.spacingMeters;
    if (field.type == DuneType::Barchan) {
        return tallestNearbyDune(field.siteSeed, alongCells, acrossCells,
                                 [&](const DuneSite& site) {
                                     return barchanShapeAt(site, alongCells, acrossCells,
                                                           field.shaping);
                                 });
    }
    return tallestNearbyDune(field.siteSeed, alongCells, acrossCells,
                             [&](const DuneSite& site) {
                                 return starShapeAt(site, alongCells, acrossCells, field.shaping);
                             });
}

float duneShapeAt(const DuneField& field, const GroundPoint& ground) {
    if (field.type == DuneType::Transverse) {
        return wavefrontShapeAt(field, field.wind.alongWindMeters(ground), transverseLeeShare);
    }
    if (field.type == DuneType::Linear) {
        return wavefrontShapeAt(field, field.wind.acrossWindMeters(ground), linearLeeShare);
    }
    return cellularShapeAt(field, ground);
}

/// A swell over several dune spacings, in [1 - depth, 1]: the field gathers
/// into banks of tall dunes and settles back to low ones between them.
float undulationAt(const DuneField& field, const GroundPoint& ground) {
    if (field.undulationDepth <= 0.0f) {
        return 1.0f;
    }
    const float swell = noiseAt(ground, field.undulationFeatureSizeMeters, field.undulationSeed,
                                noise::FractalOptions{3, 0.5f, 2.0f});
    return 1.0f - field.undulationDepth * (0.5f - 0.5f * swell);
}

float duneReliefAt(const DuneField& field, const GroundPoint& ground) {
    const GroundPoint bent = warpedPoint(field.warp, ground);
    return duneShapeAt(field, bent) * undulationAt(field, ground) * field.heightMeters;
}

DuneField duneFieldFrom(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const std::uint64_t seed = seedFrom(api, context, 9);
    DuneField field;
    field.spacingMeters = std::max(api->parameterNumber(context, 0), 1.0);
    field.type = static_cast<DuneType>(static_cast<int>(api->parameterNumber(context, 1)));
    field.wind = windFrameFor(api->parameterNumber(context, 4));
    field.heightMeters = static_cast<float>(api->parameterNumber(context, 5));
    field.shaping.softness = static_cast<float>(api->parameterNumber(context, 6));
    field.shaping.sharpness = static_cast<float>(api->parameterNumber(context, 7));
    field.floorMeters = static_cast<float>(api->parameterNumber(context, 8));
    field.warp.eastingSeed = terrainSeedForLane(seed, 1);
    field.warp.northingSeed = terrainSeedForLane(seed, 2);
    field.warp.featureSizeMeters = static_cast<float>(field.spacingMeters) * duneChaosFeatureShare;
    field.warp.amountMeters = chaosShare(static_cast<int>(api->parameterNumber(context, 2)))
                              * static_cast<float>(field.spacingMeters);
    field.undulationSeed = terrainSeedForLane(seed, 3);
    field.undulationDepth = undulationShare(static_cast<int>(api->parameterNumber(context, 3)));
    field.undulationFeatureSizeMeters =
        static_cast<float>(field.spacingMeters) * duneUndulationFeatureShare;
    field.siteSeed = terrainSeedForLane(seed, 4);
    return field;
}

void writeDuneRows(const WorldFrame& frame, const DuneField& field, int firstRow, int lastRow,
                   float* relief) {
    for (int row = firstRow; row < lastRow; ++row) {
        for (int column = 0; column < frame.size; ++column) {
            const std::size_t index = static_cast<std::size_t>(row) * frame.size + column;
            const GroundPoint ground{frame.eastingAt(column), frame.northingAt(row)};
            relief[index] = duneReliefAt(field, ground);
        }
    }
}

/// The dune relief laid on the interdune floor, held back where a mask asks
/// for less. An absent mask means full height, and nothing ever falls below
/// the floor.
void layOverFloor(const float* relief, const float* mask, float floorMeters, float* destination,
                  std::size_t count) {
    for (std::size_t index = 0; index < count; ++index) {
        float weight = 1.0f;
        if (mask != nullptr) {
            weight = std::clamp(mask[index], 0.0f, 1.0f);
        }
        destination[index] = floorMeters + std::max(relief[index], 0.0f) * weight;
    }
}

void evaluateDuneSea(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const WorldFrame frame = worldFrameFor(api, context);
    const DuneField field = duneFieldFrom(api, context);
    const std::size_t count = cellCount(api, context);

    std::vector<float> relief(count);
    forEachRowRange(api, context, frame.size, [&](int firstRow, int lastRow) {
        writeDuneRows(frame, field, firstRow, lastRow, relief.data());
    });
    layOverFloor(relief.data(), api->inputField(context, 0), field.floorMeters,
                 api->outputField(context, 0), count);
}

const TerrainNodeDescriptor duneSeaDescriptor = {
    "DuneSea",
    "Terrain",
    "Broad fields of dunes shaped by a prevailing wind, for deserts and sand seas",
    1,
    duneSeaInputs,
    1,
    duneSeaOutputs,
    1,
    duneSeaParameters,
    10,
    &evaluateDuneSea};

}  // namespace

void registerSandNodes(const TerrainRegistryApi& api, TerrainRegistry* registry) {
    api.registerNode(registry, &sandDescriptor);
    api.registerNode(registry, &duneSeaDescriptor);
}

}  // namespace terrain::nodes
