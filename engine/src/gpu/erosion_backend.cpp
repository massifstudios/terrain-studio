#include "erosion_backend.h"

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <mutex>

#include "denormal_mode.h"

#if defined(TERRAIN_WITH_VULKAN)
#include "float_parity.h"
#endif

namespace terrain::gpu {

#if defined(TERRAIN_WITH_VULKAN)
bool runThermalOnVulkan(const ThermalJob& job, const EvaluationHooks& hooks);
bool runHydraulicOnVulkan(const HydraulicJob& job, const EvaluationHooks& hooks);
#endif

#if defined(TERRAIN_WITH_CUDA)
bool cudaDeviceIsPresent();
bool runThermalOnCuda(const ThermalJob& job, const EvaluationHooks& hooks);
bool runHydraulicOnCuda(const HydraulicJob& job, const EvaluationHooks& hooks);
#endif

namespace {

std::string backendExplanation;

/// An explicit choice, or `Cpu` when nothing was asked for. The tests use this
/// to drive the same graph through each backend the machine offers.
ErosionBackend requestedBackend(bool& wasRequested) {
    const char* requested = std::getenv("TERRAIN_EROSION_BACKEND");
    wasRequested = requested != nullptr;
    if (requested == nullptr) {
        return ErosionBackend::Cpu;
    }
    if (std::strcmp(requested, "vulkan") == 0) {
        return ErosionBackend::Vulkan;
    }
    if (std::strcmp(requested, "cuda") == 0) {
        return ErosionBackend::Cuda;
    }
    return ErosionBackend::Cpu;
}

bool isVulkanUsable() {
#if defined(TERRAIN_WITH_VULKAN)
    std::string detail;
    if (deviceMatchesHostArithmetic(&detail)) {
        return true;
    }
    backendExplanation = "Vulkan declined: " + detail;
    return false;
#else
    backendExplanation = "Vulkan declined: the engine was built without it";
    return false;
#endif
}

ErosionBackend vulkanOrCpu() {
    if (isVulkanUsable()) {
        return ErosionBackend::Vulkan;
    }
    return ErosionBackend::Cpu;
}

bool isCudaUsable() {
#if defined(TERRAIN_WITH_CUDA)
    if (cudaDeviceIsPresent()) {
        return true;
    }
    backendExplanation = "CUDA declined: no device is present";
    return false;
#else
    backendExplanation = "CUDA declined: the engine was built without it";
    return false;
#endif
}

bool isBackendUsable(ErosionBackend backend) {
    if (backend == ErosionBackend::Cpu) {
        return true;
    }
    if (!FlushDenormalsToZero::isSupported()) {
        return false;
    }
    if (backend == ErosionBackend::Cuda) {
        return isCudaUsable();
    }
    return isVulkanUsable();
}

ErosionBackend chooseBackend() {
    bool wasRequested = false;
    const ErosionBackend requested = requestedBackend(wasRequested);
    if (wasRequested && requested == ErosionBackend::Cpu) {
        return ErosionBackend::Cpu;
    }
    if (!FlushDenormalsToZero::isSupported()) {
        backendExplanation =
            "the GPU was declined: this platform cannot be put into flush-to-zero, and a "
            "device that flushes denormals would then disagree with the CPU";
        return ErosionBackend::Cpu;
    }
    if (wasRequested && requested == ErosionBackend::Vulkan) {
        return vulkanOrCpu();
    }

    // CUDA first where it exists: on the hardware that has it, it is the
    // faster of the two, and both are held to the same bit-for-bit result.
    if (isCudaUsable()) {
        return ErosionBackend::Cuda;
    }
    if (wasRequested && requested == ErosionBackend::Cuda) {
        return ErosionBackend::Cpu;
    }
    return vulkanOrCpu();
}

}  // namespace

const char* nameOf(ErosionBackend backend) {
    if (backend == ErosionBackend::Vulkan) {
        return "Vulkan";
    }
    if (backend == ErosionBackend::Cuda) {
        return "CUDA";
    }
    return "CPU";
}

namespace {

/// Set once from `chooseBackend`, and afterwards only by `forceErosionBackend`.
std::atomic<ErosionBackend> activeBackend{ErosionBackend::Cpu};
std::once_flag backendChosen;

}  // namespace

ErosionBackend selectedErosionBackend() {
    std::call_once(backendChosen, [] { activeBackend.store(chooseBackend()); });
    return activeBackend.load();
}

bool forceErosionBackend(ErosionBackend backend) {
    // Whether this backend can run, not whether it is the one that would have
    // been picked: on a machine with both, asking for Vulkan must not be
    // refused merely because CUDA is preferred.
    selectedErosionBackend();
    if (!isBackendUsable(backend)) {
        return false;
    }
    activeBackend.store(backend);
    return true;
}

const std::string& erosionBackendExplanation() {
    selectedErosionBackend();
    return backendExplanation;
}

bool runThermalOnDevice(ErosionBackend backend, const ThermalJob& job,
                        const EvaluationHooks& hooks) {
#if defined(TERRAIN_WITH_VULKAN)
    if (backend == ErosionBackend::Vulkan) {
        return runThermalOnVulkan(job, hooks);
    }
#endif
#if defined(TERRAIN_WITH_CUDA)
    if (backend == ErosionBackend::Cuda) {
        return runThermalOnCuda(job, hooks);
    }
#endif
    return false;
}

bool runHydraulicOnDevice(ErosionBackend backend, const HydraulicJob& job,
                          const EvaluationHooks& hooks) {
#if defined(TERRAIN_WITH_VULKAN)
    if (backend == ErosionBackend::Vulkan) {
        return runHydraulicOnVulkan(job, hooks);
    }
#endif
#if defined(TERRAIN_WITH_CUDA)
    if (backend == ErosionBackend::Cuda) {
        return runHydraulicOnCuda(job, hooks);
    }
#endif
    return false;
}

}  // namespace terrain::gpu
