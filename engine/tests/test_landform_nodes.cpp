#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "node_test_support.h"

using Catch::Approx;
using terrain::Graph;
using terrain::ParameterValue;
using terrain::testing::allSamplesWithin;
using terrain::testing::evaluateNode;
using terrain::testing::hashField;
using terrain::testing::isConstant;
using terrain::testing::squareGraph;

namespace {

constexpr int resolution = 64;
constexpr double worldSize = 12800.0;
constexpr int middle = resolution / 2;

/// A mountain with its two stochastic dials at zero, which leaves the analytic
/// shape on its own and makes every height below predictable.
terrain::GraphNode& addPlainMountain(Graph& graph, const std::string& id) {
    terrain::GraphNode& mountain = graph.addNode(id, "Mountain");
    mountain.parameters["radius"] = ParameterValue{4000.0, ""};
    mountain.parameters["height"] = ParameterValue{1000.0, ""};
    mountain.parameters["peakSharpness"] = ParameterValue{1.0, ""};
    mountain.parameters["irregularity"] = ParameterValue{0.0, ""};
    mountain.parameters["detailStrength"] = ParameterValue{0.0, ""};
    mountain.parameters["baseAltitude"] = ParameterValue{0.0, ""};
    return mountain;
}

}  // namespace

/* ------------------------------------------------------------- Mountain -- */

TEST_CASE("a mountain summits at its centre and reaches the ground at its radius",
          "[nodes][mountain]") {
    Graph graph = squareGraph(resolution, worldSize);
    addPlainMountain(graph, "peak");

    const auto field = evaluateNode(graph, "peak");
    REQUIRE(field != nullptr);
    REQUIRE(field->at(middle, middle) > 990.0f);
    REQUIRE(field->at(0, 0) == Approx(0.0));
    REQUIRE(allSamplesWithin(*field, 0.0f, 1000.0f));
}

TEST_CASE("a mountain sits on the altitude it is given", "[nodes][mountain]") {
    Graph graph = squareGraph(resolution, worldSize);
    terrain::GraphNode& mountain = addPlainMountain(graph, "peak");
    mountain.parameters["baseAltitude"] = ParameterValue{250.0, ""};

    const auto field = evaluateNode(graph, "peak");
    REQUIRE(field->minimum() == Approx(250.0));
    REQUIRE(field->maximum() == Approx(1250.0).margin(10.0));
}

TEST_CASE("a sharper peak carries less mass on its flanks", "[nodes][mountain]") {
    Graph graph = squareGraph(resolution, worldSize);
    addPlainMountain(graph, "blunt");
    terrain::GraphNode& sharp = addPlainMountain(graph, "sharp");
    sharp.parameters["peakSharpness"] = ParameterValue{3.0, ""};

    const auto bluntField = evaluateNode(graph, "blunt");
    const auto sharpField = evaluateNode(graph, "sharp");
    REQUIRE(sharpField->at(middle, middle + 12) < bluntField->at(middle, middle + 12));
    REQUIRE(sharpField->at(middle, middle) == Approx(bluntField->at(middle, middle)).margin(15.0));
}

TEST_CASE("detail roughens the slopes without lifting the summit",
          "[nodes][mountain]") {
    Graph graph = squareGraph(resolution, worldSize);
    terrain::GraphNode& rough = addPlainMountain(graph, "rough");
    rough.parameters["detailStrength"] = ParameterValue{1.0, ""};

    const auto field = evaluateNode(graph, "rough");
    REQUIRE(field->maximum() <= 1000.0f);
    REQUIRE_FALSE(isConstant(*field));
}

TEST_CASE("a mountain draws the same shape at any thread count",
          "[nodes][mountain][determinism]") {
    Graph graph = squareGraph(resolution, worldSize);
    graph.addNode("peak", "Mountain");

    REQUIRE(hashField(*evaluateNode(graph, "peak", 1))
            == hashField(*evaluateNode(graph, "peak", 8)));
}

TEST_CASE("rolling a mountain's seed changes it", "[nodes][mountain]") {
    Graph graph = squareGraph(resolution, worldSize);
    graph.addNode("first", "Mountain");
    terrain::GraphNode& second = graph.addNode("second", "Mountain");
    second.parameters["seed"] = ParameterValue{7.0, ""};

    REQUIRE(hashField(*evaluateNode(graph, "first"))
            != hashField(*evaluateNode(graph, "second")));
}

/* -------------------------------------------------------- MountainRange -- */

TEST_CASE("a range stands along its bearing and nowhere else",
          "[nodes][mountainrange]") {
    Graph graph = squareGraph(resolution, worldSize);
    terrain::GraphNode& range = graph.addNode("range", "MountainRange");
    range.parameters["bearing"] = ParameterValue{0.0, ""};
    range.parameters["length"] = ParameterValue{9000.0, ""};
    range.parameters["width"] = ParameterValue{1500.0, ""};
    range.parameters["peakCount"] = ParameterValue{4.0, ""};
    range.parameters["height"] = ParameterValue{1500.0, ""};
    range.parameters["irregularity"] = ParameterValue{0.0, ""};
    range.parameters["detailStrength"] = ParameterValue{0.0, ""};

    const auto field = evaluateNode(graph, "range");
    // The spine runs north–south through the middle, so the east and west
    // edges of the world are well clear of it.
    REQUIRE(field->at(middle, 0) == Approx(0.0));
    REQUIRE(field->at(middle, resolution - 1) == Approx(0.0));
    REQUIRE(field->maximum() > 500.0f);
    REQUIRE(field->maximum() <= 1500.0f);
}

TEST_CASE("changing the peak count changes the range", "[nodes][mountainrange]") {
    Graph graph = squareGraph(resolution, worldSize);
    terrain::GraphNode& few = graph.addNode("few", "MountainRange");
    few.parameters["peakCount"] = ParameterValue{2.0, ""};
    terrain::GraphNode& many = graph.addNode("many", "MountainRange");
    many.parameters["peakCount"] = ParameterValue{12.0, ""};

    REQUIRE(hashField(*evaluateNode(graph, "few"))
            != hashField(*evaluateNode(graph, "many")));
}

TEST_CASE("a range draws the same chain at any thread count",
          "[nodes][mountainrange][determinism]") {
    Graph graph = squareGraph(resolution, worldSize);
    graph.addNode("range", "MountainRange");

    REQUIRE(hashField(*evaluateNode(graph, "range", 1))
            == hashField(*evaluateNode(graph, "range", 8)));
}

TEST_CASE("nothing in a range lies below the altitude it sits on",
          "[nodes][mountainrange]") {
    Graph graph = squareGraph(resolution, worldSize);
    terrain::GraphNode& range = graph.addNode("range", "MountainRange");
    range.parameters["length"] = ParameterValue{9000.0, ""};
    range.parameters["width"] = ParameterValue{1500.0, ""};
    range.parameters["height"] = ParameterValue{1500.0, ""};
    range.parameters["baseAltitude"] = ParameterValue{100.0, ""};

    const auto field = evaluateNode(graph, "range");
    REQUIRE(field->minimum() == Approx(100.0));
    REQUIRE(field->maximum() <= 1600.0f);
}
