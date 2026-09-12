#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <vector>

#include "color_lookup_support.h"
#include "erosion_support.h"
#include "node_support.h"
#include "shape_support.h"

namespace terrain::nodes {
namespace {

/* ================================================================ stones == */

enum class StoneShape { Rounded = 0, Sharp = 1 };

const TerrainEnumOption stoneShapeOptions[] = {{"rounded", "Rounded", 0},
                                               {"sharp", "Sharp", 1}};

/**
 * One stone at rest: the cell it lies in, how wide it is and how far it
 * stands proud of the ground. Width and rise are in meters, so a stone is the
 * same size on the ground at any resolution.
 */
struct Stone {
    std::uint64_t id = 0;
    int row = 0;
    int column = 0;
    float diameterMeters = 1.0f;
    float riseMeters = 0.5f;
    float rotationDegrees = 0.0f;
};

bool isInside(int row, int column, int size) {
    return row >= 0 && column >= 0 && row < size && column < size;
}

std::size_t cellIndex(int row, int column, int size) {
    return static_cast<std::size_t>(row) * size + column;
}

/// What share of its bounding cylinder each profile fills — the integral of
/// the profile below, which is how a stone too small to draw knows what
/// volume it owes the cell it lands in.
double shapeFill(StoneShape shape) {
    if (shape == StoneShape::Sharp) {
        return 1.0 / 3.0;
    }
    return 2.0 / 3.0;
}

/// Five to eight faces, drawn from the stone's own id, so the same stone is
/// always cut the same way however the walk that found it was divided.
int facetCountFor(std::uint64_t id) {
    std::uint64_t state = id;
    return 5 + static_cast<int>(terrainRandomNext(&state) % 4);
}

ShapePlacement placementOf(const Stone& stone, const WorldFrame& frame) {
    const double radians = stone.rotationDegrees * std::numbers::pi / 180.0;
    ShapePlacement placement;
    placement.centreEastingMeters = frame.eastingAt(stone.column);
    placement.centreNorthingMeters = frame.northingAt(stone.row);
    placement.cosine = std::cos(radians);
    placement.sine = std::sin(radians);
    return placement;
}

/// 1 at the stone's crown, 0 at its edge and beyond. A rounded stone is the
/// cap of a sphere; a sharp one is a faceted cone whose flats are turned with
/// the stone, so a field of them does not read as one repeated shape.
float stoneProfile(StoneShape shape, const LocalPoint& local, double radiusMeters,
                   int facetCount) {
    if (shape == StoneShape::Sharp) {
        const double inradius = radiusMeters * std::cos(std::numbers::pi / facetCount);
        const double distance = polygonDistance(local, inradius, facetCount);
        return static_cast<float>(std::clamp(-distance / inradius, 0.0, 1.0));
    }
    const double fromCentre = local.distanceFromCentre() / radiusMeters;
    return static_cast<float>(std::sqrt(std::max(1.0 - fromCentre * fromCentre, 0.0)));
}

/**
 * A stone narrower than a cell has no shape left to draw, so it leaves its
 * volume instead, spread over the one cell it stands in. This is the only
 * place stones add rather than compete: gravel that fine is a depth, and a
 * hundred grains in a cell are deeper than one.
 */
void stampSubCellStone(const Stone& stone, StoneShape shape, const WorldFrame& frame,
                       std::vector<float>& bump) {
    const double radiusMeters = stone.diameterMeters * 0.5;
    const double volume = shapeFill(shape) * std::numbers::pi * radiusMeters * radiusMeters
                          * stone.riseMeters;
    const double cellArea = frame.cellSizeMeters * frame.cellSizeMeters;
    bump[cellIndex(stone.row, stone.column, frame.size)] +=
        static_cast<float>(volume / cellArea);
}

void stampStoneRow(const Stone& stone, StoneShape shape, const WorldFrame& frame,
                   const ShapePlacement& placement, int radiusInCells, int facetCount, int row,
                   std::vector<float>& bump) {
    const double radiusMeters = stone.diameterMeters * 0.5;
    const int firstColumn = std::max(stone.column - radiusInCells, 0);
    const int lastColumn = std::min(stone.column + radiusInCells, frame.size - 1);

    for (int column = firstColumn; column <= lastColumn; ++column) {
        const LocalPoint local =
            placement.localPoint(frame.eastingAt(column), frame.northingAt(row));
        const float rise =
            stone.riseMeters * stoneProfile(shape, local, radiusMeters, facetCount);
        const std::size_t index = cellIndex(row, column, frame.size);
        bump[index] = std::max(bump[index], rise);
    }
}

/// Lays one stone into the bump grid, tallest wins. Stones do not stack: two
/// that overlap are one rock pile, not a tower of twice the height.
void stampStone(const Stone& stone, StoneShape shape, const WorldFrame& frame,
                std::vector<float>& bump) {
    const double radiusMeters = stone.diameterMeters * 0.5;
    const int radiusInCells = static_cast<int>(radiusMeters / frame.cellSizeMeters);
    if (radiusInCells < 1) {
        stampSubCellStone(stone, shape, frame, bump);
        return;
    }

    const ShapePlacement placement = placementOf(stone, frame);
    const int facetCount = facetCountFor(stone.id);
    const int firstRow = std::max(stone.row - radiusInCells, 0);
    const int lastRow = std::min(stone.row + radiusInCells, frame.size - 1);
    for (int row = firstRow; row <= lastRow; ++row) {
        stampStoneRow(stone, shape, frame, placement, radiusInCells, facetCount, row, bump);
    }
}

/**
 * Every stone, in id order.
 *
 * The walk runs in parallel and each lane keeps its own finds, so the order
 * the stones arrive in is a scheduling accident. Sorting by the seed-derived
 * id makes it one fixed order again, which is what lets the stamp be a plain
 * serial pass over a shared grid instead of an atomic one.
 */
void stampStones(std::vector<Stone>& stones, StoneShape shape, const WorldFrame& frame,
                 std::vector<float>& bump) {
    std::sort(stones.begin(), stones.end(),
              [](const Stone& first, const Stone& second) { return first.id < second.id; });
    for (const Stone& stone : stones) {
        stampStone(stone, shape, frame, bump);
    }
}

std::vector<Stone> gatherLanes(const std::vector<std::vector<Stone>>& perLane) {
    std::vector<Stone> stones;
    for (const std::vector<Stone>& lane : perLane) {
        stones.insert(stones.end(), lane.begin(), lane.end());
    }
    return stones;
}

/// A depth in meters as a mask, against its own tallest cell: a mask is a
/// weight, and the meters are already in the surface output.
void writeNormalisedDepth(const std::vector<float>& depth, float* destination) {
    const float tallest = *std::max_element(depth.begin(), depth.end());
    const float scale = 1.0f / std::max(tallest, 1.0e-6f);
    for (std::size_t index = 0; index < depth.size(); ++index) {
        destination[index] = std::clamp(depth[index] * scale, 0.0f, 1.0f);
    }
}

void writeSurface(const float* input, const std::vector<float>& bump, bool isRendered,
                  float* destination) {
    if (!isRendered) {
        std::copy(input, input + bump.size(), destination);
        return;
    }
    for (std::size_t index = 0; index < bump.size(); ++index) {
        destination[index] = input[index] + bump[index];
    }
}

/* =============================================================== rolling == */

constexpr int neighbourRow[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
constexpr int neighbourColumn[8] = {-1, 0, 1, -1, 1, -1, 0, 1};

/// The cell a stone rolls into next, how far the ground falls to it and how
/// far that is in plan. `exists` is false in a hollow, where a stone stays.
struct DownhillStep {
    int row = 0;
    int column = 0;
    double dropMeters = 0.0;
    double runMeters = 1.0;
    bool exists = false;
};

double stepRunMeters(int direction, double cellSizeMeters) {
    if (neighbourRow[direction] != 0 && neighbourColumn[direction] != 0) {
        return cellSizeMeters * std::numbers::sqrt2;
    }
    return cellSizeMeters;
}

/// How far the ground falls to one neighbour. Negative outside the grid,
/// which no candidate can beat, so a stone never steps off the edge.
double dropToNeighbour(const float* height, const WorldFrame& frame, float here, int row,
                       int column) {
    if (!isInside(row, column, frame.size)) {
        return -1.0;
    }
    return here - height[cellIndex(row, column, frame.size)];
}

/// The steepest of the eight neighbours, measured as a drop over the ground
/// actually covered so a diagonal is not favoured for being longer.
DownhillStep steepestDescent(const float* height, const WorldFrame& frame, int row, int column) {
    const float here = height[cellIndex(row, column, frame.size)];
    DownhillStep best;
    double steepest = 0.0;

    for (int direction = 0; direction < 8; ++direction) {
        const int nextRow = row + neighbourRow[direction];
        const int nextColumn = column + neighbourColumn[direction];
        const double run = stepRunMeters(direction, frame.cellSizeMeters);
        const double drop = dropToNeighbour(height, frame, here, nextRow, nextColumn);
        if (drop / run <= steepest) {
            continue;
        }
        steepest = drop / run;
        best = DownhillStep{nextRow, nextColumn, drop, run, true};
    }
    return best;
}

float groundSlopeDegrees(const float* height, const WorldFrame& frame, int row, int column) {
    return slopeDegrees(
        sampleGradient(height, frame.size, row, column, frame.cellSizeMeters));
}

/* ================================================================ Debris == */

constexpr int maximumRockSteps = 256;
constexpr double gravityMetersPerSecondSquared = 9.81;
/// The slope at which friction of 1 holds a rock. Beyond it nothing loose
/// stays put, so it is the top of the rest-angle dial rather than a limit.
constexpr double steepestRestAngleDegrees = 45.0;
/// The slope at which ground is shedding rock as fast as it ever will, for
/// the natural distribution to measure a cliff against.
constexpr float fullySheddingSlopeDegrees = 45.0f;
constexpr std::int32_t debrisLaneCount = 64;

struct DebrisSettings {
    WorldFrame frame;
    std::int64_t rockCount = 0;
    float sizeMinimumMeters = 1.0f;
    float sizeMaximumMeters = 6.0f;
    double restAngleDegrees = 22.5;
    double frictionCoefficient = 0.414;
    double restitution = 0.3;
    double emissionDropMeters = 0.0;
    StoneShape shape = StoneShape::Rounded;
    bool isNatural = false;
    bool rendersStillRocks = true;
    std::uint64_t seed = 0;
};

/// The ground rocks come off, and how willing each cell is to shed one.
struct EmissionField {
    const float* height = nullptr;
    const float* mask = nullptr;
    FieldRange altitudes;
    WorldFrame frame;
    bool isNatural = false;
};

/// Steep, high ground sheds rock; a low flat plain does not.
float naturalWeight(const EmissionField& field, int row, int column) {
    const float slope = groundSlopeDegrees(field.height, field.frame, row, column);
    const float steepness = std::clamp(slope / fullySheddingSlopeDegrees, 0.0f, 1.0f);
    const float altitude =
        field.altitudes.normalise(field.height[cellIndex(row, column, field.frame.size)]);
    return steepness * (0.5f + 0.5f * altitude);
}

float emissionWeight(const EmissionField& field, int row, int column) {
    float weight = 1.0f;
    if (field.mask != nullptr) {
        weight = std::clamp(field.mask[cellIndex(row, column, field.frame.size)], 0.0f, 1.0f);
    }
    if (!field.isNatural) {
        return weight;
    }
    return weight * naturalWeight(field, row, column);
}

int pickIndex(std::uint64_t* state, int size) {
    return std::min(static_cast<int>(terrainRandomUnitFloat(state) * size), size - 1);
}

/// Where a rock comes off the ground, or nowhere when its tries all landed on
/// ground that sheds nothing.
struct EmissionCell {
    int row = 0;
    int column = 0;
    bool exists = false;
};

/// A rock gets this many tries at finding ground willing to shed it. The
/// budget is fixed, so how long a rock takes to place never depends on how
/// little of the terrain its mask left open.
constexpr int emissionAttempts = 8;

EmissionCell chooseEmissionCell(const EmissionField& field, std::uint64_t* state) {
    for (int attempt = 0; attempt < emissionAttempts; ++attempt) {
        const int row = pickIndex(state, field.frame.size);
        const int column = pickIndex(state, field.frame.size);
        if (terrainRandomUnitFloat(state) < emissionWeight(field, row, column)) {
            return EmissionCell{row, column, true};
        }
    }
    return EmissionCell{};
}

struct RollingRock {
    int row = 0;
    int column = 0;
    double speedSquared = 0.0;
    double lastSlopeDegrees = 0.0;
};

/// A rock is done once it has no speed left and the ground beneath it is
/// gentle enough to hold it. On anything steeper, gravity beats friction and
/// it starts moving again from rest.
bool hasSettled(const RollingRock& rock, const DebrisSettings& settings, const float* height) {
    if (rock.speedSquared > 0.0) {
        return false;
    }
    const float slope = groundSlopeDegrees(height, settings.frame, rock.row, rock.column);
    return slope <= settings.restAngleDegrees;
}

/**
 * One cell downhill, in energy rather than in forces: the drop pays the rock
 * and friction charges it for the ground it covered, so it keeps rolling
 * exactly while the slope is steeper than the rest angle — whatever the cell
 * size, since both terms are lengths in meters.
 *
 * Ground that flattens under a rock costs it speed too, and restitution is
 * how much of that survives the change.
 */
void rollOneStep(RollingRock& rock, const DownhillStep& step, const DebrisSettings& settings) {
    const double gained = step.dropMeters - settings.frictionCoefficient * step.runMeters;
    rock.speedSquared =
        std::max(rock.speedSquared + 2.0 * gravityMetersPerSecondSquared * gained, 0.0);

    const double slope = std::atan(step.dropMeters / step.runMeters) * 180.0 / std::numbers::pi;
    const double flattening = std::max(rock.lastSlopeDegrees - slope, 0.0) / 90.0;
    rock.speedSquared *= 1.0 - (1.0 - settings.restitution) * flattening;
    rock.lastSlopeDegrees = slope;
    rock.row = step.row;
    rock.column = step.column;
}

Stone stoneFor(std::uint64_t id, const RollingRock& rock, const DebrisSettings& settings,
               std::uint64_t* state) {
    const float sizeSpread = settings.sizeMaximumMeters - settings.sizeMinimumMeters;
    Stone stone;
    stone.id = id;
    stone.row = rock.row;
    stone.column = rock.column;
    stone.diameterMeters =
        settings.sizeMinimumMeters + terrainRandomUnitFloat(state) * sizeSpread;
    stone.riseMeters = stone.diameterMeters * 0.5f;
    stone.rotationDegrees = terrainRandomUnitFloat(state) * 360.0f;
    return stone;
}

struct RestingRock {
    Stone stone;
    bool exists = false;
};

/**
 * One rock, from the ground it came off to the ground it stopped on.
 *
 * Everything it does comes out of its own id, so a rock's fate is the same
 * whichever lane ran it and whichever rocks ran beside it. The step budget is
 * fixed: a rock still moving when it runs out stops where it stands, which is
 * the only thing a deterministic simulation may do with one.
 */
RestingRock rollRock(std::uint64_t id, const DebrisSettings& settings,
                     const EmissionField& field) {
    std::uint64_t state = id;
    const EmissionCell start = chooseEmissionCell(field, &state);
    if (!start.exists) {
        return RestingRock{};
    }

    RollingRock rock{start.row, start.column,
                     2.0 * gravityMetersPerSecondSquared * settings.emissionDropMeters, 0.0};
    for (int step = 0; step < maximumRockSteps; ++step) {
        if (hasSettled(rock, settings, field.height)) {
            break;
        }
        const DownhillStep next =
            steepestDescent(field.height, settings.frame, rock.row, rock.column);
        if (!next.exists) {
            break;
        }
        rollOneStep(rock, next, settings);
    }
    return RestingRock{stoneFor(id, rock, settings, &state), true};
}

struct DebrisWork {
    const DebrisSettings* settings = nullptr;
    const EmissionField* field = nullptr;
    std::vector<std::vector<Stone>>* perLane = nullptr;
};

void rollRockLane(void* userData, std::int32_t laneIndex, std::int32_t laneCount) {
    const DebrisWork& work = *static_cast<const DebrisWork*>(userData);
    const std::int64_t count = work.settings->rockCount;
    const std::int64_t first = count * laneIndex / laneCount;
    const std::int64_t last = count * (laneIndex + 1) / laneCount;
    std::vector<Stone>& resting = (*work.perLane)[laneIndex];

    for (std::int64_t rock = first; rock < last; ++rock) {
        const std::uint64_t id =
            terrainSeedForLane(work.settings->seed, static_cast<std::uint64_t>(rock));
        const RestingRock settled = rollRock(id, *work.settings, *work.field);
        if (settled.exists) {
            resting.push_back(settled.stone);
        }
    }
}

void emitStones(const TerrainContextApi* api, TerrainEvaluationContext* context,
                const std::vector<Stone>& stones, const float* ground, const WorldFrame& frame) {
    for (const Stone& stone : stones) {
        TerrainPoint point;
        point.eastingMeters = static_cast<float>(frame.eastingAt(stone.column));
        point.northingMeters = static_cast<float>(frame.northingAt(stone.row));
        point.altitudeMeters = ground[cellIndex(stone.row, stone.column, frame.size)];
        point.rotationDegrees = stone.rotationDegrees;
        point.scale = stone.diameterMeters;
        point.id = stone.id;
        api->emitPoint(context, 2, &point);
    }
}

const TerrainPortDescriptor debrisInputs[] = {
    {"In", "The surface rocks come off and roll over", TerrainPortType_Field,
     TerrainFieldSemantics_Heightfield, 0},
    {"Mask", "Where rocks are shed, from 0 to 1", TerrainPortType_Field,
     TerrainFieldSemantics_Mask, 1}};

const TerrainPortDescriptor debrisOutputs[] = {
    {"Out", "The surface with the resting rocks on it", TerrainPortType_Field,
     TerrainFieldSemantics_Heightfield, 0},
    {"Rocks", "Where the rocks came to rest", TerrainPortType_Field,
     TerrainFieldSemantics_Mask, 0},
    {"Points", "One instance per resting rock, scaled to its width",
     TerrainPortType_Points, TerrainFieldSemantics_Heightfield, 0}};

const TerrainEnumOption debrisDistributionOptions[] = {{"uniform", "Uniform", 0},
                                                       {"natural", "Natural", 1}};

const TerrainParameterDescriptor debrisParameters[] = {
    {"debrisAmount", "Rocks", "How many rocks are shed over the whole surface", "",
     TerrainParameterType_Int, 2000.0, 0.0, 200000.0, "", nullptr, 0},
    {"amountMultiplier", "Amount multiplier", "Scales the rock count without touching anything "
     "else", "", TerrainParameterType_Float, 1.0, 0.0, 4.0, "", nullptr, 0},
    {"sizeMin", "Smallest", "How wide the smallest rock is", "m", TerrainParameterType_Float,
     1.0, 0.05, 500.0, "", nullptr, 0},
    {"sizeMax", "Largest", "How wide the largest rock is", "m", TerrainParameterType_Float, 6.0,
     0.05, 500.0, "", nullptr, 0},
    {"scale", "Scale", "Multiplies both rock sizes at once", "", TerrainParameterType_Float, 1.0,
     0.1, 10.0, "", nullptr, 0},
    {"friction", "Friction", "How readily a rock slides. It rests on ground gentler than "
     "friction times 45 degrees, so more friction stops rocks sooner and piles them steeper", "",
     TerrainParameterType_Float, 0.5, 0.0, 1.0, "", nullptr, 0},
    {"restitution", "Restitution", "How much speed survives where the slope flattens. More "
     "keeps rocks travelling out onto the flat", "", TerrainParameterType_Float, 0.3, 0.0, 1.0,
     "", nullptr, 0},
    {"height", "Drop", "An extra fall at the moment a rock is shed, which starts it moving", "m",
     TerrainParameterType_Float, 0.0, 0.0, 2000.0, "", nullptr, 0},
    {"shape", "Shape", "Rounded rocks are weathered boulders, sharp ones fresh fracture", "",
     TerrainParameterType_Enum, 0.0, 0.0, 1.0, "", stoneShapeOptions, 2},
    {"distribution", "Distribution", "Uniform sheds rock evenly; natural sheds it from steep "
     "high ground, so it comes off the cliffs", "", TerrainParameterType_Enum, 0.0, 0.0, 1.0, "",
     debrisDistributionOptions, 2},
    {"renderStillRocks", "Render still rocks", "Stamps the resting rocks into the surface. Turn "
     "it off to take only the mask and the points", "", TerrainParameterType_Bool, 1.0, 0.0, 1.0,
     "", nullptr, 0},
    {"seed", "Seed", "Varies this node alone, on top of the graph seed", "",
     TerrainParameterType_Seed, 0.0, 0.0, 4294967295.0, "", nullptr, 0}};

DebrisSettings debrisSettingsFor(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const double firstSize = api->parameterNumber(context, 2);
    const double secondSize = api->parameterNumber(context, 3);
    const double smallest = std::min(firstSize, secondSize);
    const double largest = std::max(firstSize, secondSize);
    const double sizeScale = api->parameterNumber(context, 4);
    const double friction = api->parameterNumber(context, 5);

    DebrisSettings settings;
    settings.frame = worldFrameFor(api, context);
    settings.rockCount = static_cast<std::int64_t>(
        std::llround(api->parameterNumber(context, 0) * api->parameterNumber(context, 1)));
    settings.sizeMinimumMeters = static_cast<float>(smallest * sizeScale);
    settings.sizeMaximumMeters = static_cast<float>(largest * sizeScale);
    settings.restAngleDegrees = friction * steepestRestAngleDegrees;
    settings.frictionCoefficient =
        std::tan(settings.restAngleDegrees * std::numbers::pi / 180.0);
    settings.restitution = api->parameterNumber(context, 6);
    settings.emissionDropMeters = api->parameterNumber(context, 7);
    settings.shape = static_cast<StoneShape>(static_cast<int>(api->parameterNumber(context, 8)));
    settings.isNatural = api->parameterNumber(context, 9) != 0.0;
    settings.rendersStillRocks = api->parameterNumber(context, 10) != 0.0;
    settings.seed = terrainSeedForLane(
        api->randomSeed(context), static_cast<std::uint64_t>(api->parameterNumber(context, 11)));
    return settings;
}

/**
 * Loose rock shed over the terrain, rolled downhill until it stops, then laid
 * into the surface where it stopped.
 *
 * The walk runs in parallel and the stamp does not. A rock's whole life comes
 * out of its own seed and reads a surface nobody is writing to, so the lanes
 * never meet; the stones they find are then sorted by id and stamped in that
 * one order, which costs a sort and saves an atomic in every cell of every
 * footprint.
 */
void evaluateDebris(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const std::size_t count = cellCount(api, context);
    const float* input = api->inputField(context, 0);
    const DebrisSettings settings = debrisSettingsFor(api, context);
    const EmissionField field{input, api->inputField(context, 1), fieldRange(input, count),
                              settings.frame, settings.isNatural};

    std::vector<std::vector<Stone>> perLane(debrisLaneCount);
    DebrisWork work{&settings, &field, &perLane};
    api->runLanes(context, debrisLaneCount, &rollRockLane, &work);
    api->reportProgress(context, 0.75f);

    std::vector<Stone> stones = gatherLanes(perLane);
    std::vector<float> bump(count, 0.0f);
    stampStones(stones, settings.shape, settings.frame, bump);

    writeSurface(input, bump, settings.rendersStillRocks, api->outputField(context, 0));
    writeNormalisedDepth(bump, api->outputField(context, 1));
    emitStones(api, context, stones, input, settings.frame);
    api->reportProgress(context, 1.0f);
}

const TerrainNodeDescriptor debrisDescriptor = {
    "Debris",
    "Simulate",
    "Sheds loose rock over the terrain and lets it roll downhill until it rests",
    1,
    debrisInputs,
    2,
    debrisOutputs,
    3,
    debrisParameters,
    12,
    &evaluateDebris};

/* ================================================================= Scree == */

constexpr std::int32_t screeLaneCount = 32;
/// Stones from one cell, at most. A density fine enough to ask for more than
/// this is asking for a depth of gravel rather than for stones, and drawing
/// them one at a time would cost time the result does not show.
constexpr int maximumStonesPerCell = 16;
constexpr int maximumScreeSteps = 512;

struct ScreeSettings {
    WorldFrame frame;
    float stoneDiameterMeters = 2.0f;
    float riseMeters = 0.8f;
    double stonesPerSquareKilometre = 400.0;
    int travelSteps = 1;
    float edge = 0.6f;
    float sourceAngleDegrees = 35.0f;
    std::uint64_t seed = 0;
};

double expectedStonesPerCell(const ScreeSettings& settings) {
    const double cellAreaSquareKilometres =
        settings.frame.cellSizeMeters * settings.frame.cellSizeMeters / 1.0e6;
    return settings.stonesPerSquareKilometre * cellAreaSquareKilometres;
}

/// A fractional expectation as a whole count: the fraction is the chance of
/// one more stone, so a density below one stone per cell still lands the
/// right number over the map.
int stoneCountIn(double expected, std::uint64_t* state) {
    const double whole = std::floor(expected);
    int count = static_cast<int>(std::min(whole, static_cast<double>(maximumStonesPerCell)));
    if (terrainRandomUnitFloat(state) < expected - whole) {
        count += 1;
    }
    return std::min(count, maximumStonesPerCell);
}

/**
 * One stone, from the face that shed it to where it came to rest.
 *
 * It rolls while the ground under it is steeper than the angle that sheds
 * stones, so it stops at the break in slope — the foot of the face — which is
 * where scree actually gathers. `edge` is its willingness to keep going: at 0
 * it drops on the face it came off, at 1 it runs the whole way down.
 */
Stone slideStone(std::uint64_t id, int row, int column, const ScreeSettings& settings,
                 const float* height) {
    std::uint64_t state = id;
    int restingRow = row;
    int restingColumn = column;

    for (int step = 0; step < settings.travelSteps; ++step) {
        if (groundSlopeDegrees(height, settings.frame, restingRow, restingColumn)
            < settings.sourceAngleDegrees) {
            break;
        }
        if (terrainRandomUnitFloat(&state) >= settings.edge) {
            break;
        }
        const DownhillStep next =
            steepestDescent(height, settings.frame, restingRow, restingColumn);
        if (!next.exists) {
            break;
        }
        restingRow = next.row;
        restingColumn = next.column;
    }

    Stone stone;
    stone.id = id;
    stone.row = restingRow;
    stone.column = restingColumn;
    // A field of identically wide stones reads as a pattern, so each one is
    // drawn within a third of the size asked for.
    stone.diameterMeters =
        settings.stoneDiameterMeters * (0.7f + 0.6f * terrainRandomUnitFloat(&state));
    stone.riseMeters = settings.riseMeters;
    stone.rotationDegrees = terrainRandomUnitFloat(&state) * 360.0f;
    return stone;
}

struct ScreeWork {
    const ScreeSettings* settings = nullptr;
    const float* height = nullptr;
    const float* mask = nullptr;
    std::vector<std::vector<Stone>>* perLane = nullptr;
};

float sourceWeight(const ScreeWork& work, int row, int column) {
    const ScreeSettings& settings = *work.settings;
    const float slope = groundSlopeDegrees(work.height, settings.frame, row, column);
    if (slope < settings.sourceAngleDegrees) {
        return 0.0f;
    }
    if (work.mask == nullptr) {
        return 1.0f;
    }
    return std::clamp(work.mask[cellIndex(row, column, settings.frame.size)], 0.0f, 1.0f);
}

void shedCell(int row, int column, const ScreeWork& work, std::vector<Stone>& resting) {
    const ScreeSettings& settings = *work.settings;
    const float weight = sourceWeight(work, row, column);
    if (weight <= 0.0f) {
        return;
    }

    const std::uint64_t cellSeed =
        terrainSeedForLane(settings.seed, cellIndex(row, column, settings.frame.size));
    std::uint64_t state = cellSeed;
    const int stoneCount = stoneCountIn(expectedStonesPerCell(settings) * weight, &state);
    for (int stone = 0; stone < stoneCount; ++stone) {
        const std::uint64_t id = terrainSeedForLane(cellSeed, static_cast<std::uint64_t>(stone));
        resting.push_back(slideStone(id, row, column, settings, work.height));
    }
}

void shedRow(int row, const ScreeWork& work, std::vector<Stone>& resting) {
    for (int column = 0; column < work.settings->frame.size; ++column) {
        shedCell(row, column, work, resting);
    }
}

void screeLane(void* userData, std::int32_t laneIndex, std::int32_t laneCount) {
    const ScreeWork& work = *static_cast<const ScreeWork*>(userData);
    const int size = work.settings->frame.size;
    const int firstRow = size * laneIndex / laneCount;
    const int lastRow = size * (laneIndex + 1) / laneCount;
    std::vector<Stone>& resting = (*work.perLane)[laneIndex];

    for (int row = firstRow; row < lastRow; ++row) {
        shedRow(row, work, resting);
    }
}

const TerrainPortDescriptor screeInputs[] = {
    {"In", "The surface stones break off and gather on", TerrainPortType_Field,
     TerrainFieldSemantics_Heightfield, 0},
    {"Mask", "Where stones may break off, from 0 to 1", TerrainPortType_Field,
     TerrainFieldSemantics_Mask, 1}};

const TerrainPortDescriptor screeOutputs[] = {
    {"Out", "The surface with scree on it", TerrainPortType_Field,
     TerrainFieldSemantics_Heightfield, 0},
    {"Scree", "Where the scree lies", TerrainPortType_Field, TerrainFieldSemantics_Mask, 0}};

const TerrainParameterDescriptor screeParameters[] = {
    {"scale", "Stone size", "How wide a scree stone is, on average", "m",
     TerrainParameterType_Float, 2.0, 0.05, 200.0, "", nullptr, 0},
    {"height", "Stone height", "How far a stone stands above the ground it rests on", "m",
     TerrainParameterType_Float, 0.8, 0.0, 100.0, "", nullptr, 0},
    {"density", "Density", "How many stones break off each square kilometre of steep ground", "",
     TerrainParameterType_Int, 400.0, 0.0, 2000000.0, "", nullptr, 0},
    {"spread", "Spread", "How far a stone may travel from the face it came off", "m",
     TerrainParameterType_Float, 80.0, 0.0, 20000.0, "", nullptr, 0},
    {"edge", "Edge gathering", "How strongly stones gather at the foot of steep ground. At 0 "
     "they lie where they broke off; at 1 they run down to where the slope gives out", "",
     TerrainParameterType_Float, 0.6, 0.0, 1.0, "", nullptr, 0},
    {"sourceAngle", "Source angle", "Ground steeper than this sheds stones, and holds none that "
     "reach it", "deg", TerrainParameterType_Float, 35.0, 0.0, 89.0, "", nullptr, 0},
    {"seed", "Seed", "Varies this node alone, on top of the graph seed", "",
     TerrainParameterType_Seed, 0.0, 0.0, 4294967295.0, "", nullptr, 0}};

/// How many cell steps cover a distance, never fewer than one and never more
/// than the budget a stone is allowed.
int stepsToTravel(double distanceMeters, double cellSizeMeters) {
    const long steps = std::lround(distanceMeters / cellSizeMeters);
    return static_cast<int>(std::clamp<long>(steps, 1, maximumScreeSteps));
}

ScreeSettings screeSettingsFor(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    ScreeSettings settings;
    settings.frame = worldFrameFor(api, context);
    settings.stoneDiameterMeters = static_cast<float>(api->parameterNumber(context, 0));
    settings.riseMeters = static_cast<float>(api->parameterNumber(context, 1));
    settings.stonesPerSquareKilometre = api->parameterNumber(context, 2);
    settings.travelSteps =
        stepsToTravel(api->parameterNumber(context, 3), settings.frame.cellSizeMeters);
    settings.edge = static_cast<float>(api->parameterNumber(context, 4));
    settings.sourceAngleDegrees = static_cast<float>(api->parameterNumber(context, 5));
    settings.seed = terrainSeedForLane(
        api->randomSeed(context), static_cast<std::uint64_t>(api->parameterNumber(context, 6)));
    return settings;
}

/**
 * The fragments a steep face sheds, gathered where the slope gives out.
 *
 * Every cell steep enough to shed stone offers its own candidates, hashed
 * from the cell and the seed rather than counted off a running total, so a
 * lane needs to know nothing about the lanes beside it. The stones are stamped
 * in id order once every lane has finished, exactly as Debris does.
 */
void evaluateScree(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const std::size_t count = cellCount(api, context);
    const float* input = api->inputField(context, 0);
    const ScreeSettings settings = screeSettingsFor(api, context);

    std::vector<std::vector<Stone>> perLane(screeLaneCount);
    ScreeWork work{&settings, input, api->inputField(context, 1), &perLane};
    api->runLanes(context, screeLaneCount, &screeLane, &work);
    api->reportProgress(context, 0.75f);

    std::vector<Stone> stones = gatherLanes(perLane);
    std::vector<float> bump(count, 0.0f);
    stampStones(stones, StoneShape::Sharp, settings.frame, bump);

    writeSurface(input, bump, true, api->outputField(context, 0));
    writeNormalisedDepth(bump, api->outputField(context, 1));
    api->reportProgress(context, 1.0f);
}

const TerrainNodeDescriptor screeDescriptor = {
    "Scree",
    "Simulate",
    "Breaks stone off steep faces and piles it where the slope gives out",
    1,
    screeInputs,
    2,
    screeOutputs,
    2,
    screeParameters,
    7,
    &evaluateScree};

/* ============================================================= Sediments == */

enum class SedimentType { Fine = 0, Bulky = 1, Classic = 2 };

const TerrainEnumOption sedimentTypeOptions[] = {
    {"fine", "Fine", 0}, {"bulky", "Bulky", 1}, {"classic", "Classic", 2}};

const TerrainEnumOption sedimentStyleOptions[] = {{"gradual", "Gradual", 0},
                                                  {"allAtOnce", "All at once", 1}};

constexpr std::uint64_t grainSalt = 0x9E3779B185EBCA87ull;
/// How far a grainy layer's depth varies about its average, as a share of it.
constexpr float grainVariation = 0.7f;

/// What the grain of a sediment does to the two numbers that describe it: how
/// steeply it rests, and how far it runs before it does.
struct SedimentGrade {
    double settleAngleScale = 1.0;
    double travelScale = 1.0;
};

SedimentGrade gradeFor(SedimentType type) {
    if (type == SedimentType::Fine) {
        return SedimentGrade{0.5, 1.6};
    }
    if (type == SedimentType::Bulky) {
        return SedimentGrade{1.6, 0.6};
    }
    return SedimentGrade{1.0, 1.0};
}

struct SedimentSettings {
    int passes = 20;
    float depthMeters = 20.0f;
    double settleAngleDegrees = 20.0;
    double travelPerPassMeters = 10.0;
    bool isAllAtOnce = false;
    bool isGrainy = false;
    std::uint64_t seed = 0;
};

/**
 * A per-cell multiplier on the released depth, averaging exactly one.
 *
 * The average is normalised rather than assumed, so a grainy layer weighs the
 * same as a flat one to the last cell — which is what lets the whole node be
 * checked against the depth it was asked to release.
 */
std::vector<float> grainField(const SedimentSettings& settings, std::size_t count) {
    std::vector<float> grain(count, 1.0f);
    if (!settings.isGrainy) {
        return grain;
    }

    double total = 0.0;
    for (std::size_t index = 0; index < count; ++index) {
        const float noise = cellNoise(settings.seed, index, grainSalt);
        grain[index] = 1.0f - grainVariation + 2.0f * grainVariation * noise;
        total += grain[index];
    }
    const float scale = static_cast<float>(count / std::max(total, 1.0e-9));
    for (float& value : grain) {
        value *= scale;
    }
    return grain;
}

/// Drops sediment onto the ground. It arrives in the layer and on the surface
/// at once, because the surface is what the slopes are read from.
void releaseLayer(std::vector<float>& surface, std::vector<float>& layer,
                  const std::vector<float>& grain, float depthMeters) {
    for (std::size_t index = 0; index < surface.size(); ++index) {
        const float released = depthMeters * grain[index];
        surface[index] += released;
        layer[index] += released;
    }
}

const TerrainPortDescriptor sedimentInputs[] = {
    {"In", "The surface the layer settles over", TerrainPortType_Field,
     TerrainFieldSemantics_Heightfield, 0},
    {"Mask", "Scales the effect per cell", TerrainPortType_Field, TerrainFieldSemantics_Mask, 1}};

const TerrainPortDescriptor sedimentOutputs[] = {
    {"Out", "The surface with the layer on it", TerrainPortType_Field,
     TerrainFieldSemantics_Heightfield, 0},
    {"Sediment", "How deep the layer ended up, at its own scale", TerrainPortType_Field,
     TerrainFieldSemantics_Mask, 0}};

const TerrainParameterDescriptor sedimentParameters[] = {
    {"type", "Type", "Fine material rests at a gentle angle and travels far, bulky rests steeper "
     "and stays put, classic takes the angle and travel as given", "",
     TerrainParameterType_Enum, 2.0, 0.0, 2.0, "", sedimentTypeOptions, 3},
    {"passes", "Passes", "How many helpings the layer arrives in, and so how evenly it builds up",
     "", TerrainParameterType_Int, 20.0, 1.0, 500.0, "", nullptr, 0},
    {"depth", "Depth", "Total depth of sediment released over the whole surface", "m",
     TerrainParameterType_Float, 20.0, 0.0, 2000.0, "", nullptr, 0},
    {"angle", "Settle angle", "The steepest slope the layer will rest on, before the type "
     "adjusts it", "deg", TerrainParameterType_Float, 20.0, 0.0, 89.0, "", nullptr, 0},
    {"travel", "Travel", "How far the layer slides before it comes to rest", "m",
     TerrainParameterType_Float, 120.0, 1.0, 20000.0, "", nullptr, 0},
    {"style", "Style", "Gradual releases an even helping each pass; all at once drops the whole "
     "layer and then lets it slide", "", TerrainParameterType_Enum, 0.0, 0.0, 1.0, "",
     sedimentStyleOptions, 2},
    {"grainyDeposits", "Grainy deposits", "Varies the depth released cell by cell, so the layer "
     "is grainy rather than flat", "", TerrainParameterType_Bool, 0.0, 0.0, 1.0, "", nullptr, 0},
    {"seed", "Seed", "Varies this node alone, on top of the graph seed", "",
     TerrainParameterType_Seed, 0.0, 0.0, 4294967295.0, "", nullptr, 0}};

SedimentSettings sedimentSettingsFor(const TerrainContextApi* api,
                                     TerrainEvaluationContext* context) {
    const auto type = static_cast<SedimentType>(static_cast<int>(api->parameterNumber(context, 0)));
    const SedimentGrade grade = gradeFor(type);

    SedimentSettings settings;
    settings.passes = static_cast<int>(api->parameterNumber(context, 1));
    settings.depthMeters = static_cast<float>(api->parameterNumber(context, 2));
    settings.settleAngleDegrees =
        std::clamp(api->parameterNumber(context, 3) * grade.settleAngleScale, 0.0, 89.0);
    // Sliding is diffusion, so the distances of the passes add in quadrature:
    // this is the per-pass travel whose passes together cover the whole one,
    // which keeps the total work the same whatever the pass count.
    settings.travelPerPassMeters = api->parameterNumber(context, 4) * grade.travelScale
                                   / std::sqrt(static_cast<double>(settings.passes));
    settings.isAllAtOnce = api->parameterNumber(context, 5) != 0.0;
    settings.isGrainy = api->parameterNumber(context, 6) != 0.0;
    settings.seed = terrainSeedForLane(
        api->randomSeed(context), static_cast<std::uint64_t>(api->parameterNumber(context, 7)));
    return settings;
}

LayerSlump slumpFor(const SedimentSettings& settings) {
    LayerSlump slump;
    slump.settleAngleDegrees = settings.settleAngleDegrees;
    slump.travelMeters = settings.travelPerPassMeters;
    slump.rate = 0.5f;
    return slump;
}

/**
 * A thick layer of loose material laid over the ground and left to settle —
 * sand, snow, ash, whatever the settle angle and the colour say it is.
 *
 * Only the layer moves; the rock beneath it stays where it is, so the layer
 * slides off anything too steep to hold it and gathers in everything that
 * curves inwards. Nothing is lost on the way: what comes out of the node is
 * exactly the depth that was released into it.
 */
void evaluateSediments(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const std::size_t count = cellCount(api, context);
    const float* input = api->inputField(context, 0);
    const SedimentSettings settings = sedimentSettingsFor(api, context);
    const std::vector<float> grain = grainField(settings, count);
    const LayerSlump slump = slumpFor(settings);

    std::vector<float> surface(input, input + count);
    std::vector<float> layer(count, 0.0f);
    if (settings.isAllAtOnce) {
        releaseLayer(surface, layer, grain, settings.depthMeters);
    }

    const float depthPerPass = settings.depthMeters / settings.passes;
    for (int pass = 0; pass < settings.passes; ++pass) {
        if (!settings.isAllAtOnce) {
            releaseLayer(surface, layer, grain, depthPerPass);
        }
        slumpLayer(api, context, surface.data(), layer.data(), slump);
        api->reportProgress(context, static_cast<float>(pass + 1) / settings.passes);
    }

    blendThroughMask(input, surface.data(), api->inputField(context, 1),
                     api->outputField(context, 0), count);
    writeNormalisedDepth(layer, api->outputField(context, 1));
}

const TerrainNodeDescriptor sedimentsDescriptor = {
    "Sediments",
    "Simulate",
    "Lays a thick layer of loose material over the ground and lets it settle",
    1,
    sedimentInputs,
    2,
    sedimentOutputs,
    2,
    sedimentParameters,
    8,
    &evaluateSediments};

}  // namespace

void registerDebrisNodes(const TerrainRegistryApi& api, TerrainRegistry* registry) {
    api.registerNode(registry, &debrisDescriptor);
    api.registerNode(registry, &screeDescriptor);
    api.registerNode(registry, &sedimentsDescriptor);
}

}  // namespace terrain::nodes
