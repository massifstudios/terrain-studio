/*
 * Times one erosion graph on every backend this machine offers.
 *
 * The point is not a score. It is that the same graph is being run each time,
 * so the numbers are comparable, and that the outputs are compared bit for bit
 * afterwards — a backend that is fast and disagrees is worth nothing.
 */

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "erosion_backend.h"
#include "terrain/evaluator.h"
#include "terrain/graph.h"
#include "terrain/registry.h"

namespace {

using terrain::gpu::ErosionBackend;

terrain::Graph erosionGraph(int resolution, double density) {
    terrain::Graph graph;
    graph.settings.resolution = resolution;
    graph.settings.worldSizeMeters = resolution * 30.0;
    graph.settings.seed = 20260826;

    terrain::GraphNode& shape = graph.addNode("shape", "RidgedNoise");
    shape.parameters["featureSize"] = terrain::ParameterValue{3000.0, ""};
    shape.parameters["octaves"] = terrain::ParameterValue{6.0, ""};
    shape.parameters["amplitude"] = terrain::ParameterValue{1200.0, ""};

    terrain::GraphNode& rain = graph.addNode("rain", "HydraulicErosion");
    rain.parameters["density"] = terrain::ParameterValue{density, ""};
    graph.connect("shape", 0, "rain", 0);
    return graph;
}

struct Timing {
    double seconds = 0.0;
    std::vector<std::uint32_t> patterns;
};

Timing runOnce(const terrain::Graph& graph) {
    terrain::NodeRegistry registry;
    terrain::registerBuiltinNodes(registry);
    terrain::Evaluator evaluator(registry);
    terrain::EvaluationOptions options;

    const auto started = std::chrono::steady_clock::now();
    const auto result = evaluator.evaluate(graph, "rain", 0, options);
    const auto finished = std::chrono::steady_clock::now();

    Timing timing;
    timing.seconds = std::chrono::duration<double>(finished - started).count();
    if (result.field == nullptr) {
        return timing;
    }
    timing.patterns.resize(result.field->samples().size());
    std::memcpy(timing.patterns.data(), result.field->samples().data(),
                timing.patterns.size() * sizeof(std::uint32_t));
    return timing;
}

}  // namespace

int main(int argumentCount, char** arguments) {
    int resolution = 1024;
    if (argumentCount > 1) {
        resolution = std::atoi(arguments[1]);
    }
    const terrain::Graph graph = erosionGraph(resolution, 2.0);
    std::printf("hydraulic erosion at %d x %d, 2 droplets per cell\n\n", resolution, resolution);

    std::vector<std::uint32_t> reference;
    for (ErosionBackend backend :
         {ErosionBackend::Cpu, ErosionBackend::Vulkan, ErosionBackend::Cuda}) {
        if (!terrain::gpu::forceErosionBackend(backend)) {
            std::printf("  %-7s unavailable\n", terrain::gpu::nameOf(backend));
            continue;
        }
        const Timing timing = runOnce(graph);
        std::printf("  %-7s %7.2f s", terrain::gpu::nameOf(backend), timing.seconds);
        if (reference.empty()) {
            reference = timing.patterns;
            std::printf("   (reference)\n");
            continue;
        }
        if (timing.patterns == reference) {
            std::printf("   bit-identical to the CPU\n");
            continue;
        }
        std::printf("   DIFFERS FROM THE CPU\n");
    }
    return 0;
}
