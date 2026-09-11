/*
 * The CUDA erosion backend.
 *
 * Where the Vulkan path has to rebuild division and square root out of the
 * operations Vulkan promises to round correctly, CUDA promises those two
 * directly — given `--prec-div=true` and `--prec-sqrt=true`, which are the
 * defaults, plus `-fmad=false` to stop it fusing multiply-adds the host
 * computes separately and `-ftz=true` to flush denormals the way the host is
 * made to. See the flags in engine/CMakeLists.txt; they are load-bearing, not
 * tuning.
 *
 * So these kernels are a direct transcription of erosion_nodes.cpp rather than
 * a careful reconstruction of it, and the parity tests hold them to the same
 * standard either way.
 */

#include <cuda_runtime.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#include "erosion_backend.h"

namespace terrain::gpu {
namespace {

constexpr int blockSize = 256;
constexpr float unitsPerMeter = 100000.0f;
constexpr float maximumDropletSpeed = 4.0f;

/// std::min and std::max compare with `<` and keep the first argument on a
/// tie. `fminf` does not answer the same question, so it is not used.
__host__ __device__ inline float smallerOf(float first, float second) {
    if (second < first) {
        return second;
    }
    return first;
}

__host__ __device__ inline float largerOf(float first, float second) {
    if (first < second) {
        return second;
    }
    return first;
}

__host__ __device__ inline int smallerIndex(int first, int second) {
    if (second < first) {
        return second;
    }
    return first;
}

__device__ inline void addFixed(int* grid, std::size_t index, float meters) {
    const int units = static_cast<int>(lroundf(meters * unitsPerMeter));
    if (units == 0) {
        return;
    }
    atomicAdd(&grid[index], units);
}

__device__ inline bool isInsideGrid(int row, int column, int size) {
    return row >= 0 && row < size && column >= 0 && column < size;
}

/* --------------------------------------------------------------- the RNG -- */

__device__ inline std::uint64_t randomNext(std::uint64_t& state) {
    std::uint64_t value = (state += 0x9E3779B97F4A7C15ull);
    value = (value ^ (value >> 30)) * 0xBF58476D1CE4E5B9ull;
    value = (value ^ (value >> 27)) * 0x94D049BB133111EBull;
    return value ^ (value >> 31);
}

__device__ inline float randomUnitFloat(std::uint64_t& state) {
    return static_cast<float>(randomNext(state) >> 40) * (1.0f / 16777216.0f);
}

__device__ inline std::uint64_t seedForLane(std::uint64_t seed, std::uint64_t lane) {
    std::uint64_t state = seed ^ (lane * 0x9E3779B97F4A7C15ull);
    return randomNext(state);
}

/* ================================================================ thermal == */

__constant__ int neighbourRow[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
__constant__ int neighbourColumn[8] = {-1, 0, 1, -1, 1, -1, 0, 1};

struct ThermalParameters {
    int size;
    float talusDropMeters;
    float rate;
    int hasAvailable;
};

__device__ inline float thresholdFor(int direction, float talusDropMeters) {
    if (neighbourRow[direction] != 0 && neighbourColumn[direction] != 0) {
        return talusDropMeters * 1.41421353816986083984375f;
    }
    return talusDropMeters;
}

struct SlumpExcess {
    float perDirection[8];
    float total;
    float largest;
};

__device__ SlumpExcess excessAround(const float* surface, const ThermalParameters& settings,
                                    int row, int column) {
    SlumpExcess excess;
    excess.total = 0.0f;
    excess.largest = 0.0f;
    for (int direction = 0; direction < 8; ++direction) {
        excess.perDirection[direction] = 0.0f;
    }

    const float centre = surface[static_cast<std::size_t>(row) * settings.size + column];
    for (int direction = 0; direction < 8; ++direction) {
        const int neighbourAtRow = row + neighbourRow[direction];
        const int neighbourAtColumn = column + neighbourColumn[direction];
        if (!isInsideGrid(neighbourAtRow, neighbourAtColumn, settings.size)) {
            continue;
        }
        const float drop =
            centre
            - surface[static_cast<std::size_t>(neighbourAtRow) * settings.size + neighbourAtColumn];
        const float over = drop - thresholdFor(direction, settings.talusDropMeters);
        if (over <= 0.0f) {
            continue;
        }
        excess.perDirection[direction] = over;
        excess.total += over;
        excess.largest = largerOf(excess.largest, over);
    }
    return excess;
}

__global__ void thermalScatterKernel(const float* surface, const float* available, int* delta,
                                     ThermalParameters settings) {
    const std::size_t cell = blockIdx.x * static_cast<std::size_t>(blockDim.x) + threadIdx.x;
    const std::size_t count = static_cast<std::size_t>(settings.size) * settings.size;
    if (cell >= count) {
        return;
    }

    const int row = static_cast<int>(cell / settings.size);
    const int column = static_cast<int>(cell % settings.size);
    const SlumpExcess excess = excessAround(surface, settings, row, column);
    if (excess.total <= 0.0f) {
        return;
    }

    float moved = excess.largest * 0.5f * settings.rate;
    if (settings.hasAvailable != 0) {
        moved = smallerOf(moved, available[cell]);
    }
    if (moved <= 0.0f) {
        return;
    }

    addFixed(delta, cell, -moved);
    for (int direction = 0; direction < 8; ++direction) {
        if (excess.perDirection[direction] <= 0.0f) {
            continue;
        }
        const std::size_t target =
            static_cast<std::size_t>(row + neighbourRow[direction]) * settings.size + column
            + neighbourColumn[direction];
        addFixed(delta, target, moved * excess.perDirection[direction] / excess.total);
    }
}

__global__ void applyDeltaKernel(float* target, const int* delta, int count) {
    const std::size_t index = blockIdx.x * static_cast<std::size_t>(blockDim.x) + threadIdx.x;
    if (index >= static_cast<std::size_t>(count)) {
        return;
    }
    target[index] += static_cast<float>(delta[index]) / unitsPerMeter;
}

__global__ void addConstantKernel(float* target, int count, float amount) {
    const std::size_t index = blockIdx.x * static_cast<std::size_t>(blockDim.x) + threadIdx.x;
    if (index >= static_cast<std::size_t>(count)) {
        return;
    }
    target[index] += amount;
}

/* ============================================================== hydraulic == */

struct HydraulicParameters {
    int size;
    int lifetime;
    float water;
    float inertia;
    float capacityFactor;
    float minimumSlope;
    float erosionRate;
    float depositionRate;
    float evaporation;
    float gravity;
    int brushSampleCount;
};

struct SurfacePoint {
    float height;
    float alongRows;
    float alongColumns;
};

struct Droplet {
    float row;
    float column;
    float directionRow;
    float directionColumn;
    float speed;
    float water;
    float sediment;
};

struct HydraulicGrids {
    const float* surface;
    int* wear;
    int* deposit;
    int* flow;
    const BrushSample* brush;
};

/// What one step exchanged with the ground, and which grid records it.
struct Exchange {
    float amount;
    int* tally;
};

__device__ SurfacePoint surfaceAt(const float* surface, int size, float row, float column) {
    const int baseRow = smallerIndex(static_cast<int>(row), size - 2);
    const int baseColumn = smallerIndex(static_cast<int>(column), size - 2);
    const float rowFraction = row - baseRow;
    const float columnFraction = column - baseColumn;

    const float* upper = surface + static_cast<std::size_t>(baseRow) * size + baseColumn;
    const float* lower = upper + size;
    const float northWest = upper[0];
    const float northEast = upper[1];
    const float southWest = lower[0];
    const float southEast = lower[1];

    SurfacePoint point;
    point.alongColumns =
        (northEast - northWest) * (1.0f - rowFraction) + (southEast - southWest) * rowFraction;
    point.alongRows =
        (southWest - northWest) * (1.0f - columnFraction) + (southEast - northEast) * columnFraction;
    point.height = northWest * (1 - columnFraction) * (1 - rowFraction)
                   + northEast * columnFraction * (1 - rowFraction)
                   + southWest * (1 - columnFraction) * rowFraction
                   + southEast * columnFraction * rowFraction;
    return point;
}

__device__ void applyOverBrush(const BrushSample* brush, const HydraulicParameters& settings,
                               int row, int column, const Exchange& exchange) {
    for (int which = 0; which < settings.brushSampleCount; ++which) {
        const BrushSample sample = brush[which];
        const int targetRow = row + sample.offsetRow;
        const int targetColumn = column + sample.offsetColumn;
        if (!isInsideGrid(targetRow, targetColumn, settings.size)) {
            continue;
        }
        const std::size_t index =
            static_cast<std::size_t>(targetRow) * settings.size + targetColumn;
        addFixed(exchange.tally, index, fabsf(exchange.amount) * sample.weight);
    }
}

__device__ inline float carryingCapacity(const Droplet& droplet, float deltaHeight,
                                         const HydraulicParameters& settings) {
    return largerOf(-deltaHeight * droplet.speed * droplet.water * settings.capacityFactor,
                    settings.minimumSlope * settings.capacityFactor);
}

__device__ inline float sedimentToDrop(const Droplet& droplet, float capacity, float deltaHeight,
                                       const HydraulicParameters& settings) {
    if (deltaHeight > 0.0f) {
        return smallerOf(deltaHeight, droplet.sediment);
    }
    return (droplet.sediment - capacity) * settings.depositionRate;
}

__device__ Exchange settleOrBite(Droplet& droplet, const HydraulicGrids& grids,
                                 const HydraulicParameters& settings, float deltaHeight) {
    const float capacity = carryingCapacity(droplet, deltaHeight, settings);
    if (deltaHeight > 0.0f || droplet.sediment > capacity) {
        const float dropped = sedimentToDrop(droplet, capacity, deltaHeight, settings);
        droplet.sediment -= dropped;
        return Exchange{dropped, grids.deposit};
    }

    const float taken =
        smallerOf((capacity - droplet.sediment) * settings.erosionRate, -deltaHeight);
    droplet.sediment += taken;
    return Exchange{taken, grids.wear};
}

__device__ bool stepDroplet(Droplet& droplet, const HydraulicGrids& grids,
                            const HydraulicParameters& settings) {
    const SurfacePoint here =
        surfaceAt(grids.surface, settings.size, droplet.row, droplet.column);
    droplet.directionRow =
        droplet.directionRow * settings.inertia - here.alongRows * (1.0f - settings.inertia);
    droplet.directionColumn =
        droplet.directionColumn * settings.inertia - here.alongColumns * (1.0f - settings.inertia);

    const float length = sqrtf(droplet.directionRow * droplet.directionRow
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
        surfaceAt(grids.surface, settings.size, droplet.row, droplet.column).height - here.height;
    // One brush pass whichever way the exchange went, so the lanes of a warp
    // that settled and the lanes that bit walk the brush together.
    const Exchange exchange = settleOrBite(droplet, grids, settings, deltaHeight);
    applyOverBrush(grids.brush, settings, static_cast<int>(fromRow), static_cast<int>(fromColumn),
                   exchange);

    droplet.speed = smallerOf(
        sqrtf(largerOf(droplet.speed * droplet.speed - deltaHeight * settings.gravity, 0.0f)),
        maximumDropletSpeed);
    droplet.water *= 1.0f - settings.evaporation;
    addFixed(grids.flow,
             static_cast<std::size_t>(droplet.row) * settings.size
                 + static_cast<std::size_t>(droplet.column),
             droplet.water);
    return true;
}

__global__ void hydraulicDropletKernel(HydraulicGrids grids, HydraulicParameters settings,
                                       std::uint64_t seed, std::uint64_t firstDroplet,
                                       std::uint32_t dropletCount) {
    const std::uint32_t index = blockIdx.x * blockDim.x + threadIdx.x;
    if (index >= dropletCount) {
        return;
    }

    std::uint64_t state = seedForLane(seed, firstDroplet + index);
    Droplet droplet;
    droplet.directionRow = 0.0f;
    droplet.directionColumn = 0.0f;
    droplet.speed = 1.0f;
    droplet.sediment = 0.0f;
    droplet.water = settings.water;
    droplet.row = randomUnitFloat(state) * (settings.size - 2);
    droplet.column = randomUnitFloat(state) * (settings.size - 2);

    for (int step = 0; step < settings.lifetime; ++step) {
        if (!stepDroplet(droplet, grids, settings)) {
            return;
        }
    }
}

/// Folds into the surface whatever the droplets moved since the last batch.
/// Deposit minus wear is the whole change, and integer subtraction is exact,
/// so the difference from what was already applied is the batch's own sum.
__global__ void applyNetChangeKernel(float* surface, const int* wear, const int* deposit,
                                     int* appliedUnits, int count) {
    const std::size_t index = blockIdx.x * static_cast<std::size_t>(blockDim.x) + threadIdx.x;
    if (index >= static_cast<std::size_t>(count)) {
        return;
    }
    const long long net = static_cast<long long>(deposit[index]) - wear[index];
    const long long change = net - appliedUnits[index];
    appliedUnits[index] = static_cast<int>(net);
    surface[index] += static_cast<float>(static_cast<int>(change)) / unitsPerMeter;
}

/* ============================================================ the plumbing == */

unsigned int blocksFor(std::size_t items) {
    return static_cast<unsigned int>((items + blockSize - 1) / blockSize);
}

/// Frees whatever it holds, so an early return cannot leak device memory.
template <typename Value>
class DeviceArray {
public:
    DeviceArray() = default;
    ~DeviceArray() { cudaFree(pointer_); }

    DeviceArray(const DeviceArray&) = delete;
    DeviceArray& operator=(const DeviceArray&) = delete;

    bool allocate(std::size_t count) {
        return cudaMalloc(&pointer_, count * sizeof(Value)) == cudaSuccess;
    }

    bool allocateZeroed(std::size_t count) {
        if (!allocate(count)) {
            return false;
        }
        return cudaMemset(pointer_, 0, count * sizeof(Value)) == cudaSuccess;
    }

    bool upload(const Value* source, std::size_t count) {
        return cudaMemcpy(pointer_, source, count * sizeof(Value), cudaMemcpyHostToDevice)
               == cudaSuccess;
    }

    bool download(Value* destination, std::size_t count) const {
        return cudaMemcpy(destination, pointer_, count * sizeof(Value), cudaMemcpyDeviceToHost)
               == cudaSuccess;
    }

    Value* get() const { return pointer_; }

private:
    Value* pointer_ = nullptr;
};

bool wasInterrupted(const EvaluationHooks& hooks) {
    return hooks.isCancelled && hooks.isCancelled();
}

void reportProgress(const EvaluationHooks& hooks, float fraction) {
    if (hooks.reportProgress) {
        hooks.reportProgress(fraction);
    }
}

ThermalParameters thermalParametersFor(const ThermalJob& job) {
    ThermalParameters settings;
    settings.size = job.size;
    settings.talusDropMeters = job.talusDropMeters;
    settings.rate = job.rate;
    settings.hasAvailable = static_cast<int>(job.sediment != nullptr);
    return settings;
}

HydraulicParameters hydraulicParametersFor(const HydraulicJob& job) {
    HydraulicParameters settings;
    settings.size = job.size;
    settings.lifetime = job.lifetime;
    settings.water = job.water;
    settings.inertia = job.inertia;
    settings.capacityFactor = job.capacityFactor;
    settings.minimumSlope = job.minimumSlope;
    settings.erosionRate = job.erosionRate;
    settings.depositionRate = job.depositionRate;
    settings.evaporation = job.evaporation;
    settings.gravity = job.gravity;
    settings.brushSampleCount = job.brushSampleCount;
    return settings;
}

}  // namespace

bool cudaDeviceIsPresent() {
    int deviceCount = 0;
    if (cudaGetDeviceCount(&deviceCount) != cudaSuccess) {
        return false;
    }
    return deviceCount > 0;
}

bool runThermalOnCuda(const ThermalJob& job, const EvaluationHooks& hooks) {
    if (!cudaDeviceIsPresent()) {
        return false;
    }

    DeviceArray<float> surface;
    DeviceArray<float> sediment;
    DeviceArray<int> delta;
    if (!surface.allocate(job.count) || !sediment.allocate(job.count)
        || !delta.allocate(job.count)) {
        return false;
    }
    if (!surface.upload(job.surface, job.count)) {
        return false;
    }
    if (job.sediment != nullptr && !sediment.upload(job.sediment, job.count)) {
        return false;
    }

    const ThermalParameters settings = thermalParametersFor(job);
    const unsigned int blocks = blocksFor(job.count);
    const int count = static_cast<int>(job.count);
    for (int iteration = 0; iteration < job.iterations; ++iteration) {
        if (wasInterrupted(hooks)) {
            return false;
        }
        if (job.sediment != nullptr) {
            addConstantKernel<<<blocks, blockSize>>>(surface.get(), count, job.amountPerIteration);
            addConstantKernel<<<blocks, blockSize>>>(sediment.get(), count, job.amountPerIteration);
        }
        cudaMemset(delta.get(), 0, job.count * sizeof(int));
        thermalScatterKernel<<<blocks, blockSize>>>(surface.get(), sediment.get(), delta.get(),
                                                    settings);
        applyDeltaKernel<<<blocks, blockSize>>>(surface.get(), delta.get(), count);
        if (job.sediment != nullptr) {
            applyDeltaKernel<<<blocks, blockSize>>>(sediment.get(), delta.get(), count);
        }
        reportProgress(hooks, static_cast<float>(iteration + 1) / job.iterations);
    }

    if (cudaDeviceSynchronize() != cudaSuccess) {
        return false;
    }
    if (!surface.download(job.surface, job.count)) {
        return false;
    }
    if (job.sediment == nullptr) {
        return true;
    }
    return sediment.download(job.sediment, job.count);
}

bool runHydraulicOnCuda(const HydraulicJob& job, const EvaluationHooks& hooks) {
    if (!cudaDeviceIsPresent()) {
        return false;
    }

    DeviceArray<float> surface;
    DeviceArray<int> appliedUnits;
    DeviceArray<int> wear;
    DeviceArray<int> deposit;
    DeviceArray<int> flow;
    DeviceArray<BrushSample> brush;
    if (!surface.allocate(job.count) || !appliedUnits.allocateZeroed(job.count)
        || !wear.allocateZeroed(job.count) || !deposit.allocateZeroed(job.count)
        || !flow.allocateZeroed(job.count) || !brush.allocate(job.brushSampleCount)) {
        return false;
    }
    if (!surface.upload(job.surface, job.count)
        || !brush.upload(job.brush, job.brushSampleCount)) {
        return false;
    }

    HydraulicGrids grids;
    grids.surface = surface.get();
    grids.wear = wear.get();
    grids.deposit = deposit.get();
    grids.flow = flow.get();
    grids.brush = brush.get();

    const HydraulicParameters settings = hydraulicParametersFor(job);
    const int count = static_cast<int>(job.count);
    for (std::int64_t first = 0; first < job.dropletCount; first += job.batchSize) {
        if (wasInterrupted(hooks)) {
            return false;
        }
        const std::int64_t last = std::min(first + job.batchSize, job.dropletCount);
        const auto inBatch = static_cast<std::uint32_t>(last - first);
        hydraulicDropletKernel<<<blocksFor(inBatch), blockSize>>>(
            grids, settings, job.seed, static_cast<std::uint64_t>(first), inBatch);
        applyNetChangeKernel<<<blocksFor(job.count), blockSize>>>(
            surface.get(), wear.get(), deposit.get(), appliedUnits.get(), count);
        reportProgress(hooks, static_cast<float>(first) / job.dropletCount);
    }

    if (cudaDeviceSynchronize() != cudaSuccess) {
        return false;
    }
    if (!wear.download(job.wear, job.count) || !deposit.download(job.deposit, job.count)) {
        return false;
    }
    return flow.download(job.flow, job.count);
}

}  // namespace terrain::gpu
