/*
 * Reports what each Vulkan device on this machine promises about float
 * behaviour.
 *
 * Bit-exact agreement with the CPU is only possible where the device can be
 * pinned to round-to-nearest-even, preserve denormals, and preserve signed
 * zero, infinity and NaN for 32-bit floats. This tool answers whether a given
 * machine can do that, which is the question the erosion backend asks before
 * it offers itself.
 */

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>


#include "float_parity.h"
#include "vulkan_api.h"
#include "vulkan_device.h"

namespace {

using terrain::gpu::VulkanApi;

const char* yesOrNo(VkBool32 value) {
    if (value == VK_TRUE) {
        return "yes";
    }
    return "no";
}

const char* independenceName(VkShaderFloatControlsIndependence value) {
    if (value == VK_SHADER_FLOAT_CONTROLS_INDEPENDENCE_32_BIT_ONLY) {
        return "32-bit only";
    }
    if (value == VK_SHADER_FLOAT_CONTROLS_INDEPENDENCE_ALL) {
        return "all";
    }
    return "none";
}

const char* deviceTypeName(VkPhysicalDeviceType type) {
    if (type == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        return "discrete";
    }
    if (type == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
        return "integrated";
    }
    if (type == VK_PHYSICAL_DEVICE_TYPE_CPU) {
        return "software";
    }
    return "other";
}

VkInstance createInstance(const VulkanApi& api) {
    VkApplicationInfo application{};
    application.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    application.pApplicationName = "terrain-gpu-probe";
    application.apiVersion = VK_API_VERSION_1_2;

    VkInstanceCreateInfo creation{};
    creation.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    creation.pApplicationInfo = &application;

    VkInstance instance = VK_NULL_HANDLE;
    if (api.vkCreateInstance(&creation, nullptr, &instance) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }
    return instance;
}

int computeQueueFamily(const VulkanApi& api, VkPhysicalDevice device) {
    std::uint32_t familyCount = 0;
    api.vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, nullptr);
    std::vector<VkQueueFamilyProperties> families(familyCount);
    api.vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, families.data());

    for (std::uint32_t index = 0; index < familyCount; ++index) {
        if ((families[index].queueFlags & VK_QUEUE_COMPUTE_BIT) != 0) {
            return static_cast<int>(index);
        }
    }
    return -1;
}

void reportDevice(const VulkanApi& api, VkPhysicalDevice device) {
    VkPhysicalDeviceFloatControlsProperties floatControls{};
    floatControls.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FLOAT_CONTROLS_PROPERTIES;

    VkPhysicalDeviceProperties2 properties{};
    properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    properties.pNext = &floatControls;
    api.vkGetPhysicalDeviceProperties2(device, &properties);

    VkPhysicalDeviceFeatures2 features{};
    features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    api.vkGetPhysicalDeviceFeatures2(device, &features);

    const VkPhysicalDeviceProperties& basic = properties.properties;
    std::printf("%s  [%s]  Vulkan %u.%u.%u\n", basic.deviceName, deviceTypeName(basic.deviceType),
                VK_API_VERSION_MAJOR(basic.apiVersion), VK_API_VERSION_MINOR(basic.apiVersion),
                VK_API_VERSION_PATCH(basic.apiVersion));
    std::printf("  compute queue family      %d\n", computeQueueFamily(api, device));
    std::printf("  shaderFloat64             %s\n", yesOrNo(features.features.shaderFloat64));
    std::printf("  fp32 rounding RTE         %s\n",
                yesOrNo(floatControls.shaderRoundingModeRTEFloat32));
    std::printf("  fp32 denorm preserve      %s\n",
                yesOrNo(floatControls.shaderDenormPreserveFloat32));
    std::printf("  fp32 denorm flush to zero %s\n",
                yesOrNo(floatControls.shaderDenormFlushToZeroFloat32));
    std::printf("  fp32 signed zero/inf/nan  %s\n",
                yesOrNo(floatControls.shaderSignedZeroInfNanPreserveFloat32));
    std::printf("  denorm independence       %s\n",
                independenceName(floatControls.denormBehaviorIndependence));
    std::printf("  rounding independence     %s\n",
                independenceName(floatControls.roundingModeIndependence));
    std::printf("  max workgroup invocations %u\n", basic.limits.maxComputeWorkGroupInvocations);
    std::printf("\n");
}

void reportParity(const char* label, terrain::gpu::ArithmeticRoutine routine, bool isSequential,
                  std::uint64_t firstPattern, std::uint64_t count) {
    terrain::gpu::ParityRequest request;
    request.routine = routine;
    request.isSequential = isSequential;
    request.firstPattern = firstPattern;
    request.count = count;
    request.seed = 0x9E3779B97F4A7C15ull;

    const terrain::gpu::ParityReport report = terrain::gpu::checkArithmeticParity(request);
    if (!report.ranOnDevice) {
        std::printf("  %-14s could not be run\n", label);
        return;
    }
    std::printf("  %-14s %llu tested, %llu disagreed", label,
                static_cast<unsigned long long>(report.testedCount),
                static_cast<unsigned long long>(report.mismatchCount));
    if (report.mismatchCount > 0) {
        std::printf("  (first: input %08x, host %08x, device %08x)", report.firstMismatchPattern,
                    report.hostResult, report.deviceResult);
    }
    std::printf("\n");
}

}  // namespace

int main(int argumentCount, char** arguments) {
    const VulkanApi* loaded = terrain::gpu::loadVulkanApi();
    if (loaded == nullptr) {
        std::printf("no Vulkan loader on this machine\n");
        return 0;
    }

    VulkanApi api = *loaded;
    VkInstance instance = createInstance(api);
    if (instance == VK_NULL_HANDLE) {
        std::printf("Vulkan loader present, but no instance could be created\n");
        return 0;
    }
    terrain::gpu::loadVulkanInstanceFunctions(api, instance);

    std::uint32_t deviceCount = 0;
    api.vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    std::vector<VkPhysicalDevice> devices(deviceCount);
    api.vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
    std::printf("%u Vulkan device(s)\n\n", deviceCount);

    for (VkPhysicalDevice device : devices) {
        reportDevice(api, device);
    }

    api.vkDestroyInstance(instance, nullptr);

    terrain::gpu::VulkanDevice* chosen = terrain::gpu::VulkanDevice::shared();
    if (chosen == nullptr) {
        std::printf("no device can be pinned to round-to-nearest-even; erosion stays on the CPU\n");
        return 0;
    }
    std::printf("chosen for compute: %s\n\n", chosen->name().c_str());


    const bool isExhaustive = argumentCount > 1 && std::string(arguments[1]) == "--exhaustive";
    if (!isExhaustive) {
        std::printf("arithmetic parity against the host (sampled; pass --exhaustive to sweep)\n");
        reportParity("reciprocal", terrain::gpu::ArithmeticRoutine::Reciprocal, false, 0,
                     1u << 22);
        reportParity("square root", terrain::gpu::ArithmeticRoutine::SquareRoot, false, 0,
                     1u << 22);
        reportParity("divide", terrain::gpu::ArithmeticRoutine::Divide, false, 0, 1u << 22);
        reportParity("round to int", terrain::gpu::ArithmeticRoutine::RoundToInt, false, 0,
                     1u << 22);
        return 0;
    }

    std::printf("arithmetic parity against the host (every 32-bit pattern)\n");
    reportParity("reciprocal", terrain::gpu::ArithmeticRoutine::Reciprocal, true, 0, 1ull << 32);
    reportParity("square root", terrain::gpu::ArithmeticRoutine::SquareRoot, true, 0, 1ull << 32);
    reportParity("round to int", terrain::gpu::ArithmeticRoutine::RoundToInt, true, 0, 1ull << 32);
    reportParity("divide", terrain::gpu::ArithmeticRoutine::Divide, false, 0, 1ull << 28);
    return 0;
}
