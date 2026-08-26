#include <algorithm>
#include <cstring>
#include <vector>

#include "add_constant_spv.h"
#include "apply_delta_spv.h"
#include "erosion_backend.h"
#include "hydraulic_droplet_spv.h"
#include "thermal_scatter_spv.h"
#include "vulkan_device.h"

namespace terrain::gpu {
namespace {

constexpr std::uint32_t workgroupSize = 64;

/// How many iterations, or rainfall batches, go into one submission. Small
/// enough that a cancelled preview stops promptly, large enough that the
/// per-submit cost disappears against the work.
constexpr int iterationsPerSubmission = 16;

struct ThermalSettings {
    std::int32_t size = 0;
    float talusDropMeters = 0.0f;
    float rate = 0.0f;
    std::int32_t hasAvailable = 0;
};

struct ApplyDeltaSettings {
    std::int32_t count = 0;
};

struct AddConstantSettings {
    std::int32_t count = 0;
    float amount = 0.0f;
};

struct HydraulicSettings {
    std::int32_t size = 0;
    std::int32_t lifetime = 0;
    float water = 0.0f;
    float inertia = 0.0f;
    float capacityFactor = 0.0f;
    float minimumSlope = 0.0f;
    float erosionRate = 0.0f;
    float depositionRate = 0.0f;
    float evaporation = 0.0f;
    float gravity = 0.0f;
    std::int32_t brushSampleCount = 0;
    std::uint32_t seedLow = 0;
    std::uint32_t seedHigh = 0;
    std::uint32_t firstDropletLow = 0;
    std::uint32_t firstDropletHigh = 0;
    std::uint32_t dropletCount = 0;
};

std::uint32_t groupsFor(std::uint64_t items) {
    return static_cast<std::uint32_t>((items + workgroupSize - 1) / workgroupSize);
}

/// Built once per process. Compiling four modules on every parameter drag
/// would cost more than the dispatches they describe.
struct ErosionPipelines {
    VulkanComputePipeline thermalScatter;
    VulkanComputePipeline applyDelta;
    VulkanComputePipeline addConstant;
    VulkanComputePipeline hydraulicDroplet;

    bool isValid() const {
        return thermalScatter.isValid() && applyDelta.isValid() && addConstant.isValid()
               && hydraulicDroplet.isValid();
    }
};

ErosionPipelines buildPipelines(VulkanDevice& device) {
    ErosionPipelines pipelines;
    pipelines.thermalScatter = device.createPipeline(
        shaders::thermalScatter, sizeof(shaders::thermalScatter), 3, sizeof(ThermalSettings));
    pipelines.applyDelta = device.createPipeline(shaders::applyDelta, sizeof(shaders::applyDelta),
                                                 2, sizeof(ApplyDeltaSettings));
    pipelines.addConstant = device.createPipeline(
        shaders::addConstant, sizeof(shaders::addConstant), 1, sizeof(AddConstantSettings));
    pipelines.hydraulicDroplet =
        device.createPipeline(shaders::hydraulicDroplet, sizeof(shaders::hydraulicDroplet), 6,
                              sizeof(HydraulicSettings));
    return pipelines;
}

const ErosionPipelines* pipelinesFor(VulkanDevice& device) {
    static ErosionPipelines pipelines = buildPipelines(device);
    if (!pipelines.isValid()) {
        return nullptr;
    }
    return &pipelines;
}

void uploadFloats(VulkanBuffer& buffer, const float* source, std::size_t count) {
    std::memcpy(buffer.mapped(), source, count * sizeof(float));
    buffer.flush();
}

void downloadFloats(VulkanBuffer& buffer, float* destination, std::size_t count) {
    buffer.invalidate();
    std::memcpy(destination, buffer.mapped(), count * sizeof(float));
}

void downloadIntegers(VulkanBuffer& buffer, std::int32_t* destination, std::size_t count) {
    buffer.invalidate();
    std::memcpy(destination, buffer.mapped(), count * sizeof(std::int32_t));
}

bool wasInterrupted(const EvaluationHooks& hooks) {
    return hooks.isCancelled && hooks.isCancelled();
}

void reportProgress(const EvaluationHooks& hooks, float fraction) {
    if (hooks.reportProgress) {
        hooks.reportProgress(fraction);
    }
}

/* =============================================================== thermal == */

/// The buffers and bindings one thermal run reuses across every iteration.
class ThermalRun {
public:
    ThermalRun(VulkanDevice& device, const ErosionPipelines& pipelines)
        : device_(device), pipelines_(pipelines) {}

    bool prepare(const ThermalJob& job);
    bool execute(const ThermalJob& job, const EvaluationHooks& hooks);

private:
    void recordIteration(const ThermalJob& job, std::vector<VulkanDispatch>& dispatches);

    VulkanDevice& device_;
    const ErosionPipelines& pipelines_;
    VulkanBuffer surface_;
    VulkanBuffer sediment_;
    VulkanBuffer delta_;
    VulkanBinding scatterBinding_;
    VulkanBinding applySurfaceBinding_;
    VulkanBinding applySedimentBinding_;
    VulkanBinding addToSurfaceBinding_;
    VulkanBinding addToSedimentBinding_;
    ThermalSettings scatterSettings_;
    ApplyDeltaSettings applySettings_;
    AddConstantSettings addSettings_;
};

bool ThermalRun::prepare(const ThermalJob& job) {
    const VkDeviceSize floatBytes = job.count * sizeof(float);
    surface_ = device_.createBuffer(floatBytes);
    sediment_ = device_.createBuffer(floatBytes);
    delta_ = device_.createBuffer(job.count * sizeof(std::int32_t));
    if (!surface_.isValid() || !sediment_.isValid() || !delta_.isValid()) {
        return false;
    }

    uploadFloats(surface_, job.surface, job.count);
    if (job.sediment != nullptr) {
        uploadFloats(sediment_, job.sediment, job.count);
    }

    scatterBinding_ = device_.createBinding(pipelines_.thermalScatter,
                                            {&surface_, &sediment_, &delta_});
    applySurfaceBinding_ = device_.createBinding(pipelines_.applyDelta, {&surface_, &delta_});
    applySedimentBinding_ = device_.createBinding(pipelines_.applyDelta, {&sediment_, &delta_});
    addToSurfaceBinding_ = device_.createBinding(pipelines_.addConstant, {&surface_});
    addToSedimentBinding_ = device_.createBinding(pipelines_.addConstant, {&sediment_});
    if (!scatterBinding_.isValid() || !applySurfaceBinding_.isValid()) {
        return false;
    }

    scatterSettings_.size = job.size;
    scatterSettings_.talusDropMeters = job.talusDropMeters;
    scatterSettings_.rate = job.rate;
    scatterSettings_.hasAvailable = static_cast<std::int32_t>(job.sediment != nullptr);
    applySettings_.count = static_cast<std::int32_t>(job.count);
    addSettings_.count = static_cast<std::int32_t>(job.count);
    addSettings_.amount = job.amountPerIteration;
    return applySedimentBinding_.isValid() && addToSurfaceBinding_.isValid()
           && addToSedimentBinding_.isValid();
}

void ThermalRun::recordIteration(const ThermalJob& job,
                                 std::vector<VulkanDispatch>& dispatches) {
    const std::uint32_t cellGroups = groupsFor(job.count);
    if (job.sediment != nullptr) {
        dispatches.push_back({&addToSurfaceBinding_, &addSettings_, sizeof(addSettings_),
                              cellGroups, nullptr});
        dispatches.push_back({&addToSedimentBinding_, &addSettings_, sizeof(addSettings_),
                              cellGroups, nullptr});
    }

    dispatches.push_back({&scatterBinding_, &scatterSettings_, sizeof(scatterSettings_),
                          cellGroups, &delta_});
    dispatches.push_back({&applySurfaceBinding_, &applySettings_, sizeof(applySettings_),
                          cellGroups, nullptr});
    if (job.sediment == nullptr) {
        return;
    }
    dispatches.push_back({&applySedimentBinding_, &applySettings_, sizeof(applySettings_),
                          cellGroups, nullptr});
}

bool ThermalRun::execute(const ThermalJob& job, const EvaluationHooks& hooks) {
    for (int done = 0; done < job.iterations; done += iterationsPerSubmission) {
        if (wasInterrupted(hooks)) {
            return false;
        }
        const int span = std::min(iterationsPerSubmission, job.iterations - done);
        std::vector<VulkanDispatch> dispatches;
        for (int iteration = 0; iteration < span; ++iteration) {
            recordIteration(job, dispatches);
        }
        if (!device_.runDispatches(dispatches)) {
            return false;
        }
        reportProgress(hooks, static_cast<float>(done + span) / job.iterations);
    }

    downloadFloats(surface_, job.surface, job.count);
    if (job.sediment != nullptr) {
        downloadFloats(sediment_, job.sediment, job.count);
    }
    return true;
}

/* ============================================================= hydraulic == */

/// The buffers and bindings one hydraulic run reuses across every batch.
class HydraulicRun {
public:
    HydraulicRun(VulkanDevice& device, const ErosionPipelines& pipelines)
        : device_(device), pipelines_(pipelines) {}

    bool prepare(const HydraulicJob& job);
    bool execute(const HydraulicJob& job, const EvaluationHooks& hooks);

private:
    VulkanDevice& device_;
    const ErosionPipelines& pipelines_;
    VulkanBuffer surface_;
    VulkanBuffer delta_;
    VulkanBuffer wear_;
    VulkanBuffer deposit_;
    VulkanBuffer flow_;
    VulkanBuffer brush_;
    VulkanBinding dropletBinding_;
    VulkanBinding applyBinding_;
    HydraulicSettings dropletSettings_;
    ApplyDeltaSettings applySettings_;
};

bool HydraulicRun::prepare(const HydraulicJob& job) {
    const VkDeviceSize cellBytes = job.count * sizeof(std::int32_t);
    surface_ = device_.createBuffer(job.count * sizeof(float));
    delta_ = device_.createBuffer(cellBytes);
    wear_ = device_.createBuffer(cellBytes);
    deposit_ = device_.createBuffer(cellBytes);
    flow_ = device_.createBuffer(cellBytes);
    brush_ = device_.createBuffer(job.brushSampleCount * sizeof(BrushSample));
    if (!surface_.isValid() || !delta_.isValid() || !brush_.isValid() || !flow_.isValid()) {
        return false;
    }

    uploadFloats(surface_, job.surface, job.count);
    std::memcpy(brush_.mapped(), job.brush, job.brushSampleCount * sizeof(BrushSample));
    brush_.flush();
    std::memset(wear_.mapped(), 0, cellBytes);
    std::memset(deposit_.mapped(), 0, cellBytes);
    std::memset(flow_.mapped(), 0, cellBytes);
    wear_.flush();
    deposit_.flush();
    flow_.flush();

    dropletBinding_ = device_.createBinding(
        pipelines_.hydraulicDroplet, {&surface_, &delta_, &wear_, &deposit_, &flow_, &brush_});
    applyBinding_ = device_.createBinding(pipelines_.applyDelta, {&surface_, &delta_});
    if (!dropletBinding_.isValid() || !applyBinding_.isValid()) {
        return false;
    }

    dropletSettings_.size = job.size;
    dropletSettings_.lifetime = job.lifetime;
    dropletSettings_.water = job.water;
    dropletSettings_.inertia = job.inertia;
    dropletSettings_.capacityFactor = job.capacityFactor;
    dropletSettings_.minimumSlope = job.minimumSlope;
    dropletSettings_.erosionRate = job.erosionRate;
    dropletSettings_.depositionRate = job.depositionRate;
    dropletSettings_.evaporation = job.evaporation;
    dropletSettings_.gravity = job.gravity;
    dropletSettings_.brushSampleCount = job.brushSampleCount;
    dropletSettings_.seedLow = static_cast<std::uint32_t>(job.seed);
    dropletSettings_.seedHigh = static_cast<std::uint32_t>(job.seed >> 32);
    applySettings_.count = static_cast<std::int32_t>(job.count);
    return true;
}

bool HydraulicRun::execute(const HydraulicJob& job, const EvaluationHooks& hooks) {
    const std::uint32_t cellGroups = groupsFor(job.count);
    // One settings block per batch has to outlive the recording, since a
    // dispatch only borrows the push constants it points at.
    std::vector<HydraulicSettings> perBatch;
    for (std::int64_t first = 0; first < job.dropletCount; first += job.batchSize) {
        HydraulicSettings settings = dropletSettings_;
        const std::int64_t last = std::min(first + job.batchSize, job.dropletCount);
        settings.firstDropletLow = static_cast<std::uint32_t>(first);
        settings.firstDropletHigh = static_cast<std::uint32_t>(first >> 32);
        settings.dropletCount = static_cast<std::uint32_t>(last - first);
        perBatch.push_back(settings);
    }

    for (std::size_t done = 0; done < perBatch.size(); done += iterationsPerSubmission) {
        if (wasInterrupted(hooks)) {
            return false;
        }
        const std::size_t span =
            std::min<std::size_t>(iterationsPerSubmission, perBatch.size() - done);
        std::vector<VulkanDispatch> dispatches;
        for (std::size_t batch = done; batch < done + span; ++batch) {
            dispatches.push_back({&dropletBinding_, &perBatch[batch], sizeof(HydraulicSettings),
                                  groupsFor(perBatch[batch].dropletCount), &delta_});
            dispatches.push_back({&applyBinding_, &applySettings_, sizeof(applySettings_),
                                  cellGroups, nullptr});
        }
        if (!device_.runDispatches(dispatches)) {
            return false;
        }
        reportProgress(hooks, static_cast<float>(done + span) / perBatch.size());
    }

    downloadIntegers(wear_, job.wear, job.count);
    downloadIntegers(deposit_, job.deposit, job.count);
    downloadIntegers(flow_, job.flow, job.count);
    return true;
}

}  // namespace

bool runThermalOnVulkan(const ThermalJob& job, const EvaluationHooks& hooks) {
    VulkanDevice* device = VulkanDevice::shared();
    if (device == nullptr) {
        return false;
    }
    std::lock_guard<std::mutex> guard(device->mutex());
    const ErosionPipelines* pipelines = pipelinesFor(*device);
    if (pipelines == nullptr) {
        return false;
    }

    ThermalRun run(*device, *pipelines);
    if (!run.prepare(job)) {
        return false;
    }
    return run.execute(job, hooks);
}

bool runHydraulicOnVulkan(const HydraulicJob& job, const EvaluationHooks& hooks) {
    VulkanDevice* device = VulkanDevice::shared();
    if (device == nullptr) {
        return false;
    }
    std::lock_guard<std::mutex> guard(device->mutex());
    const ErosionPipelines* pipelines = pipelinesFor(*device);
    if (pipelines == nullptr) {
        return false;
    }

    HydraulicRun run(*device, *pipelines);
    if (!run.prepare(job)) {
        return false;
    }
    return run.execute(job, hooks);
}

}  // namespace terrain::gpu
