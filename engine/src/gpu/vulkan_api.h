#pragma once

#include <vulkan/vulkan.h>

namespace terrain::gpu {

/*
 * Vulkan resolved at runtime rather than linked against.
 *
 * The engine must run on a machine with no Vulkan loader at all — erosion
 * falls back to the CPU there — so a link-time dependency would be wrong: it
 * would refuse to start rather than refuse to accelerate. Every entry point
 * below is fetched through `vkGetInstanceProcAddr`, and a missing library
 * simply leaves the table unloaded.
 */

#define TERRAIN_VULKAN_LOADER_FUNCTIONS(X) \
    X(vkCreateInstance)                    \
    X(vkEnumerateInstanceVersion)          \
    X(vkEnumerateInstanceExtensionProperties)

#define TERRAIN_VULKAN_INSTANCE_FUNCTIONS(X)   \
    X(vkDestroyInstance)                       \
    X(vkEnumeratePhysicalDevices)              \
    X(vkGetPhysicalDeviceProperties2)          \
    X(vkGetPhysicalDeviceFeatures2)            \
    X(vkGetPhysicalDeviceQueueFamilyProperties)\
    X(vkGetPhysicalDeviceMemoryProperties)     \
    X(vkEnumerateDeviceExtensionProperties)    \
    X(vkCreateDevice)                          \
    X(vkGetDeviceProcAddr)

#define TERRAIN_VULKAN_DEVICE_FUNCTIONS(X) \
    X(vkDestroyDevice)                     \
    X(vkGetDeviceQueue)                    \
    X(vkCreateBuffer)                      \
    X(vkDestroyBuffer)                     \
    X(vkGetBufferMemoryRequirements)       \
    X(vkAllocateMemory)                    \
    X(vkFreeMemory)                        \
    X(vkBindBufferMemory)                  \
    X(vkMapMemory)                         \
    X(vkUnmapMemory)                       \
    X(vkFlushMappedMemoryRanges)           \
    X(vkInvalidateMappedMemoryRanges)      \
    X(vkCreateShaderModule)                \
    X(vkDestroyShaderModule)               \
    X(vkCreateDescriptorSetLayout)         \
    X(vkDestroyDescriptorSetLayout)        \
    X(vkCreatePipelineLayout)              \
    X(vkDestroyPipelineLayout)             \
    X(vkCreateComputePipelines)            \
    X(vkDestroyPipeline)                   \
    X(vkCreateDescriptorPool)              \
    X(vkDestroyDescriptorPool)             \
    X(vkAllocateDescriptorSets)            \
    X(vkFreeDescriptorSets)                \
    X(vkResetDescriptorPool)               \
    X(vkUpdateDescriptorSets)              \
    X(vkCreateCommandPool)                 \
    X(vkDestroyCommandPool)                \
    X(vkAllocateCommandBuffers)            \
    X(vkFreeCommandBuffers)                \
    X(vkBeginCommandBuffer)                \
    X(vkEndCommandBuffer)                  \
    X(vkResetCommandPool)                  \
    X(vkCmdBindPipeline)                   \
    X(vkCmdBindDescriptorSets)             \
    X(vkCmdPushConstants)                  \
    X(vkCmdDispatch)                       \
    X(vkCmdPipelineBarrier)                \
    X(vkCmdFillBuffer)                     \
    X(vkCmdCopyBuffer)                     \
    X(vkQueueSubmit)                       \
    X(vkCreateFence)                       \
    X(vkDestroyFence)                      \
    X(vkWaitForFences)                     \
    X(vkResetFences)

#define TERRAIN_VULKAN_DECLARE(name) PFN_##name name = nullptr;

/// The loader-level and instance-level entry points, shared by every device.
struct VulkanApi {
    PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = nullptr;
    TERRAIN_VULKAN_LOADER_FUNCTIONS(TERRAIN_VULKAN_DECLARE)
    TERRAIN_VULKAN_INSTANCE_FUNCTIONS(TERRAIN_VULKAN_DECLARE)
};

/// The device-level entry points, fetched per logical device so the loader's
/// dispatch trampoline is bypassed.
struct VulkanDeviceApi {
    TERRAIN_VULKAN_DEVICE_FUNCTIONS(TERRAIN_VULKAN_DECLARE)
};

#undef TERRAIN_VULKAN_DECLARE

/**
 * Opens the system Vulkan loader and resolves the loader-level entry points.
 *
 * Returns null when no loader is installed, which is not an error: the caller
 * runs on the CPU instead. The library stays open for the lifetime of the
 * process and the returned table is shared.
 */
const VulkanApi* loadVulkanApi();

/// Resolves the instance-level entry points into `api`. Call once per instance.
void loadVulkanInstanceFunctions(VulkanApi& api, VkInstance instance);

/// Resolves the device-level entry points for `device`.
void loadVulkanDeviceFunctions(const VulkanApi& api, VkDevice device, VulkanDeviceApi& deviceApi);

}  // namespace terrain::gpu
