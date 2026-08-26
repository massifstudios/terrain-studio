#include "vulkan_device.h"

#include <algorithm>
#include <cstring>

namespace terrain::gpu {
namespace {

/// Enough for every pipeline and binding erosion builds in one process. The
/// pool is not reset between evaluations because a binding outlives a batch.
constexpr std::uint32_t descriptorSetBudget = 64;
constexpr std::uint32_t storageBufferBudget = 512;

constexpr std::uint64_t fenceTimeoutNanoseconds = 60ull * 1000ull * 1000ull * 1000ull;

bool isSuccess(VkResult result) {
    return result == VK_SUCCESS;
}

/// A device is only a candidate if it can be pinned to round-to-nearest-even
/// for 32-bit floats and asked to leave signed zero, infinity and NaN alone.
/// Denormal behaviour is not in the test: no NVIDIA device offers control over
/// it, and the erosion backend measures the device's actual arithmetic against
/// the CPU before it is used for anything.
bool hasUsableFloatControls(const VkPhysicalDeviceFloatControlsProperties& controls) {
    if (controls.shaderRoundingModeRTEFloat32 != VK_TRUE) {
        return false;
    }
    return controls.shaderSignedZeroInfNanPreserveFloat32 == VK_TRUE;
}

int preferenceForType(VkPhysicalDeviceType type) {
    if (type == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        return 3;
    }
    if (type == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
        return 2;
    }
    if (type == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU) {
        return 1;
    }
    return 0;
}

}  // namespace

/* ================================================================= buffer == */

VulkanBuffer::~VulkanBuffer() {
    release();
}

VulkanBuffer::VulkanBuffer(VulkanBuffer&& other) noexcept {
    takeFrom(other);
}

VulkanBuffer& VulkanBuffer::operator=(VulkanBuffer&& other) noexcept {
    if (this == &other) {
        return *this;
    }
    release();
    takeFrom(other);
    return *this;
}

void VulkanBuffer::takeFrom(VulkanBuffer& other) noexcept {
    device_ = other.device_;
    handle_ = other.handle_;
    memory_ = other.memory_;
    sizeBytes_ = other.sizeBytes_;
    mapped_ = other.mapped_;
    isCoherent_ = other.isCoherent_;
    other.device_ = nullptr;
    other.handle_ = VK_NULL_HANDLE;
    other.memory_ = VK_NULL_HANDLE;
    other.sizeBytes_ = 0;
    other.mapped_ = nullptr;
}

void VulkanBuffer::release() {
    if (device_ == nullptr) {
        return;
    }
    const VulkanDeviceApi& api = device_->deviceApi_;
    if (mapped_ != nullptr) {
        api.vkUnmapMemory(device_->device_, memory_);
    }
    api.vkDestroyBuffer(device_->device_, handle_, nullptr);
    api.vkFreeMemory(device_->device_, memory_, nullptr);
    device_ = nullptr;
    handle_ = VK_NULL_HANDLE;
    memory_ = VK_NULL_HANDLE;
    mapped_ = nullptr;
}

void VulkanBuffer::flush() {
    if (device_ == nullptr || isCoherent_) {
        return;
    }
    VkMappedMemoryRange range{};
    range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    range.memory = memory_;
    range.size = VK_WHOLE_SIZE;
    device_->deviceApi_.vkFlushMappedMemoryRanges(device_->device_, 1, &range);
}

void VulkanBuffer::invalidate() {
    if (device_ == nullptr || isCoherent_) {
        return;
    }
    VkMappedMemoryRange range{};
    range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    range.memory = memory_;
    range.size = VK_WHOLE_SIZE;
    device_->deviceApi_.vkInvalidateMappedMemoryRanges(device_->device_, 1, &range);
}

/* =============================================================== pipeline == */

VulkanComputePipeline::~VulkanComputePipeline() {
    release();
}

VulkanComputePipeline::VulkanComputePipeline(VulkanComputePipeline&& other) noexcept {
    takeFrom(other);
}

VulkanComputePipeline& VulkanComputePipeline::operator=(VulkanComputePipeline&& other) noexcept {
    if (this == &other) {
        return *this;
    }
    release();
    takeFrom(other);
    return *this;
}

void VulkanComputePipeline::takeFrom(VulkanComputePipeline& other) noexcept {
    device_ = other.device_;
    descriptorLayout_ = other.descriptorLayout_;
    pipelineLayout_ = other.pipelineLayout_;
    pipeline_ = other.pipeline_;
    bufferCount_ = other.bufferCount_;
    pushConstantBytes_ = other.pushConstantBytes_;
    other.device_ = nullptr;
    other.descriptorLayout_ = VK_NULL_HANDLE;
    other.pipelineLayout_ = VK_NULL_HANDLE;
    other.pipeline_ = VK_NULL_HANDLE;
}

void VulkanComputePipeline::release() {
    if (device_ == nullptr) {
        return;
    }
    const VulkanDeviceApi& api = device_->deviceApi_;
    api.vkDestroyPipeline(device_->device_, pipeline_, nullptr);
    api.vkDestroyPipelineLayout(device_->device_, pipelineLayout_, nullptr);
    api.vkDestroyDescriptorSetLayout(device_->device_, descriptorLayout_, nullptr);
    device_ = nullptr;
    pipeline_ = VK_NULL_HANDLE;
    pipelineLayout_ = VK_NULL_HANDLE;
    descriptorLayout_ = VK_NULL_HANDLE;
}

/* ================================================================ binding == */

VulkanBinding::~VulkanBinding() {
    release();
}

VulkanBinding::VulkanBinding(VulkanBinding&& other) noexcept {
    takeFrom(other);
}

VulkanBinding& VulkanBinding::operator=(VulkanBinding&& other) noexcept {
    if (this == &other) {
        return *this;
    }
    release();
    takeFrom(other);
    return *this;
}

void VulkanBinding::takeFrom(VulkanBinding& other) noexcept {
    device_ = other.device_;
    pipeline_ = other.pipeline_;
    descriptorSet_ = other.descriptorSet_;
    other.device_ = nullptr;
    other.pipeline_ = nullptr;
    other.descriptorSet_ = VK_NULL_HANDLE;
}

void VulkanBinding::release() {
    if (device_ == nullptr) {
        return;
    }
    device_->deviceApi_.vkFreeDescriptorSets(device_->device_, device_->descriptorPool_, 1,
                                             &descriptorSet_);
    device_ = nullptr;
    pipeline_ = nullptr;
    descriptorSet_ = VK_NULL_HANDLE;
}

/* ================================================================= device == */

VulkanDevice::~VulkanDevice() {
    if (device_ != VK_NULL_HANDLE) {
        deviceApi_.vkDestroyDescriptorPool(device_, descriptorPool_, nullptr);
        deviceApi_.vkDestroyCommandPool(device_, commandPool_, nullptr);
        deviceApi_.vkDestroyDevice(device_, nullptr);
    }
    if (instance_ != VK_NULL_HANDLE) {
        api_.vkDestroyInstance(instance_, nullptr);
    }
}

VulkanDevice* VulkanDevice::shared() {
    static std::unique_ptr<VulkanDevice> device = [] {
        std::unique_ptr<VulkanDevice> candidate(new VulkanDevice());
        if (!candidate->initialise()) {
            return std::unique_ptr<VulkanDevice>();
        }
        return candidate;
    }();
    return device.get();
}

bool VulkanDevice::initialise() {
    const VulkanApi* loaded = loadVulkanApi();
    if (loaded == nullptr) {
        return false;
    }
    api_ = *loaded;

    if (!createInstance()) {
        return false;
    }
    if (!selectPhysicalDevice()) {
        return false;
    }
    if (!createLogicalDevice()) {
        return false;
    }
    if (!createCommandPool()) {
        return false;
    }
    return createDescriptorPool();
}

bool VulkanDevice::createInstance() {
    VkApplicationInfo application{};
    application.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    application.pApplicationName = "Terrain Studio";
    application.apiVersion = VK_API_VERSION_1_2;

    VkInstanceCreateInfo creation{};
    creation.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    creation.pApplicationInfo = &application;

    if (!isSuccess(api_.vkCreateInstance(&creation, nullptr, &instance_))) {
        instance_ = VK_NULL_HANDLE;
        return false;
    }
    loadVulkanInstanceFunctions(api_, instance_);
    return api_.vkEnumeratePhysicalDevices != nullptr;
}

bool VulkanDevice::selectPhysicalDevice() {
    std::uint32_t deviceCount = 0;
    api_.vkEnumeratePhysicalDevices(instance_, &deviceCount, nullptr);
    std::vector<VkPhysicalDevice> devices(deviceCount);
    api_.vkEnumeratePhysicalDevices(instance_, &deviceCount, devices.data());

    int bestPreference = -1;
    for (VkPhysicalDevice candidate : devices) {
        VkPhysicalDeviceFloatControlsProperties controls{};
        controls.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FLOAT_CONTROLS_PROPERTIES;
        VkPhysicalDeviceProperties2 properties{};
        properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        properties.pNext = &controls;
        api_.vkGetPhysicalDeviceProperties2(candidate, &properties);

        const int preference = preferenceForType(properties.properties.deviceType);
        if (preference <= bestPreference || !hasUsableFloatControls(controls)) {
            continue;
        }
        std::uint32_t familyCount = 0;
        api_.vkGetPhysicalDeviceQueueFamilyProperties(candidate, &familyCount, nullptr);
        std::vector<VkQueueFamilyProperties> families(familyCount);
        api_.vkGetPhysicalDeviceQueueFamilyProperties(candidate, &familyCount, families.data());

        for (std::uint32_t index = 0; index < familyCount; ++index) {
            if ((families[index].queueFlags & VK_QUEUE_COMPUTE_BIT) == 0) {
                continue;
            }
            bestPreference = preference;
            physicalDevice_ = candidate;
            queueFamily_ = index;
            name_ = properties.properties.deviceName;
            maxWorkgroupSize_ = properties.properties.limits.maxComputeWorkGroupInvocations;
            break;
        }
    }
    return physicalDevice_ != VK_NULL_HANDLE;
}

bool VulkanDevice::createLogicalDevice() {
    const float priority = 1.0f;
    VkDeviceQueueCreateInfo queueCreation{};
    queueCreation.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreation.queueFamilyIndex = queueFamily_;
    queueCreation.queueCount = 1;
    queueCreation.pQueuePriorities = &priority;

    VkDeviceCreateInfo creation{};
    creation.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    creation.queueCreateInfoCount = 1;
    creation.pQueueCreateInfos = &queueCreation;

    if (!isSuccess(api_.vkCreateDevice(physicalDevice_, &creation, nullptr, &device_))) {
        device_ = VK_NULL_HANDLE;
        return false;
    }
    loadVulkanDeviceFunctions(api_, device_, deviceApi_);
    deviceApi_.vkGetDeviceQueue(device_, queueFamily_, 0, &queue_);
    api_.vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &memoryProperties_);
    return queue_ != VK_NULL_HANDLE;
}

bool VulkanDevice::createCommandPool() {
    VkCommandPoolCreateInfo creation{};
    creation.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    creation.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    creation.queueFamilyIndex = queueFamily_;
    return isSuccess(deviceApi_.vkCreateCommandPool(device_, &creation, nullptr, &commandPool_));
}

bool VulkanDevice::createDescriptorPool() {
    VkDescriptorPoolSize size{};
    size.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    size.descriptorCount = storageBufferBudget;

    VkDescriptorPoolCreateInfo creation{};
    creation.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    creation.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    creation.maxSets = descriptorSetBudget;
    creation.poolSizeCount = 1;
    creation.pPoolSizes = &size;
    return isSuccess(
        deviceApi_.vkCreateDescriptorPool(device_, &creation, nullptr, &descriptorPool_));
}

std::uint32_t VulkanDevice::findMemoryType(std::uint32_t allowedTypes,
                                           VkMemoryPropertyFlags wanted, bool& isCoherent) const {
    for (std::uint32_t index = 0; index < memoryProperties_.memoryTypeCount; ++index) {
        const VkMemoryPropertyFlags flags = memoryProperties_.memoryTypes[index].propertyFlags;
        const bool isAllowed = (allowedTypes & (1u << index)) != 0;
        if (!isAllowed || (flags & wanted) != wanted) {
            continue;
        }
        isCoherent = (flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0;
        return index;
    }
    return UINT32_MAX;
}

VulkanBuffer VulkanDevice::createBuffer(VkDeviceSize sizeBytes) {
    VulkanBuffer buffer;
    VkBufferCreateInfo creation{};
    creation.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    creation.size = std::max<VkDeviceSize>(sizeBytes, 4);
    creation.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    creation.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (!isSuccess(deviceApi_.vkCreateBuffer(device_, &creation, nullptr, &buffer.handle_))) {
        return buffer;
    }

    VkMemoryRequirements requirements{};
    deviceApi_.vkGetBufferMemoryRequirements(device_, buffer.handle_, &requirements);
    bool isCoherent = false;
    std::uint32_t memoryType = findMemoryType(requirements.memoryTypeBits,
                                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                                  | VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
                                              isCoherent);
    if (memoryType == UINT32_MAX) {
        memoryType = findMemoryType(requirements.memoryTypeBits,
                                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, isCoherent);
    }
    if (!allocateAndMap(buffer, requirements, memoryType, isCoherent, creation.size)) {
        deviceApi_.vkDestroyBuffer(device_, buffer.handle_, nullptr);
        buffer.handle_ = VK_NULL_HANDLE;
    }
    return buffer;
}

bool VulkanDevice::allocateAndMap(VulkanBuffer& buffer, const VkMemoryRequirements& requirements,
                                  std::uint32_t memoryType, bool isCoherent,
                                  VkDeviceSize sizeBytes) {
    if (memoryType == UINT32_MAX) {
        return false;
    }
    VkMemoryAllocateInfo allocation{};
    allocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocation.allocationSize = requirements.size;
    allocation.memoryTypeIndex = memoryType;
    if (!isSuccess(deviceApi_.vkAllocateMemory(device_, &allocation, nullptr, &buffer.memory_))) {
        return false;
    }
    if (!isSuccess(deviceApi_.vkBindBufferMemory(device_, buffer.handle_, buffer.memory_, 0))) {
        return false;
    }
    if (!isSuccess(deviceApi_.vkMapMemory(device_, buffer.memory_, 0, VK_WHOLE_SIZE, 0,
                                          &buffer.mapped_))) {
        return false;
    }
    buffer.device_ = this;
    buffer.sizeBytes_ = sizeBytes;
    buffer.isCoherent_ = isCoherent;
    return true;
}

bool VulkanDevice::finishPipeline(VulkanComputePipeline& pipeline, const std::uint32_t* spirv,
                                  std::size_t spirvBytes) {
    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushRange.size = pipeline.pushConstantBytes_;

    VkPipelineLayoutCreateInfo layoutCreation{};
    layoutCreation.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutCreation.setLayoutCount = 1;
    layoutCreation.pSetLayouts = &pipeline.descriptorLayout_;
    layoutCreation.pushConstantRangeCount = 1;
    layoutCreation.pPushConstantRanges = &pushRange;
    if (!isSuccess(deviceApi_.vkCreatePipelineLayout(device_, &layoutCreation, nullptr,
                                                     &pipeline.pipelineLayout_))) {
        return false;
    }

    VkShaderModuleCreateInfo moduleCreation{};
    moduleCreation.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    moduleCreation.codeSize = spirvBytes;
    moduleCreation.pCode = spirv;
    VkShaderModule module = VK_NULL_HANDLE;
    if (!isSuccess(deviceApi_.vkCreateShaderModule(device_, &moduleCreation, nullptr, &module))) {
        return false;
    }

    VkComputePipelineCreateInfo creation{};
    creation.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    creation.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    creation.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    creation.stage.module = module;
    creation.stage.pName = "main";
    creation.layout = pipeline.pipelineLayout_;
    const bool created = isSuccess(deviceApi_.vkCreateComputePipelines(
        device_, VK_NULL_HANDLE, 1, &creation, nullptr, &pipeline.pipeline_));
    deviceApi_.vkDestroyShaderModule(device_, module, nullptr);
    return created;
}

bool VulkanDevice::submitAndWait(VkCommandBuffer commands) {
    VkFenceCreateInfo fenceCreation{};
    fenceCreation.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence fence = VK_NULL_HANDLE;
    if (!isSuccess(deviceApi_.vkCreateFence(device_, &fenceCreation, nullptr, &fence))) {
        return false;
    }

    VkSubmitInfo submission{};
    submission.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submission.commandBufferCount = 1;
    submission.pCommandBuffers = &commands;
    bool finished = isSuccess(deviceApi_.vkQueueSubmit(queue_, 1, &submission, fence));
    if (finished) {
        finished = isSuccess(deviceApi_.vkWaitForFences(device_, 1, &fence, VK_TRUE,
                                                        fenceTimeoutNanoseconds));
    }
    deviceApi_.vkDestroyFence(device_, fence, nullptr);
    deviceApi_.vkFreeCommandBuffers(device_, commandPool_, 1, &commands);
    return finished;
}

VulkanComputePipeline VulkanDevice::createPipeline(const std::uint32_t* spirv,
                                                   std::size_t spirvBytes,
                                                   std::uint32_t bufferCount,
                                                   std::uint32_t pushConstantBytes) {
    VulkanComputePipeline pipeline;
    std::vector<VkDescriptorSetLayoutBinding> bindings(bufferCount);
    for (std::uint32_t index = 0; index < bufferCount; ++index) {
        bindings[index].binding = index;
        bindings[index].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[index].descriptorCount = 1;
        bindings[index].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    }

    VkDescriptorSetLayoutCreateInfo layoutCreation{};
    layoutCreation.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutCreation.bindingCount = bufferCount;
    layoutCreation.pBindings = bindings.data();
    if (!isSuccess(deviceApi_.vkCreateDescriptorSetLayout(device_, &layoutCreation, nullptr,
                                                          &pipeline.descriptorLayout_))) {
        return pipeline;
    }

    pipeline.device_ = this;
    pipeline.bufferCount_ = bufferCount;
    pipeline.pushConstantBytes_ = pushConstantBytes;
    finishPipeline(pipeline, spirv, spirvBytes);
    return pipeline;
}

VulkanBinding VulkanDevice::createBinding(const VulkanComputePipeline& pipeline,
                                          const std::vector<const VulkanBuffer*>& buffers) {
    VulkanBinding binding;
    VkDescriptorSetAllocateInfo allocation{};
    allocation.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocation.descriptorPool = descriptorPool_;
    allocation.descriptorSetCount = 1;
    allocation.pSetLayouts = &pipeline.descriptorLayout_;
    if (!isSuccess(
            deviceApi_.vkAllocateDescriptorSets(device_, &allocation, &binding.descriptorSet_))) {
        binding.descriptorSet_ = VK_NULL_HANDLE;
        return binding;
    }

    std::vector<VkDescriptorBufferInfo> infos(buffers.size());
    std::vector<VkWriteDescriptorSet> writes(buffers.size());
    for (std::size_t index = 0; index < buffers.size(); ++index) {
        infos[index].buffer = buffers[index]->handle();
        infos[index].offset = 0;
        infos[index].range = VK_WHOLE_SIZE;
        writes[index].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[index].dstSet = binding.descriptorSet_;
        writes[index].dstBinding = static_cast<std::uint32_t>(index);
        writes[index].descriptorCount = 1;
        writes[index].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[index].pBufferInfo = &infos[index];
    }
    deviceApi_.vkUpdateDescriptorSets(device_, static_cast<std::uint32_t>(writes.size()),
                                      writes.data(), 0, nullptr);
    binding.device_ = this;
    binding.pipeline_ = &pipeline;
    return binding;
}

void VulkanDevice::recordDispatch(VkCommandBuffer commands, const VulkanDispatch& dispatch,
                                  bool needsBarrier) {
    if (needsBarrier) {
        VkMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        deviceApi_.vkCmdPipelineBarrier(
            commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);
    }
    if (dispatch.clearFirst != nullptr) {
        deviceApi_.vkCmdFillBuffer(commands, dispatch.clearFirst->handle(), 0, VK_WHOLE_SIZE, 0);
        recordDispatch(commands, VulkanDispatch{dispatch.binding, dispatch.pushConstants,
                                                dispatch.pushConstantBytes, dispatch.groupCount,
                                                nullptr},
                       true);
        return;
    }

    const VulkanComputePipeline& pipeline = *dispatch.binding->pipeline_;
    deviceApi_.vkCmdBindPipeline(commands, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.pipeline_);
    deviceApi_.vkCmdBindDescriptorSets(commands, VK_PIPELINE_BIND_POINT_COMPUTE,
                                       pipeline.pipelineLayout_, 0, 1,
                                       &dispatch.binding->descriptorSet_, 0, nullptr);
    if (dispatch.pushConstantBytes > 0) {
        deviceApi_.vkCmdPushConstants(commands, pipeline.pipelineLayout_,
                                      VK_SHADER_STAGE_COMPUTE_BIT, 0, dispatch.pushConstantBytes,
                                      dispatch.pushConstants);
    }
    deviceApi_.vkCmdDispatch(commands, dispatch.groupCount, 1, 1);
}

bool VulkanDevice::runDispatches(const std::vector<VulkanDispatch>& dispatches) {
    if (dispatches.empty()) {
        return true;
    }

    VkCommandBufferAllocateInfo allocation{};
    allocation.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocation.commandPool = commandPool_;
    allocation.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocation.commandBufferCount = 1;
    VkCommandBuffer commands = VK_NULL_HANDLE;
    if (!isSuccess(deviceApi_.vkAllocateCommandBuffers(device_, &allocation, &commands))) {
        return false;
    }

    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    deviceApi_.vkBeginCommandBuffer(commands, &begin);
    for (std::size_t index = 0; index < dispatches.size(); ++index) {
        recordDispatch(commands, dispatches[index], index > 0);
    }
    deviceApi_.vkEndCommandBuffer(commands);
    return submitAndWait(commands);
}

}  // namespace terrain::gpu
