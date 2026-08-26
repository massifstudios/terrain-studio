#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <numeric>
#include <string>
#include <vector>

#include "terrain/evaluator.h"
#include "terrain/formats/hillshade.h"
#include "terrain/formats/massifdem.h"
#include "terrain/graph.h"
#include "terrain/hash.h"
#include "terrain/registry.h"
#include "terrain/resample.h"
#include "test_support.h"

using Catch::Approx;
using terrain::EvaluationOptions;
using terrain::EvaluationOutcome;
using terrain::Evaluator;
using terrain::Graph;
using terrain::Heightfield;
using terrain::NodeRegistry;
using terrain::ParameterValue;
using terrain::testing::dataDirectory;
using terrain::testing::ScratchFile;

namespace {

NodeRegistry builtinRegistry() {
    NodeRegistry registry;
    terrain::registerBuiltinNodes(registry);
    return registry;
}

/// The reference graph the determinism and cache tests both run:
/// the fixture DEM, blurred, with a constant offset added.
Graph refinementGraph(int resolution, double blurRadiusMeters = 120.0) {
    Graph graph;
    graph.settings.resolution = resolution;
    graph.settings.worldSizeMeters = 2880.0;
    graph.settings.seed = 20260826;

    terrain::GraphNode& source = graph.addNode("source", "ImportDEM");
    source.parameters["path"] =
        ParameterValue{0.0, (dataDirectory() / "fixture.massifdem").string()};

    terrain::GraphNode& soft = graph.addNode("soft", "Blur");
    soft.parameters["radius"] = ParameterValue{blurRadiusMeters, ""};

    terrain::GraphNode& lift = graph.addNode("lift", "Constant");
    lift.parameters["value"] = ParameterValue{25.0, ""};

    graph.addNode("sum", "Add");

    graph.connect("source", 0, "soft", 0);
    graph.connect("soft", 0, "sum", 0);
    graph.connect("lift", 0, "sum", 1);
    return graph;
}

std::uint64_t hashOf(const Heightfield& field) {
    terrain::Hasher hasher;
    for (const float sample : field.samples()) {
        hasher.combine(static_cast<double>(sample));
    }
    return hasher.digest();
}

double meanOf(const Heightfield& field) {
    double total = 0.0;
    for (const float sample : field.samples()) {
        total += sample;
    }
    return total / field.sampleCount();
}

}  // namespace

TEST_CASE("a hardcoded graph evaluates end to end", "[evaluator]") {
    const NodeRegistry registry = builtinRegistry();
    Evaluator evaluator(registry);

    const EvaluationOutcome outcome =
        evaluator.evaluate(refinementGraph(128), "sum", 0, EvaluationOptions{});

    REQUIRE(outcome.errors.empty());
    REQUIRE(outcome.succeeded());
    REQUIRE(outcome.field->width() == 128);
    REQUIRE(outcome.field->height() == 128);
    REQUIRE(outcome.field->cellSizeMeters() == Approx(2880.0 / 128));
    REQUIRE(outcome.evaluatedNodeIds.size() == 4);
}

TEST_CASE("blur preserves the mean and the constant lifts it", "[evaluator]") {
    const NodeRegistry registry = builtinRegistry();
    Evaluator evaluator(registry);
    const Graph graph = refinementGraph(128);

    const EvaluationOutcome imported =
        evaluator.evaluate(graph, "source", 0, EvaluationOptions{});
    const EvaluationOutcome blurred = evaluator.evaluate(graph, "soft", 0, EvaluationOptions{});
    const EvaluationOutcome summed = evaluator.evaluate(graph, "sum", 0, EvaluationOptions{});

    // A box blur with clamped edges is mean-preserving to within edge effects.
    REQUIRE(meanOf(*blurred.field) == Approx(meanOf(*imported.field)).epsilon(0.01));
    REQUIRE(meanOf(*summed.field) == Approx(meanOf(*blurred.field) + 25.0).epsilon(1.0e-4));
}

TEST_CASE("the same graph and seed give bit-identical output twice", "[evaluator][determinism]") {
    const NodeRegistry registry = builtinRegistry();
    const Graph graph = refinementGraph(128);

    Evaluator first(registry);
    Evaluator second(registry);
    const std::uint64_t firstHash =
        hashOf(*first.evaluate(graph, "sum", 0, EvaluationOptions{}).field);
    const std::uint64_t secondHash =
        hashOf(*second.evaluate(graph, "sum", 0, EvaluationOptions{}).field);

    REQUIRE(firstHash == secondHash);
}

TEST_CASE("one thread and many threads give the same output", "[evaluator][determinism]") {
    const NodeRegistry registry = builtinRegistry();
    const Graph graph = refinementGraph(128);

    EvaluationOptions single;
    single.threadCount = 1;
    EvaluationOptions many;
    many.threadCount = 8;

    Evaluator sequential(registry);
    Evaluator parallel(registry);

    REQUIRE(hashOf(*sequential.evaluate(graph, "sum", 0, single).field)
            == hashOf(*parallel.evaluate(graph, "sum", 0, many).field));
}

TEST_CASE("a clean cache re-evaluates nothing", "[evaluator][cache]") {
    const NodeRegistry registry = builtinRegistry();
    Evaluator evaluator(registry);
    const Graph graph = refinementGraph(128);

    REQUIRE(evaluator.evaluate(graph, "sum", 0, EvaluationOptions{}).evaluatedNodeIds.size() == 4);
    REQUIRE(evaluator.evaluate(graph, "sum", 0, EvaluationOptions{}).evaluatedNodeIds.empty());
}

TEST_CASE("editing a parameter re-evaluates only that node and its dependents",
          "[evaluator][cache]") {
    const NodeRegistry registry = builtinRegistry();
    Evaluator evaluator(registry);

    Graph graph = refinementGraph(128);
    evaluator.evaluate(graph, "sum", 0, EvaluationOptions{});

    const Graph edited = refinementGraph(128, 240.0);
    const EvaluationOutcome outcome = evaluator.evaluate(edited, "sum", 0, EvaluationOptions{});
    REQUIRE(outcome.evaluatedNodeIds == std::vector<std::string>{"soft", "sum"});
}

TEST_CASE("a node that fails reports the failure instead of a result", "[evaluator]") {
    const NodeRegistry registry = builtinRegistry();
    Evaluator evaluator(registry);

    Graph graph;
    graph.settings.resolution = 32;
    terrain::GraphNode& source = graph.addNode("source", "ImportDEM");
    source.parameters["path"] = ParameterValue{0.0, "/does/not/exist.massifdem"};

    const EvaluationOutcome outcome =
        evaluator.evaluate(graph, "source", 0, EvaluationOptions{});
    REQUIRE_FALSE(outcome.succeeded());
    REQUIRE(outcome.errors.size() == 1);
    REQUIRE(outcome.errors.front().starts_with("source: "));
}

TEST_CASE("a cancelled evaluation returns without a result", "[evaluator]") {
    const NodeRegistry registry = builtinRegistry();
    Evaluator evaluator(registry);
    terrain::CancellationToken token;
    token.cancel();

    EvaluationOptions options;
    options.cancellation = &token;

    const EvaluationOutcome outcome =
        evaluator.evaluate(refinementGraph(64), "sum", 0, options);
    REQUIRE(outcome.wasCancelled);
    REQUIRE(outcome.field == nullptr);
}

TEST_CASE("export writes only during a build", "[evaluator]") {
    const NodeRegistry registry = builtinRegistry();
    Evaluator evaluator(registry);

    ScratchFile output("graph-export.massifdem");
    Graph graph = refinementGraph(64);
    terrain::GraphNode& exporter = graph.addNode("out", "Export");
    exporter.parameters["path"] = ParameterValue{0.0, output.path().string()};
    graph.connect("sum", 0, "out", 0);

    evaluator.evaluate(graph, "out", 0, EvaluationOptions{});
    REQUIRE_FALSE(std::filesystem::exists(output.path()));

    // Deliberately not clearing the cache: a build straight after a preview
    // must still write, which it only does if the two are different cache
    // entries.
    EvaluationOptions build;
    build.isFinalBuild = true;
    const EvaluationOutcome outcome = evaluator.evaluate(graph, "out", 0, build);

    REQUIRE(outcome.errors.empty());
    REQUIRE(std::filesystem::exists(output.path()));
    REQUIRE(std::filesystem::exists(terrain::formats::previewPathFor(output.path())));
    std::filesystem::remove(terrain::formats::previewPathFor(output.path()));
}

TEST_CASE("a node seed depends on the graph seed and the node id", "[evaluator][determinism]") {
    REQUIRE(terrain::deriveNodeSeed(7, "erode") == terrain::deriveNodeSeed(7, "erode"));
    REQUIRE(terrain::deriveNodeSeed(7, "erode") != terrain::deriveNodeSeed(8, "erode"));
    REQUIRE(terrain::deriveNodeSeed(7, "erode") != terrain::deriveNodeSeed(7, "scatter"));
}

TEST_CASE("every node returns a thumbnail when one is asked for", "[evaluator]") {
    const NodeRegistry registry = builtinRegistry();
    Evaluator evaluator(registry);

    EvaluationOptions options;
    options.thumbnailResolution = 32;

    const EvaluationOutcome outcome =
        evaluator.evaluate(refinementGraph(128), "sum", 0, options);

    REQUIRE(outcome.thumbnails.size() == 4);
    for (const terrain::NodeThumbnail& thumbnail : outcome.thumbnails) {
        REQUIRE(thumbnail.image.size == 32);
        REQUIRE(thumbnail.image.rgba.size() == 32u * 32u * 4u);
    }
}

TEST_CASE("a thumbnail averages rather than point-samples", "[evaluator]") {
    Heightfield source(4, 4);
    source.setCellSizeMeters(1.0);
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            source.at(row, column) = static_cast<float>(row * 4 + column);
        }
    }

    const Heightfield small = terrain::downsampleAverage(source, 2);
    REQUIRE(small.width() == 2);
    REQUIRE(small.at(0, 0) == Approx(2.5));
    REQUIRE(small.at(1, 1) == Approx(12.5));
}
