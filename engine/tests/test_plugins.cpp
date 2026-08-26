#include <catch2/catch_test_macros.hpp>

#include <algorithm>

#include "node_test_support.h"
#include "terrain/plugin_host.h"

using namespace terrain;

namespace {

const std::filesystem::path pluginDirectory{TERRAIN_TEST_PLUGIN_DIR};

const PluginReport* reportNamed(const std::vector<PluginReport>& reports, std::string_view name) {
    const auto found = std::find_if(reports.begin(), reports.end(), [name](const PluginReport& one) {
        return one.name == name;
    });
    if (found == reports.end()) {
        return nullptr;
    }
    return &*found;
}

/// A registry with the built-ins and whatever the example plugin adds.
struct LoadedPlugins {
    NodeRegistry registry;
    std::vector<PluginReport> reports;
};

LoadedPlugins loadExamplePlugins() {
    LoadedPlugins loaded;
    registerBuiltinNodes(loaded.registry);
    PluginHost host;
    loaded.reports = host.loadDirectory(pluginDirectory, loaded.registry);
    return loaded;
}

/// A tilted plane, so the plugin has real relief to roughen.
Graph rampGraph() {
    Graph graph = testing::squareGraph(64, 6400.0);
    GraphNode& source = graph.addNode("ramp", "Gradient");
    source.parameters["startValue"] = ParameterValue{0.0, ""};
    source.parameters["endValue"] = ParameterValue{1000.0, ""};
    GraphNode& speckle = graph.addNode("speckle", "Speckle");
    speckle.parameters["amplitude"] = ParameterValue{5.0, ""};
    graph.connect("ramp", 0, "speckle", 0);
    return graph;
}

std::shared_ptr<const Heightfield> evaluateWith(const NodeRegistry& registry, const Graph& graph,
                                                const std::string& nodeId, int threadCount) {
    Evaluator evaluator(registry);
    EvaluationOptions options;
    options.threadCount = threadCount;
    return evaluator.evaluate(graph, nodeId, 0, options).field;
}

}  // namespace

TEST_CASE("a plugin built from the SDK header alone loads and registers its node") {
    const LoadedPlugins loaded = loadExamplePlugins();

    const PluginReport* example = reportNamed(loaded.reports, "speckle");
    REQUIRE(example != nullptr);
    INFO(example->message);
    CHECK(example->loaded);
    CHECK(example->message.empty());
    CHECK(example->abiVersion == TERRAIN_SDK_ABI_VERSION);
    CHECK(example->nodeCount == 1);

    const NodeType* speckle = loaded.registry.find("Speckle");
    REQUIRE(speckle != nullptr);
    CHECK(speckle->category == "Example");
    CHECK(speckle->inputs.size() == 2);
    CHECK(speckle->parameters.size() == 2);
}

TEST_CASE("a plugin node reaches the registry JSON like any built-in") {
    const LoadedPlugins loaded = loadExamplePlugins();
    CHECK(loaded.registry.toJson().find("\"Speckle\"") != std::string::npos);
}

TEST_CASE("a plugin built against another ABI version is refused, not skipped") {
    const LoadedPlugins loaded = loadExamplePlugins();

    const PluginReport* future = reportNamed(loaded.reports, "future_abi");
    REQUIRE(future != nullptr);
    CHECK_FALSE(future->loaded);
    CHECK(future->abiVersion == TERRAIN_SDK_ABI_VERSION + 1);
    CHECK(future->message.find("ABI version") != std::string::npos);
    CHECK(future->message.find(std::to_string(TERRAIN_SDK_ABI_VERSION)) != std::string::npos);
}

TEST_CASE("a plugin node evaluates through the ordinary graph path") {
    const LoadedPlugins loaded = loadExamplePlugins();
    const Graph graph = rampGraph();

    const std::shared_ptr<const Heightfield> plain = evaluateWith(loaded.registry, graph, "ramp", 1);
    const std::shared_ptr<const Heightfield> rough =
        evaluateWith(loaded.registry, graph, "speckle", 1);
    REQUIRE(rough != nullptr);
    REQUIRE(plain != nullptr);

    CHECK(rough->maximum() > plain->maximum());
    for (std::size_t index = 0; index < rough->samples().size(); ++index) {
        REQUIRE(std::abs(rough->samples()[index] - plain->samples()[index]) <= 5.0f);
    }
}

TEST_CASE("a plugin node is bound by the determinism contract") {
    const LoadedPlugins loaded = loadExamplePlugins();
    const Graph graph = rampGraph();

    const std::shared_ptr<const Heightfield> single =
        evaluateWith(loaded.registry, graph, "speckle", 1);
    const std::shared_ptr<const Heightfield> many =
        evaluateWith(loaded.registry, graph, "speckle", 8);
    REQUIRE(single != nullptr);
    REQUIRE(many != nullptr);

    CHECK(testing::hashField(*single) == testing::hashField(*many));
}

TEST_CASE("a rescan does not load the same library twice") {
    NodeRegistry registry;
    registerBuiltinNodes(registry);
    PluginHost host;

    const std::vector<PluginReport> first = host.loadDirectory(pluginDirectory, registry);
    const std::vector<PluginReport> second = host.loadDirectory(pluginDirectory, registry);

    CHECK(first.size() >= 2);
    CHECK(second.empty());
    CHECK(host.reports().size() == first.size());
}

TEST_CASE("a plugin directory that is not there is not an error") {
    NodeRegistry registry;
    PluginHost host;
    CHECK(host.loadDirectory(pluginDirectory / "nowhere", registry).empty());
}
