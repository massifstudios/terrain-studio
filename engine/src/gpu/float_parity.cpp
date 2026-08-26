#include "float_parity.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#include "denormal_mode.h"
#include "float_parity_spv.h"
#include "terrain_node_sdk.h"
#include "vulkan_device.h"

namespace terrain::gpu {
namespace {

constexpr std::uint32_t workgroupSize = 256;
constexpr std::uint32_t chunkSize = 1u << 22;

/// How many inputs per routine the startup gate looks at. Large enough that a
/// device with a different denormal or rounding habit cannot slip through,
/// small enough to disappear into process start.
constexpr std::uint64_t startupSampleCount = 1u << 21;
constexpr std::uint64_t startupSeed = 0x9E3779B97F4A7C15ull;

struct ParitySettings {
    std::uint32_t operation = 0;
    std::uint32_t firstPattern = 0;
    std::uint32_t isSequential = 0;
    std::uint32_t count = 0;
};

float floatFromBits(std::uint32_t bits) {
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

std::uint32_t bitsFromFloat(float value) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

bool isNotANumber(std::uint32_t bits) {
    return (bits & 0x7f800000u) == 0x7f800000u && (bits & 0x007fffffu) != 0;
}

/// The host's answer, computed exactly as the CPU erosion path computes it.
std::uint32_t hostResultFor(ArithmeticRoutine routine, std::uint32_t firstBits,
                            std::uint32_t secondBits) {
    const float first = floatFromBits(firstBits);
    if (routine == ArithmeticRoutine::Reciprocal) {
        return bitsFromFloat(1.0f / first);
    }
    if (routine == ArithmeticRoutine::SquareRoot) {
        return bitsFromFloat(std::sqrt(first));
    }
    if (routine == ArithmeticRoutine::RoundToInt) {
        return static_cast<std::uint32_t>(static_cast<std::int32_t>(std::lround(first)));
    }
    return bitsFromFloat(first / floatFromBits(secondBits));
}

/**
 * Whether a disagreement on this input is one the erosion solver could ever
 * meet.
 *
 * NaN payloads are not specified by IEEE and differ between vendors, and an
 * answer that overflows or lands among the denormals is outside the domain the
 * solver works in: every division there is guarded to a normal denominator and
 * every square root to a non-negative argument. Counting those would reject
 * every real device over inputs no terrain produces.
 */
bool isOutsideErosionDomain(ArithmeticRoutine routine, std::uint32_t firstBits,
                            std::uint32_t secondBits, std::uint32_t hostBits) {
    if (isNotANumber(firstBits) || isNotANumber(secondBits) || isNotANumber(hostBits)) {
        return true;
    }
    if (routine == ArithmeticRoutine::SquareRoot && floatFromBits(firstBits) < 0.0f) {
        return true;
    }
    if (routine == ArithmeticRoutine::RoundToInt) {
        return !(std::fabs(floatFromBits(firstBits)) < 2147483648.0f);
    }
    const std::uint32_t exponent = hostBits & 0x7f800000u;
    return exponent == 0x7f800000u || exponent == 0;
}

/// Owns the pipeline and the three buffers a parity sweep reuses across chunks.
class ParityRunner {
public:
    explicit ParityRunner(VulkanDevice& device) : device_(device) {}

    bool prepare();
    bool runChunk(const ParitySettings& settings, const std::vector<std::uint32_t>& firstOperands,
                  const std::vector<std::uint32_t>& secondOperands,
                  std::vector<std::uint32_t>& results);

private:
    VulkanDevice& device_;
    VulkanComputePipeline pipeline_;
    VulkanBuffer firstBuffer_;
    VulkanBuffer secondBuffer_;
    VulkanBuffer resultBuffer_;
    VulkanBinding binding_;
};

bool ParityRunner::prepare() {
    pipeline_ = device_.createPipeline(shaders::floatParity, sizeof(shaders::floatParity), 3,
                                       sizeof(ParitySettings));
    if (!pipeline_.isValid()) {
        return false;
    }
    const VkDeviceSize bytes = static_cast<VkDeviceSize>(chunkSize) * sizeof(std::uint32_t);
    firstBuffer_ = device_.createBuffer(bytes, true);
    secondBuffer_ = device_.createBuffer(bytes, true);
    resultBuffer_ = device_.createBuffer(bytes, true);
    if (!firstBuffer_.isValid() || !secondBuffer_.isValid() || !resultBuffer_.isValid()) {
        return false;
    }
    binding_ = device_.createBinding(pipeline_, {&firstBuffer_, &secondBuffer_, &resultBuffer_});
    return binding_.isValid();
}

bool ParityRunner::runChunk(const ParitySettings& settings,
                            const std::vector<std::uint32_t>& firstOperands,
                            const std::vector<std::uint32_t>& secondOperands,
                            std::vector<std::uint32_t>& results) {
    const std::size_t bytes = static_cast<std::size_t>(settings.count) * sizeof(std::uint32_t);
    if (!firstOperands.empty()) {
        std::memcpy(firstBuffer_.mapped(), firstOperands.data(), bytes);
        firstBuffer_.flush();
    }
    if (!secondOperands.empty()) {
        std::memcpy(secondBuffer_.mapped(), secondOperands.data(), bytes);
        secondBuffer_.flush();
    }

    VulkanDispatch dispatch;
    dispatch.binding = &binding_;
    dispatch.pushConstants = &settings;
    dispatch.pushConstantBytes = sizeof(ParitySettings);
    dispatch.groupCount = (settings.count + workgroupSize - 1) / workgroupSize;
    if (!device_.runDispatches({dispatch})) {
        return false;
    }

    resultBuffer_.invalidate();
    results.resize(settings.count);
    std::memcpy(results.data(), resultBuffer_.mapped(), bytes);
    return true;
}

/// The operands for one chunk. A sweep leaves the first list empty and lets
/// the shader derive the pattern from the invocation index, which keeps a
/// 2^32 sweep from uploading 16 GB.
struct ChunkOperands {
    std::vector<std::uint32_t> first;
    std::vector<std::uint32_t> second;
};

ChunkOperands operandsForChunk(const ParityRequest& request, std::uint32_t span,
                               std::uint64_t& state) {
    ChunkOperands operands;
    if (!request.isSequential) {
        operands.first.resize(span);
        for (std::uint32_t index = 0; index < span; ++index) {
            operands.first[index] = static_cast<std::uint32_t>(terrainRandomNext(&state));
        }
    }
    if (request.routine != ArithmeticRoutine::Divide) {
        return operands;
    }
    operands.second.resize(span);
    for (std::uint32_t index = 0; index < span; ++index) {
        operands.second[index] = static_cast<std::uint32_t>(terrainRandomNext(&state));
    }
    return operands;
}

std::uint32_t firstPatternAt(const ParityRequest& request, const ChunkOperands& operands,
                             std::uint64_t chunkStart, std::uint32_t index) {
    if (request.isSequential) {
        return static_cast<std::uint32_t>(request.firstPattern + chunkStart + index);
    }
    return operands.first[index];
}

void accumulateMismatches(const ParityRequest& request, const ChunkOperands& operands,
                          const std::vector<std::uint32_t>& results, std::uint64_t chunkStart,
                          ParityReport& report) {
    for (std::uint32_t index = 0; index < results.size(); ++index) {
        const std::uint32_t firstBits = firstPatternAt(request, operands, chunkStart, index);
        std::uint32_t secondBits = 0x3f800000u;
        if (!operands.second.empty()) {
            secondBits = operands.second[index];
        }
        const std::uint32_t hostBits = hostResultFor(request.routine, firstBits, secondBits);
        if (isOutsideErosionDomain(request.routine, firstBits, secondBits, hostBits)) {
            continue;
        }
        report.testedCount += 1;
        if (results[index] == hostBits) {
            continue;
        }
        if (report.mismatchCount == 0) {
            report.firstMismatchPattern = firstBits;
            report.hostResult = hostBits;
            report.deviceResult = results[index];
        }
        report.mismatchCount += 1;
    }
}

}  // namespace

ParityReport checkArithmeticParity(const ParityRequest& request) {
    // The comparison is against the host as erosion runs it, and erosion runs
    // it flushing denormals. Computing the reference in any other mode would
    // measure a machine that never evaluates a graph.
    const FlushDenormalsToZero denormals;

    ParityReport report;
    VulkanDevice* device = VulkanDevice::shared();
    if (device == nullptr) {
        return report;
    }

    std::lock_guard<std::mutex> guard(device->mutex());
    ParityRunner runner(*device);
    if (!runner.prepare()) {
        return report;
    }
    report.ranOnDevice = true;

    std::vector<std::uint32_t> results;
    std::uint64_t state = request.seed;
    for (std::uint64_t done = 0; done < request.count; done += chunkSize) {
        const auto span =
            static_cast<std::uint32_t>(std::min<std::uint64_t>(chunkSize, request.count - done));
        const ChunkOperands operands = operandsForChunk(request, span, state);
        ParitySettings settings;
        settings.operation = static_cast<std::uint32_t>(request.routine);
        settings.firstPattern = static_cast<std::uint32_t>(request.firstPattern + done);
        settings.isSequential = static_cast<std::uint32_t>(request.isSequential);
        settings.count = span;

        if (!runner.runChunk(settings, operands.first, operands.second, results)) {
            report.ranOnDevice = false;
            return report;
        }
        accumulateMismatches(request, operands, results, done, report);
    }
    return report;
}

namespace {

const ArithmeticRoutine startupRoutines[] = {ArithmeticRoutine::Reciprocal,
                                             ArithmeticRoutine::SquareRoot,
                                             ArithmeticRoutine::Divide,
                                             ArithmeticRoutine::RoundToInt};

const char* routineName(ArithmeticRoutine routine) {
    if (routine == ArithmeticRoutine::Reciprocal) {
        return "reciprocal";
    }
    if (routine == ArithmeticRoutine::SquareRoot) {
        return "square root";
    }
    if (routine == ArithmeticRoutine::Divide) {
        return "divide";
    }
    return "round to int";
}

bool runStartupCheck(std::string& failureDetail) {
    for (ArithmeticRoutine routine : startupRoutines) {
        ParityRequest request;
        request.routine = routine;
        request.isSequential = false;
        request.count = startupSampleCount;
        request.seed = startupSeed;
        const ParityReport report = checkArithmeticParity(request);

        if (!report.ranOnDevice) {
            failureDetail = "the device could not be driven";
            return false;
        }
        if (report.mismatchCount > 0) {
            failureDetail = std::string(routineName(routine)) + " disagreed with the host on "
                            + std::to_string(report.mismatchCount) + " of "
                            + std::to_string(report.testedCount) + " inputs";
            return false;
        }
    }
    return true;
}

}  // namespace

bool deviceMatchesHostArithmetic(std::string* failureDetail) {
    static std::string cachedDetail;
    static const bool matches = runStartupCheck(cachedDetail);
    if (failureDetail != nullptr) {
        *failureDetail = cachedDetail;
    }
    return matches;
}

}  // namespace terrain::gpu
