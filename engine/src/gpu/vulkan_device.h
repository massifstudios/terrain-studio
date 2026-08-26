#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "vulkan_api.h"

namespace terrain::gpu {

class VulkanDevice;

/**
 * A block of device memory with a host-visible mapping.
 *
 * Erosion buffers are read and written by the host between dispatches, and
 * they are small enough (a few grids of floats) that the copy a device-local
 * buffer would save is not worth the staging machinery. One allocation, one
 * persistent mapping.
 */
class VulkanBuffer {
public:
    VulkanBuffer() = default;
    ~VulkanBuffer();

    VulkanBuffer(VulkanBuffer&& other) noexcept;
    VulkanBuffer& operator=(VulkanBuffer&& other) noexcept;
    VulkanBuffer(const VulkanBuffer&) = delete;
    VulkanBuffer& operator=(const VulkanBuffer&) = delete;

    bool isValid() const { return handle_ != VK_NULL_HANDLE; }
    VkBuffer handle() const { return handle_; }
    VkDeviceSize sizeBytes() const { return sizeBytes_; }

    /// The persistent host mapping. Writes are visible to the device after
    /// `flush()`; device writes are visible to the host after `invalidate()`.
    void* mapped() const { return mapped_; }

    void flush();
    void invalidate();

private:
    friend class VulkanDevice;

    VulkanDevice* device_ = nullptr;
    VkBuffer handle_ = VK_NULL_HANDLE;
    VkDeviceMemory memory_ = VK_NULL_HANDLE;
    VkDeviceSize sizeBytes_ = 0;
    void* mapped_ = nullptr;
    bool isCoherent_ = false;

    void release();
    void takeFrom(VulkanBuffer& other) noexcept;
};

/**
 * One compute shader, its descriptor layout, and its pipeline.
 *
 * Every erosion shader has the same shape — some number of storage buffers at
 * binding 0..n-1 in set 0, plus a push constant block — so the layout is
 * described by two numbers rather than by a builder.
 */
class VulkanComputePipeline {
public:
    VulkanComputePipeline() = default;
    ~VulkanComputePipeline();

    VulkanComputePipeline(VulkanComputePipeline&& other) noexcept;
    VulkanComputePipeline& operator=(VulkanComputePipeline&& other) noexcept;
    VulkanComputePipeline(const VulkanComputePipeline&) = delete;
    VulkanComputePipeline& operator=(const VulkanComputePipeline&) = delete;

    bool isValid() const { return pipeline_ != VK_NULL_HANDLE; }

private:
    friend class VulkanDevice;

    VulkanDevice* device_ = nullptr;
    VkDescriptorSetLayout descriptorLayout_ = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
    std::uint32_t bufferCount_ = 0;
    std::uint32_t pushConstantBytes_ = 0;

    void release();
    void takeFrom(VulkanComputePipeline& other) noexcept;
};

/**
 * A pipeline bound to a particular set of buffers.
 *
 * Erosion runs the same shader over the same buffers hundreds of times, so the
 * descriptor set is built once and reused. Rebuilding it per dispatch would
 * mean a descriptor pool large enough for every iteration of a thousand-step
 * thermal run.
 */
class VulkanBinding {
public:
    VulkanBinding() = default;
    ~VulkanBinding();

    VulkanBinding(VulkanBinding&& other) noexcept;
    VulkanBinding& operator=(VulkanBinding&& other) noexcept;
    VulkanBinding(const VulkanBinding&) = delete;
    VulkanBinding& operator=(const VulkanBinding&) = delete;

    bool isValid() const { return descriptorSet_ != VK_NULL_HANDLE; }

private:
    friend class VulkanDevice;

    VulkanDevice* device_ = nullptr;
    const VulkanComputePipeline* pipeline_ = nullptr;
    VkDescriptorSet descriptorSet_ = VK_NULL_HANDLE;

    void release();
    void takeFrom(VulkanBinding& other) noexcept;
};

/// One dispatch in a recorded batch.
struct VulkanDispatch {
    const VulkanBinding* binding = nullptr;
    const void* pushConstants = nullptr;
    std::uint32_t pushConstantBytes = 0;
    std::uint32_t groupCount = 0;
    /// Zeroes this buffer before the dispatch, in place of a clear shader.
    const VulkanBuffer* clearFirst = nullptr;
};

/**
 * A compute-only Vulkan device, chosen for bit-exact float behaviour rather
 * than for speed.
 *
 * The device is created with round-to-nearest-even pinned for 32-bit floats
 * and signed zero, infinity and NaN preserved, because the erosion result has
 * to match the CPU's bit for bit. Denormal handling cannot be pinned on every
 * vendor, so it is measured instead — see `erosion_parity` — and a device that
 * disagrees with the CPU is never used.
 *
 * There is one device per process and one lock around it. Nodes in a wave
 * evaluate in parallel, so without the lock two erosion nodes would interleave
 * their dispatches; serialising them costs nothing, since the GPU was the
 * bottleneck either way.
 */
class VulkanDevice {
public:
    ~VulkanDevice();

    VulkanDevice(const VulkanDevice&) = delete;
    VulkanDevice& operator=(const VulkanDevice&) = delete;

    /// The process-wide device, or null when this machine has no Vulkan
    /// loader, no compute device, or no device that can be pinned to
    /// round-to-nearest-even.
    static VulkanDevice* shared();

    const std::string& name() const { return name_; }
    std::mutex& mutex() { return mutex_; }
    std::uint32_t maxWorkgroupSize() const { return maxWorkgroupSize_; }

    VulkanBuffer createBuffer(VkDeviceSize sizeBytes);
    VulkanComputePipeline createPipeline(const std::uint32_t* spirv, std::size_t spirvBytes,
                                         std::uint32_t bufferCount,
                                         std::uint32_t pushConstantBytes);
    VulkanBinding createBinding(const VulkanComputePipeline& pipeline,
                                const std::vector<const VulkanBuffer*>& buffers);

    /// Records every dispatch into one command buffer, with a barrier between
    /// neighbours, submits it, and returns once the queue is idle.
    bool runDispatches(const std::vector<VulkanDispatch>& dispatches);

private:
    friend class VulkanBuffer;
    friend class VulkanComputePipeline;
    friend class VulkanBinding;

    VulkanDevice() = default;

    bool initialise();
    bool createInstance();
    bool selectPhysicalDevice();
    bool createLogicalDevice();
    bool createCommandPool();
    bool createDescriptorPool();
    std::uint32_t findMemoryType(std::uint32_t allowedTypes, VkMemoryPropertyFlags wanted,
                                 bool& isCoherent) const;
    bool allocateAndMap(VulkanBuffer& buffer, const VkMemoryRequirements& requirements,
                        std::uint32_t memoryType, bool isCoherent, VkDeviceSize sizeBytes);
    bool finishPipeline(VulkanComputePipeline& pipeline, const std::uint32_t* spirv,
                        std::size_t spirvBytes);
    bool submitAndWait(VkCommandBuffer commands);
    void recordDispatch(VkCommandBuffer commands, const VulkanDispatch& dispatch,
                        bool needsBarrier);

    VulkanApi api_{};
    VulkanDeviceApi deviceApi_{};
    VkInstance instance_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkQueue queue_ = VK_NULL_HANDLE;
    VkCommandPool commandPool_ = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
    VkPhysicalDeviceMemoryProperties memoryProperties_{};
    std::uint32_t queueFamily_ = 0;
    std::uint32_t maxWorkgroupSize_ = 0;
    std::string name_;
    std::mutex mutex_;
};

}  // namespace terrain::gpu
