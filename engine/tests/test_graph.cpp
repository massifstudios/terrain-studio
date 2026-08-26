#include <catch2/catch_test_macros.hpp>

#include <algorithm>

#include "terrain/graph.h"
#include "terrain/registry.h"

using terrain::dependencyOrder;
using terrain::Graph;
using terrain::GraphNode;
using terrain::NodeRegistry;
using terrain::validateGraph;

namespace {

NodeRegistry builtinRegistry() {
    NodeRegistry registry;
    terrain::registerBuiltinNodes(registry);
    return registry;
}

/// Constant -> Blur -> Add, with a second Constant on the other Add input.
Graph blurAndAddGraph() {
    Graph graph;
    graph.addNode("base", "Constant");
    graph.addNode("bump", "Constant");
    graph.addNode("soft", "Blur");
    graph.addNode("sum", "Add");
    graph.connect("base", 0, "soft", 0);
    graph.connect("soft", 0, "sum", 0);
    graph.connect("bump", 0, "sum", 1);
    return graph;
}

std::vector<std::string> idsOf(const std::vector<const GraphNode*>& nodes) {
    std::vector<std::string> ids;
    for (const GraphNode* node : nodes) {
        ids.push_back(node->id);
    }
    return ids;
}

}  // namespace

TEST_CASE("a well-formed graph validates", "[graph]") {
    const terrain::GraphValidation result = validateGraph(blurAndAddGraph(), builtinRegistry());
    REQUIRE(result.errors.empty());
    REQUIRE(result.isValid);
}

TEST_CASE("dependencies come before the nodes that need them", "[graph]") {
    const Graph graph = blurAndAddGraph();
    const std::vector<std::string> order = idsOf(dependencyOrder(graph, "sum"));

    REQUIRE(order.size() == 4);
    REQUIRE(order.back() == "sum");
    const auto positionOf = [&order](const std::string& id) {
        return std::find(order.begin(), order.end(), id) - order.begin();
    };
    REQUIRE(positionOf("base") < positionOf("soft"));
    REQUIRE(positionOf("soft") < positionOf("sum"));
    REQUIRE(positionOf("bump") < positionOf("sum"));
}

TEST_CASE("only the requested node's dependencies are ordered", "[graph]") {
    const Graph graph = blurAndAddGraph();
    const std::vector<std::string> order = idsOf(dependencyOrder(graph, "soft"));

    REQUIRE(order == std::vector<std::string>{"base", "soft"});
}

TEST_CASE("an unknown node type is a validation error", "[graph]") {
    Graph graph;
    graph.addNode("mystery", "NotARealNode");

    const terrain::GraphValidation result = validateGraph(graph, builtinRegistry());
    REQUIRE_FALSE(result.isValid);
    REQUIRE(result.errors.front().find("Unknown node type") != std::string::npos);
}

TEST_CASE("a required input left unconnected is a validation error", "[graph]") {
    Graph graph;
    graph.addNode("soft", "Blur");

    const terrain::GraphValidation result = validateGraph(graph, builtinRegistry());
    REQUIRE_FALSE(result.isValid);
    REQUIRE(result.errors.front().find("required port") != std::string::npos);
}

TEST_CASE("an optional mask input may stay unconnected", "[graph]") {
    Graph graph;
    graph.addNode("base", "Constant");
    graph.addNode("soft", "Blur");
    graph.connect("base", 0, "soft", 0);

    REQUIRE(validateGraph(graph, builtinRegistry()).isValid);
}

TEST_CASE("two edges into one input are refused", "[graph]") {
    Graph graph;
    graph.addNode("base", "Constant");
    graph.addNode("bump", "Constant");
    graph.addNode("soft", "Blur");
    graph.connect("base", 0, "soft", 0);
    graph.connect("bump", 0, "soft", 0);

    const terrain::GraphValidation result = validateGraph(graph, builtinRegistry());
    REQUIRE_FALSE(result.isValid);
}

TEST_CASE("a cycle is a validation error, not a hang", "[graph]") {
    Graph graph;
    graph.addNode("first", "Blur");
    graph.addNode("second", "Blur");
    graph.connect("first", 0, "second", 0);
    graph.connect("second", 0, "first", 0);

    const terrain::GraphValidation result = validateGraph(graph, builtinRegistry());
    REQUIRE_FALSE(result.isValid);
    REQUIRE(dependencyOrder(graph, "first").empty());
}

TEST_CASE("a port index that does not exist is refused", "[graph]") {
    Graph graph;
    graph.addNode("base", "Constant");
    graph.addNode("soft", "Blur");
    graph.connect("base", 7, "soft", 0);

    REQUIRE_FALSE(validateGraph(graph, builtinRegistry()).isValid);
}
