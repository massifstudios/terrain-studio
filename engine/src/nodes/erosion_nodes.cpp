#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <vector>

#include "denormal_mode.h"
#include "erosion_backend.h"
#include "erosion_support.h"
#include "node_support.h"

namespace terrain::nodes {
namespace {

/// Lets a node hand the engine's cancellation and progress to a backend that
/// knows nothing about the SDK.
gpu::EvaluationHooks hooksFor(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    gpu::EvaluationHooks hooks;
    hooks.isCancelled = [api, context] { return api->isCancelled(context) != 0; };
    hooks.reportProgress = [api, context](float fraction) {
        api->reportProgress(context, fraction);
    };
    return hooks;
}

/* ================================================================ thermal == */

/// The eight neighbours, and the fact that the opposite of direction d is
/// 7 - d, which is what lets a cell work out what it sent where.
constexpr int neighbourRow[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
constexpr int neighbourColumn[8] = {-1, 0, 1, -1, 1, -1, 0, 1};

bool isDiagonal(int direction) {
    return neighbourRow[direction] != 0 && neighbourColumn[direction] != 0;
}

struct ThermalSettings {
    int size = 0;
    /// The height difference a straight neighbour may be below this cell
    /// before material starts to move. Diagonals get the same angle over a
    /// longer run, so their threshold is larger by the square root of two.
    float talusDropMeters = 0.0f;
    float rate = 0.5f;
};

bool isInsideGrid(int row, int column, int size) {
    return row >= 0 && row < size && column >= 0 && column < size;
}

float thresholdFor(int direction, float talusDropMeters) {
    if (isDiagonal(direction)) {
        return talusDropMeters * std::numbers::sqrt2_v<float>;
    }
    return talusDropMeters;
}

/// How far each downhill neighbour lies below the angle of repose. Anything
/// at or above that angle is stable and takes nothing.
struct SlumpExcess {
    float perDirection[8] = {};
    float total = 0.0f;
    float largest = 0.0f;
};

SlumpExcess excessAround(const float* height, const ThermalSettings& settings, int row,
                         int column) {
    const float centre = height[static_cast<std::size_t>(row) * settings.size + column];
    SlumpExcess excess;

    for (int direction = 0; direction < 8; ++direction) {
        const int neighbourAtRow = row + neighbourRow[direction];
        const int neighbourAtColumn = column + neighbourColumn[direction];
        if (!isInsideGrid(neighbourAtRow, neighbourAtColumn, settings.size)) {
            continue;
        }
        const float drop =
            centre
            - height[static_cast<std::size_t>(neighbourAtRow) * settings.size + neighbourAtColumn];
        const float over = drop - thresholdFor(direction, settings.talusDropMeters);
        if (over <= 0.0f) {
            continue;
        }
        excess.perDirection[direction] = over;
        excess.total += over;
        excess.largest = std::max(excess.largest, over);
    }
    return excess;
}

/**
 * Material leaves the cell and arrives at its neighbours in the same pass, so
 * two neighbouring cells write into each other at once. The delta grid takes
 * those writes as integer atomics, which is what makes the result independent
 * of the order the threads happened to run in.
 *
 * `available` limits how much this cell has to give. Bedrock slumping passes
 * null and moves whatever is unstable; a sediment layer passes its own depth,
 * so loose material slides off the rock without taking the rock with it.
 */
void scatterSlump(const float* height, const float* available, FixedPointGrid& delta,
                  const ThermalSettings& settings, int row, int column) {
    const std::size_t index = static_cast<std::size_t>(row) * settings.size + column;
    const SlumpExcess excess = excessAround(height, settings, row, column);
    if (excess.total <= 0.0f) {
        return;
    }

    // Half the steepest excess keeps a step from overshooting into a hole.
    float moved = excess.largest * 0.5f * settings.rate;
    if (available != nullptr) {
        moved = std::min(moved, available[index]);
    }
    if (moved <= 0.0f) {
        return;
    }
    delta.add(index, -moved);
    for (int direction = 0; direction < 8; ++direction) {
        if (excess.perDirection[direction] <= 0.0f) {
            continue;
        }
        const std::size_t target =
            static_cast<std::size_t>(row + neighbourRow[direction]) * settings.size + column
            + neighbourColumn[direction];
        delta.add(target, moved * excess.perDirection[direction] / excess.total);
    }
}

void applyDelta(const TerrainContextApi* api, TerrainEvaluationContext* context, float* height,
                const FixedPointGrid& delta, int size) {
    forEachRowRange(api, context, size, [&](int firstRow, int lastRow) {
        const gpu::FlushDenormalsToZero denormals;
        for (int row = firstRow; row < lastRow; ++row) {
            for (int column = 0; column < size; ++column) {
                const std::size_t index = static_cast<std::size_t>(row) * size + column;
                height[index] += delta.valueAt(index);
            }
        }
    });
}

/// Fills `delta` with one pass of slumping over `surface`. The caller applies
/// it, because a sediment layer has to move both its own depth and the
/// surface standing on top of it.
void thermalScatter(const TerrainContextApi* api, TerrainEvaluationContext* context,
                    const float* surface, const float* available, FixedPointGrid& delta,
                    const ThermalSettings& settings) {
    delta.clear();
    forEachRowRange(api, context, settings.size, [&](int firstRow, int lastRow) {
        const gpu::FlushDenormalsToZero denormals;
        for (int row = firstRow; row < lastRow; ++row) {
            for (int column = 0; column < settings.size; ++column) {
                scatterSlump(surface, available, delta, settings, row, column);
            }
        }
    });
}

ThermalSettings thermalSettingsFor(const TerrainContextApi* api,
                                   TerrainEvaluationContext* context, double talusAngleDegrees,
                                   float rate) {
    ThermalSettings settings;
    settings.size = api->width(context);
    settings.rate = rate;
    const double radians = talusAngleDegrees * std::numbers::pi / 180.0;
    settings.talusDropMeters =
        static_cast<float>(std::tan(radians) * api->cellSizeMeters(context));
    return settings;
}

const TerrainPortDescriptor erosionInputs[] = {
    {"In", "The surface to erode", TerrainPortType_Field, TerrainFieldSemantics_Heightfield, 0},
    {"Mask", "Scales the effect per cell", TerrainPortType_Field, TerrainFieldSemantics_Mask, 1}};

const TerrainPortDescriptor thermalOutputs[] = {
    {"Out", "The slumped surface", TerrainPortType_Field, TerrainFieldSemantics_Heightfield, 0}};

const TerrainParameterDescriptor thermalParameters[] = {
    {"talusAngle", "Angle of repose", "The steepest slope loose material will hold", "deg",
     TerrainParameterType_Float, 35.0, 0.0, 89.0, "", nullptr, 0},
    {"iterations", "Iterations", "How many times material is allowed to move on", "",
     TerrainParameterType_Int, 40.0, 1.0, 1000.0, "", nullptr, 0},
    {"strength", "Strength", "How much of the unstable material moves each time", "",
     TerrainParameterType_Float, 0.5, 0.0, 1.0, "", nullptr, 0}};

gpu::ThermalJob thermalJobFor(const ThermalSettings& settings, float* surface, float* sediment,
                              std::size_t count, int iterations, float amountPerIteration) {
    gpu::ThermalJob job;
    job.surface = surface;
    job.sediment = sediment;
    job.count = count;
    job.size = settings.size;
    job.iterations = iterations;
    job.talusDropMeters = settings.talusDropMeters;
    job.rate = settings.rate;
    job.amountPerIteration = amountPerIteration;
    return job;
}

void releaseSediment(const gpu::ThermalJob& job) {
    for (std::size_t index = 0; index < job.count; ++index) {
        job.surface[index] += job.amountPerIteration;
        job.sediment[index] += job.amountPerIteration;
    }
}

void runThermalOnCpu(const TerrainContextApi* api, TerrainEvaluationContext* context,
                     const gpu::ThermalJob& job, const ThermalSettings& settings) {
    FixedPointGrid delta(job.count);
    for (int iteration = 0; iteration < job.iterations; ++iteration) {
        if (api->isCancelled(context) != 0) {
            return;
        }
        if (job.sediment != nullptr) {
            releaseSediment(job);
        }
        thermalScatter(api, context, job.surface, job.sediment, delta, settings);
        applyDelta(api, context, job.surface, delta, settings.size);
        if (job.sediment != nullptr) {
            applyDelta(api, context, job.sediment, delta, settings.size);
        }
        api->reportProgress(context, static_cast<float>(iteration + 1) / job.iterations);
    }
}

/// The thermal solve, on whichever backend this machine offers. A device that
/// declines costs speed and nothing else — the two agree bit for bit.
void runThermalSolve(const TerrainContextApi* api, TerrainEvaluationContext* context,
                     const gpu::ThermalJob& job, const ThermalSettings& settings) {
    const gpu::ErosionBackend backend = gpu::selectedErosionBackend();
    if (backend != gpu::ErosionBackend::Cpu
        && gpu::runThermalOnDevice(backend, job, hooksFor(api, context))) {
        return;
    }
    runThermalOnCpu(api, context, job, settings);
}

void evaluateThermalErosion(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const gpu::FlushDenormalsToZero denormals;
    const std::size_t count = cellCount(api, context);
    const float* input = api->inputField(context, 0);
    const int iterations = static_cast<int>(api->parameterNumber(context, 1));
    const ThermalSettings settings = thermalSettingsFor(
        api, context, api->parameterNumber(context, 0),
        static_cast<float>(api->parameterNumber(context, 2)));

    std::vector<float> working(input, input + count);
    runThermalSolve(api, context,
                    thermalJobFor(settings, working.data(), nullptr, count, iterations, 0.0f),
                    settings);

    blendThroughMask(input, working.data(), api->inputField(context, 1),
                     api->outputField(context, 0), count);
}

const TerrainNodeDescriptor thermalDescriptor = {
    "ThermalErosion",
    "Erode",
    "Lets loose material slide until no slope is steeper than the angle of repose",
    1,
    erosionInputs,
    2,
    thermalOutputs,
    1,
    thermalParameters,
    3,
    &evaluateThermalErosion};

/* ================================================================ deposit == */

const TerrainPortDescriptor depositOutputs[] = {
    {"Out", "The surface with sediment on it", TerrainPortType_Field,
     TerrainFieldSemantics_Heightfield, 0},
    {"Deposit", "How thick the sediment ended up, at its own scale",
     TerrainPortType_Field, TerrainFieldSemantics_Mask, 0}};

const TerrainParameterDescriptor depositParameters[] = {
    {"amount", "Amount", "Total depth of sediment released over the whole surface", "m",
     TerrainParameterType_Float, 30.0, 0.0, 2000.0, "", nullptr, 0},
    {"settleAngle", "Settle angle", "The steepest slope the sediment will rest on", "deg",
     TerrainParameterType_Float, 12.0, 0.0, 89.0, "", nullptr, 0},
    {"iterations", "Iterations", "How far the sediment is allowed to travel", "",
     TerrainParameterType_Int, 60.0, 1.0, 1000.0, "", nullptr, 0}};

/// A depth in meters as a mask, scaled against its own deepest point: a mask
/// is a weight, and the meters are already in the surface output.
void writeNormalisedDepth(const std::vector<float>& depth, float* destination) {
    const float deepest = *std::max_element(depth.begin(), depth.end());
    const float scale = 1.0f / std::max(deepest, 1.0e-6f);
    for (std::size_t index = 0; index < depth.size(); ++index) {
        destination[index] = std::clamp(depth[index] * scale, 0.0f, 1.0f);
    }
}

/**
 * Sediment is released evenly and then allowed to settle. Only the sediment
 * moves — the rock underneath stays where it is — so the layer slides off
 * anything steeper than the settle angle and gathers wherever the ground
 * already curves inwards. That fills a concavity without anyone having to
 * find the concavities first.
 */
void evaluateDeposit(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const gpu::FlushDenormalsToZero denormals;
    const std::size_t count = cellCount(api, context);
    const float* input = api->inputField(context, 0);
    const float amount = static_cast<float>(api->parameterNumber(context, 0));
    const int iterations = static_cast<int>(api->parameterNumber(context, 2));
    const ThermalSettings settings =
        thermalSettingsFor(api, context, api->parameterNumber(context, 1), 0.5f);
    const float perIteration = amount / iterations;

    std::vector<float> surface(input, input + count);
    std::vector<float> sediment(count, 0.0f);
    runThermalSolve(api, context,
                    thermalJobFor(settings, surface.data(), sediment.data(), count, iterations,
                                  perIteration),
                    settings);

    blendThroughMask(input, surface.data(), api->inputField(context, 1),
                     api->outputField(context, 0), count);
    writeNormalisedDepth(sediment, api->outputField(context, 1));
}

const TerrainNodeDescriptor depositDescriptor = {
    "Deposit",
    "Erode",
    "Fills hollows with sediment that slides off anything too steep to hold it",
    1,
    erosionInputs,
    2,
    depositOutputs,
    2,
    depositParameters,
    3,
    &evaluateDeposit};

/* ============================================================== hydraulic == */

/**
 * The droplet solver works in cell units, not meters: one step sideways is
 * one unit, so a height difference of one unit is a 45 degree slope. Mixing
 * the two would make every parameter depend on the ground resolution — a
 * capacity tuned at 30 m per cell would be meaningless at 3 m per cell. The
 * caller scales the surface on the way in and back again on the way out.
 */
struct HydraulicSettings {
    int size = 0;
    int lifetime = 48;
    /// A droplet's share of the rainfall. Capacity scales with it, so the
    /// total erosion depends on how much rain fell and not on how many
    /// droplets it was divided into: raising the droplet count refines the
    /// same result instead of deepening it.
    float water = 1.0f;
    float inertia = 0.05f;
    float capacityFactor = 4.0f;
    float minimumSlope = 0.01f;
    float erosionRate = 0.3f;
    float depositionRate = 0.3f;
    float evaporation = 0.02f;
    float gravity = 10.0f;
};

struct HydraulicGrids {
    const float* height = nullptr;
    FixedPointGrid* delta = nullptr;
    FixedPointGrid* wear = nullptr;
    FixedPointGrid* deposit = nullptr;
    FixedPointGrid* flow = nullptr;
};

struct SurfacePoint {
    float height = 0.0f;
    /// Change per row southwards and per column eastwards, in meters per cell.
    float alongRows = 0.0f;
    float alongColumns = 0.0f;
};

SurfacePoint surfaceAt(const float* height, int size, float row, float column) {
    const int baseRow = std::min(static_cast<int>(row), size - 2);
    const int baseColumn = std::min(static_cast<int>(column), size - 2);
    const float rowFraction = row - baseRow;
    const float columnFraction = column - baseColumn;

    const float* upper = height + static_cast<std::size_t>(baseRow) * size + baseColumn;
    const float* lower = upper + size;
    const float northWest = upper[0];
    const float northEast = upper[1];
    const float southWest = lower[0];
    const float southEast = lower[1];

    SurfacePoint point;
    point.alongColumns = (northEast - northWest) * (1.0f - rowFraction)
                         + (southEast - southWest) * rowFraction;
    point.alongRows = (southWest - northWest) * (1.0f - columnFraction)
                      + (southEast - northEast) * columnFraction;
    point.height = northWest * (1 - columnFraction) * (1 - rowFraction)
                   + northEast * columnFraction * (1 - rowFraction)
                   + southWest * (1 - columnFraction) * rowFraction
                   + southEast * columnFraction * rowFraction;
    return point;
}

/**
 * Moves `signedAmount` of material across the droplet's footprint, and records
 * what happened in `tally` — the wear grid when taking, the deposit grid when
 * leaving. Both directions use the same footprint deliberately: a droplet that
 * takes from a disc but gives back to a single cell builds a spike in every
 * hollow it reaches, because deposition then concentrates what erosion spread.
 */
void applyOverBrush(const HydraulicGrids& grids, const ErosionBrush& brush, int size, int row,
                    int column, float signedAmount, FixedPointGrid& tally) {
    for (const ErosionBrush::Sample& sample : brush.samples()) {
        const int targetRow = row + sample.offsetRow;
        const int targetColumn = column + sample.offsetColumn;
        if (!isInsideGrid(targetRow, targetColumn, size)) {
            continue;
        }
        const std::size_t index = static_cast<std::size_t>(targetRow) * size + targetColumn;
        grids.delta->add(index, signedAmount * sample.weight);
        tally.add(index, std::abs(signedAmount) * sample.weight);
    }
}

constexpr float maximumDropletSpeed = 4.0f;

struct Droplet {
    float row = 0.0f;
    float column = 0.0f;
    float directionRow = 0.0f;
    float directionColumn = 0.0f;
    float speed = 1.0f;
    float water = 1.0f;
    float sediment = 0.0f;
};

/// How much sediment the droplet can hold right now. A droplet running over
/// flat ground would otherwise have no capacity at all and drop everything.
float carryingCapacity(const Droplet& droplet, float deltaHeight,
                       const HydraulicSettings& settings) {
    return std::max(-deltaHeight * droplet.speed * droplet.water * settings.capacityFactor,
                    settings.minimumSlope * settings.capacityFactor);
}

/// How much the droplet gives back this step: everything above its capacity,
/// or enough to climb the step it just failed to descend.
float sedimentToDrop(const Droplet& droplet, float capacity, float deltaHeight,
                     const HydraulicSettings& settings) {
    if (deltaHeight > 0.0f) {
        // Uphill: the droplet is climbing out of a hollow and fills it instead.
        return std::min(deltaHeight, droplet.sediment);
    }
    return (droplet.sediment - capacity) * settings.depositionRate;
}

void settleOrBite(Droplet& droplet, const HydraulicGrids& grids, const ErosionBrush& brush,
                  const HydraulicSettings& settings, float deltaHeight, int fromRow,
                  int fromColumn) {
    const float capacity = carryingCapacity(droplet, deltaHeight, settings);
    if (deltaHeight > 0.0f || droplet.sediment > capacity) {
        const float dropped = sedimentToDrop(droplet, capacity, deltaHeight, settings);
        droplet.sediment -= dropped;
        applyOverBrush(grids, brush, settings.size, fromRow, fromColumn, dropped,
                       *grids.deposit);
        return;
    }

    const float taken =
        std::min((capacity - droplet.sediment) * settings.erosionRate, -deltaHeight);
    droplet.sediment += taken;
    applyOverBrush(grids, brush, settings.size, fromRow, fromColumn, -taken, *grids.wear);
}

/// One step downhill. Returns false when the droplet has left the grid or has
/// come to rest, which is what ends its life early.
bool stepDroplet(Droplet& droplet, const HydraulicGrids& grids, const ErosionBrush& brush,
                 const HydraulicSettings& settings) {
    const SurfacePoint here = surfaceAt(grids.height, settings.size, droplet.row, droplet.column);
    droplet.directionRow =
        droplet.directionRow * settings.inertia - here.alongRows * (1.0f - settings.inertia);
    droplet.directionColumn =
        droplet.directionColumn * settings.inertia - here.alongColumns * (1.0f - settings.inertia);

    const float length = std::sqrt(droplet.directionRow * droplet.directionRow
                                   + droplet.directionColumn * droplet.directionColumn);
    if (length < 1.0e-6f) {
        return false;
    }

    const float fromRow = droplet.row;
    const float fromColumn = droplet.column;
    droplet.row += droplet.directionRow / length;
    droplet.column += droplet.directionColumn / length;
    if (droplet.row < 0.0f || droplet.column < 0.0f || droplet.row >= settings.size - 1
        || droplet.column >= settings.size - 1) {
        return false;
    }

    const float deltaHeight =
        surfaceAt(grids.height, settings.size, droplet.row, droplet.column).height - here.height;
    settleOrBite(droplet, grids, brush, settings, deltaHeight, static_cast<int>(fromRow),
                 static_cast<int>(fromColumn));

    // A droplet reaches a terminal velocity. Without the cap it accelerates
    // for its whole life, and its carrying capacity with it, until a single
    // droplet can drop a mountain's worth of sediment into one hollow.
    droplet.speed = std::min(std::sqrt(std::max(droplet.speed * droplet.speed
                                                    - deltaHeight * settings.gravity,
                                                0.0f)),
                             maximumDropletSpeed);
    droplet.water *= 1.0f - settings.evaporation;
    grids.flow->add(static_cast<std::size_t>(droplet.row) * settings.size
                        + static_cast<std::size_t>(droplet.column),
                    droplet.water);
    return true;
}

void runDroplet(std::uint64_t seed, const HydraulicGrids& grids, const ErosionBrush& brush,
                const HydraulicSettings& settings) {
    std::uint64_t state = seed;
    Droplet droplet;
    droplet.water = settings.water;
    droplet.row = terrainRandomUnitFloat(&state) * (settings.size - 2);
    droplet.column = terrainRandomUnitFloat(&state) * (settings.size - 2);

    for (int step = 0; step < settings.lifetime; ++step) {
        if (!stepDroplet(droplet, grids, brush, settings)) {
            return;
        }
    }
}

/**
 * One batch of droplets, all reading the same frozen surface and writing into
 * the delta grid. Batching is what makes the run reproducible: a droplet never
 * sees another droplet's work mid-batch, so nothing depends on which thread
 * got there first. Batch membership is a function of the droplet index alone.
 */
struct BatchWork {
    std::uint64_t seed = 0;
    std::int64_t firstDroplet = 0;
    std::int64_t lastDroplet = 0;
    const HydraulicGrids* grids = nullptr;
    const ErosionBrush* brush = nullptr;
    const HydraulicSettings* settings = nullptr;
};

void runDropletLane(void* userData, std::int32_t laneIndex, std::int32_t laneCount) {
    // The mode is a thread's own, and this one belongs to the engine's pool.
    const gpu::FlushDenormalsToZero denormals;
    const BatchWork& work = *static_cast<const BatchWork*>(userData);
    const std::int64_t span = work.lastDroplet - work.firstDroplet;
    const std::int64_t first = work.firstDroplet + span * laneIndex / laneCount;
    const std::int64_t last = work.firstDroplet + span * (laneIndex + 1) / laneCount;

    for (std::int64_t droplet = first; droplet < last; ++droplet) {
        runDroplet(terrainSeedForLane(work.seed, static_cast<std::uint64_t>(droplet)), *work.grids,
                   *work.brush, *work.settings);
    }
}

const TerrainPortDescriptor hydraulicOutputs[] = {
    {"Out", "The eroded surface", TerrainPortType_Field, TerrainFieldSemantics_Heightfield, 0},
    {"Wear", "Where material was carried away", TerrainPortType_Field,
     TerrainFieldSemantics_Mask, 0},
    {"Deposit", "Where material settled again", TerrainPortType_Field,
     TerrainFieldSemantics_Mask, 0},
    {"Flow", "How much water passed over each cell", TerrainPortType_Field,
     TerrainFieldSemantics_Mask, 0}};

const TerrainParameterDescriptor hydraulicParameters[] = {
    {"density", "Droplets per cell",
     "How finely the rainfall is sampled. Raising it refines the same result rather than "
     "deepening it, at the cost of time",
     "", TerrainParameterType_Float, 1.0, 0.0, 16.0, "", nullptr, 0},
    {"seed", "Seed", "Varies this node alone, on top of the graph seed", "",
     TerrainParameterType_Seed, 0.0, 0.0, 4294967295.0, "", nullptr, 0},
    {"lifetime", "Droplet life", "How far a droplet travels before it is gone", "",
     TerrainParameterType_Int, 64.0, 1.0, 512.0, "", nullptr, 0},
    {"radius", "Channel width", "How wide a patch a droplet works over, and so how broad its "
     "channels are. Narrow is faster but starts to punch holes",
     "m", TerrainParameterType_Float, 60.0, 0.0, 5000.0, "", nullptr, 0},
    {"inertia", "Inertia", "How much a droplet keeps its heading instead of falling downhill",
     "", TerrainParameterType_Float, 0.05, 0.0, 0.99, "", nullptr, 0},
    {"capacity", "Capacity", "How much sediment the rain can carry, and so how deeply it cuts",
     "", TerrainParameterType_Float, 1.0, 0.01, 16.0, "", nullptr, 0},
    {"erosionRate", "Erosion rate", "How quickly an unloaded droplet cuts in", "",
     TerrainParameterType_Float, 0.3, 0.0, 1.0, "", nullptr, 0},
    {"depositionRate", "Deposition rate", "How quickly an overloaded droplet drops its load", "",
     TerrainParameterType_Float, 0.3, 0.0, 1.0, "", nullptr, 0},
    {"evaporation", "Evaporation", "How fast a droplet dries up as it travels", "",
     TerrainParameterType_Float, 0.02, 0.0, 1.0, "", nullptr, 0},
    {"gravity", "Gravity", "How much a drop in height speeds a droplet up", "",
     TerrainParameterType_Float, 10.0, 0.1, 40.0, "", nullptr, 0}};

HydraulicSettings hydraulicSettingsFor(const TerrainContextApi* api,
                                       TerrainEvaluationContext* context) {
    HydraulicSettings settings;
    settings.size = api->width(context);
    settings.lifetime = static_cast<int>(api->parameterNumber(context, 2));
    settings.water =
        1.0f / std::max(static_cast<float>(api->parameterNumber(context, 0)), 1.0e-3f);
    settings.inertia = static_cast<float>(api->parameterNumber(context, 4));
    settings.capacityFactor = static_cast<float>(api->parameterNumber(context, 5));
    settings.erosionRate = static_cast<float>(api->parameterNumber(context, 6));
    settings.depositionRate = static_cast<float>(api->parameterNumber(context, 7));
    settings.evaporation = static_cast<float>(api->parameterNumber(context, 8));
    settings.gravity = static_cast<float>(api->parameterNumber(context, 9));
    return settings;
}

constexpr std::int32_t dropletLaneCount = 64;

/**
 * The rainfall arrives in this many batches, and the surface is updated
 * between them.
 *
 * Droplets within a batch cannot see each other's work, which is what makes
 * the run reproducible — but a batch that covered the whole rainfall would let
 * every droplet pour into the same untouched hollow and pile a mountain in it.
 * Splitting the *rainfall* rather than the droplet count keeps the feedback
 * identical however finely the rain was divided, which is what lets the
 * droplet count be a quality dial rather than a strength dial.
 */
constexpr std::int64_t rainfallBatchCount = 32;

std::int64_t dropletsPerBatch(std::int64_t dropletCount) {
    return std::max<std::int64_t>((dropletCount + rainfallBatchCount - 1) / rainfallBatchCount, 1);
}

void writeNormalisedGrid(const FixedPointGrid& source, float* destination) {
    const float scale = 1.0f / std::max(source.strongest(), 1.0e-6f);
    for (std::size_t index = 0; index < source.size(); ++index) {
        destination[index] = std::clamp(source.valueAt(index) * scale, 0.0f, 1.0f);
    }
}

static_assert(sizeof(ErosionBrush::Sample) == sizeof(gpu::BrushSample),
              "the brush is uploaded to a device as it stands, so the two layouts are one");

/// The grids a hydraulic run accumulates into, and the surface it walks over.
struct HydraulicWorkspace {
    std::vector<float> working;
    FixedPointGrid delta;
    FixedPointGrid wear;
    FixedPointGrid deposit;
    FixedPointGrid flow;

    explicit HydraulicWorkspace(std::size_t count)
        : working(count), delta(count), wear(count), deposit(count), flow(count) {}
};

gpu::HydraulicJob hydraulicJobFor(const HydraulicSettings& settings, HydraulicWorkspace& workspace,
                                  const ErosionBrush& brush, std::size_t count,
                                  std::int64_t dropletCount, std::uint64_t seed) {
    gpu::HydraulicJob job;
    job.surface = workspace.working.data();
    job.delta = workspace.delta.data();
    job.wear = workspace.wear.data();
    job.deposit = workspace.deposit.data();
    job.flow = workspace.flow.data();
    job.brush = reinterpret_cast<const gpu::BrushSample*>(brush.samples().data());
    job.brushSampleCount = static_cast<std::int32_t>(brush.samples().size());
    job.count = count;
    job.dropletCount = dropletCount;
    job.batchSize = dropletsPerBatch(dropletCount);
    job.seed = seed;
    job.size = settings.size;
    job.lifetime = settings.lifetime;
    job.water = settings.water;
    job.inertia = settings.inertia;
    job.capacityFactor = settings.capacityFactor;
    job.minimumSlope = settings.minimumSlope;
    job.erosionRate = settings.erosionRate;
    job.depositionRate = settings.depositionRate;
    job.evaporation = settings.evaporation;
    job.gravity = settings.gravity;
    return job;
}

void runRainfallOnCpu(const TerrainContextApi* api, TerrainEvaluationContext* context,
                      const HydraulicSettings& settings, HydraulicWorkspace& workspace,
                      const ErosionBrush& brush, const gpu::HydraulicJob& job) {
    const HydraulicGrids grids{workspace.working.data(), &workspace.delta, &workspace.wear,
                               &workspace.deposit, &workspace.flow};
    for (std::int64_t first = 0; first < job.dropletCount; first += job.batchSize) {
        if (api->isCancelled(context) != 0) {
            return;
        }
        workspace.delta.clear();
        BatchWork work{job.seed, first, std::min(first + job.batchSize, job.dropletCount), &grids,
                       &brush, &settings};
        api->runLanes(context, dropletLaneCount, &runDropletLane, &work);
        applyDelta(api, context, workspace.working.data(), workspace.delta, settings.size);
        api->reportProgress(context, static_cast<float>(first) / job.dropletCount);
    }
}

void runRainfall(const TerrainContextApi* api, TerrainEvaluationContext* context,
                 const HydraulicSettings& settings, HydraulicWorkspace& workspace,
                 const ErosionBrush& brush, const gpu::HydraulicJob& job) {
    const gpu::ErosionBackend backend = gpu::selectedErosionBackend();
    if (backend != gpu::ErosionBackend::Cpu
        && gpu::runHydraulicOnDevice(backend, job, hooksFor(api, context))) {
        return;
    }
    runRainfallOnCpu(api, context, settings, workspace, brush, job);
}

void evaluateHydraulicErosion(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const gpu::FlushDenormalsToZero denormals;
    const std::size_t count = cellCount(api, context);
    const float* input = api->inputField(context, 0);
    const HydraulicSettings settings = hydraulicSettingsFor(api, context);
    if (settings.size < 4) {
        api->setError(context, "Hydraulic erosion needs a grid of at least four cells");
        return;
    }

    const auto dropletCount = static_cast<std::int64_t>(api->parameterNumber(context, 0) * count);
    const double cellSizeMeters = api->cellSizeMeters(context);
    const int radiusInCells =
        static_cast<int>(std::lround(api->parameterNumber(context, 3) / cellSizeMeters));
    const ErosionBrush brush(radiusInCells, settings.size);

    HydraulicWorkspace workspace(count);
    for (std::size_t index = 0; index < count; ++index) {
        workspace.working[index] = static_cast<float>(input[index] / cellSizeMeters);
    }

    const std::uint64_t seed = terrainSeedForLane(
        api->randomSeed(context), static_cast<std::uint64_t>(api->parameterNumber(context, 1)));
    const gpu::HydraulicJob job =
        hydraulicJobFor(settings, workspace, brush, count, dropletCount, seed);
    runRainfall(api, context, settings, workspace, brush, job);

    const FixedPointGrid& wear = workspace.wear;
    const FixedPointGrid& deposit = workspace.deposit;
    std::vector<float>& working = workspace.working;

    // Every write to the surface went into wear or deposit as well, so their
    // difference is the whole change. Reading it back that way keeps the
    // absolute altitude out of the round trip through cell units, and makes a
    // run of no droplets return the input untouched rather than merely close.
    for (std::size_t index = 0; index < count; ++index) {
        const float change = deposit.valueAt(index) - wear.valueAt(index);
        working[index] = input[index] + static_cast<float>(change * cellSizeMeters);
    }
    blendThroughMask(input, working.data(), api->inputField(context, 1),
                     api->outputField(context, 0), count);
    writeNormalisedGrid(wear, api->outputField(context, 1));
    writeNormalisedGrid(deposit, api->outputField(context, 2));
    writeNormalisedGrid(workspace.flow, api->outputField(context, 3));
}

const TerrainNodeDescriptor hydraulicDescriptor = {
    "HydraulicErosion",
    "Erode",
    "Runs rain over the surface, cutting channels and laying down what it carries",
    1,
    erosionInputs,
    2,
    hydraulicOutputs,
    4,
    hydraulicParameters,
    10,
    &evaluateHydraulicErosion};

}  // namespace

void registerErosionNodes(const TerrainRegistryApi& api, TerrainRegistry* registry) {
    api.registerNode(registry, &thermalDescriptor);
    api.registerNode(registry, &depositDescriptor);
    api.registerNode(registry, &hydraulicDescriptor);
}

}  // namespace terrain::nodes
