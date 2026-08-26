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
    if (wasRequested && requested == ErosionBackend::Cuda) {
        backendExplanation = "CUDA was asked for but is not built into this engine";
        return ErosionBackend::Cpu;
    }
    if (isVulkanUsable()) {
        return ErosionBackend::Vulkan;
    }
    return ErosionBackend::Cpu;
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
    if (backend == ErosionBackend::Cpu) {
        selectedErosionBackend();
        activeBackend.store(ErosionBackend::Cpu);
        return true;
    }
    if (selectedErosionBackend() != backend && chooseBackend() != backend) {
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
    return false;
}

bool runHydraulicOnDevice(ErosionBackend backend, const HydraulicJob& job,
                          const EvaluationHooks& hooks) {
#if defined(TERRAIN_WITH_VULKAN)
    if (backend == ErosionBackend::Vulkan) {
        return runHydraulicOnVulkan(job, hooks);
    }
#endif
    return false;
}

}  // namespace terrain::gpu
