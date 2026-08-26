#pragma once

#include <cstdint>
#include <functional>
#include <string>

namespace terrain::gpu {

/// Where an erosion node's arithmetic happened.
enum class ErosionBackend {
    Cpu,
    Vulkan,
    Cuda
};

const char* nameOf(ErosionBackend backend);

/**
 * The backend erosion will use on this machine.
 *
 * A GPU is offered only once it has reproduced the host's arithmetic bit for
 * bit — see `deviceMatchesHostArithmetic` — and only where the host can be put
 * into the same denormal mode. Anything else answers `Cpu`, which is a slower
 * result and not a different one.
 *
 * `TERRAIN_EROSION_BACKEND` overrides the choice with `cpu`, `vulkan` or
 * `cuda`; an unavailable choice falls back rather than failing, and the tests
 * use it to run the same graph through every backend the machine has.
 */
ErosionBackend selectedErosionBackend();

/// Why the GPU was declined, when it was. Empty when it was not.
const std::string& erosionBackendExplanation();

/**
 * Pins the backend for the rest of the process, and reports whether that was
 * possible.
 *
 * This exists for the parity tests, which have to drive one graph through
 * every backend a machine offers and compare the results bit for bit — the
 * claim the whole GPU path rests on. Asking for a backend the machine cannot
 * provide changes nothing and returns false.
 */
bool forceErosionBackend(ErosionBackend backend);

/// A weighted disc of cells, laid out exactly as the shader's brush buffer.
struct BrushSample {
    std::int32_t offsetRow = 0;
    std::int32_t offsetColumn = 0;
    float weight = 0.0f;
};

/// What a node lets the engine interrupt and report through.
struct EvaluationHooks {
    std::function<bool()> isCancelled;
    std::function<void(float)> reportProgress;
};

/**
 * One thermal run.
 *
 * `sediment` set turns this into the Deposit node: `amountPerIteration` is
 * released over both grids each pass and only the sediment is allowed to
 * move, so the rock underneath stays where it is.
 */
struct ThermalJob {
    float* surface = nullptr;
    float* sediment = nullptr;
    std::size_t count = 0;
    int size = 0;
    int iterations = 0;
    float talusDropMeters = 0.0f;
    float rate = 0.5f;
    float amountPerIteration = 0.0f;
};

/**
 * One hydraulic run.
 *
 * `surface` arrives in cell units and is scratch: the node reads its answer out
 * of `wear` and `deposit`, whose difference is the whole change.
 */
struct HydraulicJob {
    float* surface = nullptr;
    std::int32_t* delta = nullptr;
    std::int32_t* wear = nullptr;
    std::int32_t* deposit = nullptr;
    std::int32_t* flow = nullptr;
    const BrushSample* brush = nullptr;
    std::size_t count = 0;
    std::int32_t brushSampleCount = 0;
    std::int64_t dropletCount = 0;
    std::int64_t batchSize = 0;
    std::uint64_t seed = 0;
    int size = 0;
    int lifetime = 0;
    float water = 1.0f;
    float inertia = 0.05f;
    float capacityFactor = 4.0f;
    float minimumSlope = 0.01f;
    float erosionRate = 0.3f;
    float depositionRate = 0.3f;
    float evaporation = 0.02f;
    float gravity = 10.0f;
};

/**
 * Runs the job on `backend`, which must not be `Cpu`.
 *
 * False means the device could not be driven — never that it produced a
 * different answer — and the caller runs the CPU path instead. A cancelled run
 * also returns false, and the node checks cancellation itself before using
 * anything.
 */
bool runThermalOnDevice(ErosionBackend backend, const ThermalJob& job,
                        const EvaluationHooks& hooks);
bool runHydraulicOnDevice(ErosionBackend backend, const HydraulicJob& job,
                          const EvaluationHooks& hooks);

}  // namespace terrain::gpu
