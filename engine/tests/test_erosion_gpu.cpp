#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <string>
#include <vector>

#include "erosion_backend.h"
#include "node_test_support.h"

using terrain::Graph;
using terrain::Heightfield;
using terrain::ParameterValue;
using terrain::gpu::ErosionBackend;
using terrain::gpu::forceErosionBackend;
using terrain::gpu::selectedErosionBackend;
using terrain::testing::evaluateOutput;
using terrain::testing::squareGraph;

namespace {

constexpr int resolution = 96;
constexpr double worldSize = 9600.0;

Graph ridgedGround(double amplitudeMeters) {
    Graph graph = squareGraph(resolution, worldSize);
    terrain::GraphNode& shape = graph.addNode("shape", "RidgedNoise");
    shape.parameters["featureSize"] = ParameterValue{3000.0, ""};
    shape.parameters["octaves"] = ParameterValue{5.0, ""};
    shape.parameters["amplitude"] = ParameterValue{amplitudeMeters, ""};
    return graph;
}

/**
 * Every sample as its exact bit pattern.
 *
 * A tolerance would defeat the purpose. The claim being tested is not that the
 * GPU is close to the CPU but that the two are the same number, because a
 * graph must not depend on which machine built it.
 */
std::vector<std::uint32_t> samplePatterns(const Heightfield& field) {
    std::vector<std::uint32_t> patterns(field.samples().size());
    std::memcpy(patterns.data(), field.samples().data(), patterns.size() * sizeof(std::uint32_t));
    return patterns;
}

/// The backends this machine can actually run, always including the CPU.
std::vector<ErosionBackend> availableBackends() {
    std::vector<ErosionBackend> backends{ErosionBackend::Cpu};
    for (ErosionBackend candidate : {ErosionBackend::Vulkan, ErosionBackend::Cuda}) {
        if (forceErosionBackend(candidate)) {
            backends.push_back(candidate);
        }
    }
    forceErosionBackend(ErosionBackend::Cpu);
    return backends;
}

/// Runs one output of one node on every backend and requires them to agree.
void requireEveryBackendAgrees(const Graph& graph, const std::string& nodeId, int outputPort) {
    const std::vector<ErosionBackend> backends = availableBackends();
    if (backends.size() < 2) {
        SKIP("no GPU backend on this machine: " + terrain::gpu::erosionBackendExplanation());
    }

    REQUIRE(forceErosionBackend(ErosionBackend::Cpu));
    const auto onCpu = evaluateOutput(graph, nodeId, outputPort);
    REQUIRE(onCpu != nullptr);
    const std::vector<std::uint32_t> reference = samplePatterns(*onCpu);

    for (ErosionBackend backend : backends) {
        REQUIRE(forceErosionBackend(backend));
        const auto onDevice = evaluateOutput(graph, nodeId, outputPort);
        REQUIRE(onDevice != nullptr);
        INFO("backend: " << terrain::gpu::nameOf(backend));
        REQUIRE(samplePatterns(*onDevice) == reference);
    }
    forceErosionBackend(ErosionBackend::Cpu);
}

Graph rainOnRidges(double density) {
    Graph graph = ridgedGround(1200.0);
    terrain::GraphNode& rain = graph.addNode("rain", "HydraulicErosion");
    rain.parameters["density"] = ParameterValue{density, ""};
    graph.connect("shape", 0, "rain", 0);
    return graph;
}

}  // namespace

TEST_CASE("every backend slumps to the same surface, bit for bit", "[erosion][gpu][determinism]") {
    Graph graph = ridgedGround(1200.0);
    graph.addNode("slump", "ThermalErosion");
    graph.connect("shape", 0, "slump", 0);
    requireEveryBackendAgrees(graph, "slump", 0);
}

TEST_CASE("every backend settles the same sediment, bit for bit", "[erosion][gpu][determinism]") {
    Graph graph = ridgedGround(1200.0);
    terrain::GraphNode& fill = graph.addNode("fill", "Deposit");
    fill.parameters["amount"] = ParameterValue{40.0, ""};
    graph.connect("shape", 0, "fill", 0);

    requireEveryBackendAgrees(graph, "fill", 0);
    requireEveryBackendAgrees(graph, "fill", 1);
}

TEST_CASE("every backend cuts the same channels, bit for bit", "[erosion][gpu][determinism]") {
    const Graph graph = rainOnRidges(2.0);
    requireEveryBackendAgrees(graph, "rain", 0);
}

TEST_CASE("every backend reports the same wear, deposit and flow, bit for bit",
          "[erosion][gpu][determinism]") {
    const Graph graph = rainOnRidges(2.0);
    for (int outputPort = 1; outputPort <= 3; ++outputPort) {
        requireEveryBackendAgrees(graph, "rain", outputPort);
    }
}

TEST_CASE("a mask still scales the effect when the GPU did the work",
          "[erosion][gpu][determinism]") {
    Graph graph = ridgedGround(1200.0);
    terrain::GraphNode& band = graph.addNode("band", "SlopeMask");
    graph.addNode("slump", "ThermalErosion");
    graph.connect("shape", 0, "band", 0);
    graph.connect("shape", 0, "slump", 0);
    graph.connect("band", 0, "slump", 1);
    requireEveryBackendAgrees(graph, "slump", 0);
}

TEST_CASE("the backend in use is one the machine can actually run", "[erosion][gpu]") {
    const ErosionBackend backend = selectedErosionBackend();
    if (backend == ErosionBackend::Cpu) {
        SUCCEED("erosion runs on the CPU here");
        return;
    }
    REQUIRE(forceErosionBackend(backend));
}
