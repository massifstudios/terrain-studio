#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>

#include "node_test_support.h"

using Catch::Approx;
using terrain::Graph;
using terrain::ParameterValue;
using terrain::testing::evaluateNode;
using terrain::testing::evaluateOutput;
using terrain::testing::hashField;
using terrain::testing::squareGraph;

namespace {

constexpr int resolution = 96;
constexpr double worldSize = 9600.0;

/// A mountain with fluvial erosion over it, the creep off so only the
/// streams touch the ground.
terrain::GraphNode& addErodedMountain(Graph& graph, int passes, double incision,
                                      double creep) {
    terrain::GraphNode& mountain = graph.addNode("mountain", "Mountain");
    mountain.parameters["radius"] = ParameterValue{4000.0, ""};
    mountain.parameters["height"] = ParameterValue{2000.0, ""};
    terrain::GraphNode& eroded = graph.addNode("eroded", "FluvialErosion");
    eroded.parameters["passes"] = ParameterValue{static_cast<double>(passes), ""};
    eroded.parameters["incision"] = ParameterValue{incision, ""};
    eroded.parameters["creep"] = ParameterValue{creep, ""};
    graph.connect("mountain", 0, "eroded", 0);
    return eroded;
}

}  // namespace

TEST_CASE("streams only ever cut the ground down", "[nodes][fluvial]") {
    Graph graph = squareGraph(resolution, worldSize);
    addErodedMountain(graph, 20, 30.0, 0.0);

    const auto before = evaluateNode(graph, "mountain");
    const auto after = evaluateNode(graph, "eroded");
    float deepestCut = 0.0f;
    for (int row = 0; row < resolution; ++row) {
        for (int column = 0; column < resolution; ++column) {
            const float change = after->at(column, row) - before->at(column, row);
            REQUIRE(change <= 1.0e-3f);
            deepestCut = std::min(deepestCut, change);
        }
    }
    REQUIRE(deepestCut < -50.0f);
}

TEST_CASE("a stream cuts deeper the more ground drains into it", "[nodes][fluvial]") {
    // On a tilted plane every cell drains the same way, so the water gathered
    // grows row by row towards the low edge, and so does the cut.
    Graph graph = squareGraph(resolution, worldSize);
    terrain::GraphNode& ground = graph.addNode("ground", "Gradient");
    ground.parameters["direction"] = ParameterValue{0.0, ""};
    ground.parameters["startValue"] = ParameterValue{0.0, ""};
    ground.parameters["endValue"] = ParameterValue{3000.0, ""};
    terrain::GraphNode& eroded = graph.addNode("eroded", "FluvialErosion");
    eroded.parameters["passes"] = ParameterValue{10.0, ""};
    eroded.parameters["incision"] = ParameterValue{20.0, ""};
    eroded.parameters["creep"] = ParameterValue{0.0, ""};
    graph.connect("ground", 0, "eroded", 0);

    const auto before = evaluateNode(graph, "ground");
    const auto after = evaluateNode(graph, "eroded");
    // The low edge itself is an outlet and never moves, which holds the cut
    // back beside it, so the comparison is made between the top and the middle.
    const int column = resolution / 2;
    int nearTop = 8;
    int midway = resolution / 2;
    if (before->at(column, nearTop) < before->at(column, resolution - 8)) {
        nearTop = resolution - 8;
    }
    const float cutNearTop = before->at(column, nearTop) - after->at(column, nearTop);
    const float cutMidway = before->at(column, midway) - after->at(column, midway);
    REQUIRE(cutMidway > cutNearTop * 1.5f);

    const auto flow = evaluateOutput(graph, "eroded", 1);
    REQUIRE(flow->at(column, midway) > flow->at(column, nearTop));
}

TEST_CASE("creep rounds the divides the streams leave behind", "[nodes][fluvial]") {
    Graph graph = squareGraph(resolution, worldSize);
    addErodedMountain(graph, 20, 30.0, 0.0);
    terrain::GraphNode& rounded = graph.addNode("rounded", "FluvialErosion");
    rounded.parameters["passes"] = ParameterValue{20.0, ""};
    rounded.parameters["incision"] = ParameterValue{30.0, ""};
    rounded.parameters["creep"] = ParameterValue{60.0, ""};
    graph.connect("mountain", 0, "rounded", 0);

    const auto sharp = evaluateNode(graph, "eroded");
    const auto soft = evaluateNode(graph, "rounded");
    REQUIRE(hashField(*sharp) != hashField(*soft));
    // Creep moves ground without removing it, so the two agree on the total.
    double sharpTotal = 0.0;
    double softTotal = 0.0;
    for (int row = 4; row < resolution - 4; ++row) {
        for (int column = 4; column < resolution - 4; ++column) {
            sharpTotal += sharp->at(column, row);
            softTotal += soft->at(column, row);
        }
    }
    REQUIRE(softTotal == Approx(sharpTotal).epsilon(0.05));
}

TEST_CASE("fluvial erosion leaves masked ground alone", "[nodes][fluvial]") {
    Graph graph = squareGraph(resolution, worldSize);
    addErodedMountain(graph, 20, 30.0, 10.0);
    terrain::GraphNode& mask = graph.addNode("mask", "Constant");
    mask.parameters["value"] = ParameterValue{0.0, ""};
    graph.connect("mask", 0, "eroded", 1);

    REQUIRE(hashField(*evaluateNode(graph, "eroded")) == hashField(*evaluateNode(graph, "mountain")));
}

TEST_CASE("fluvial erosion cuts the same valleys at any thread count",
          "[nodes][fluvial][determinism]") {
    Graph graph = squareGraph(resolution, worldSize);
    addErodedMountain(graph, 30, 30.0, 15.0);

    REQUIRE(hashField(*evaluateNode(graph, "eroded", 1))
            == hashField(*evaluateNode(graph, "eroded", 8)));
}
