#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <string>

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
constexpr double worldSize = 6400.0;
constexpr int middle = resolution / 2;

/// A north-facing ramp, the field every rearranging node is checked against
/// because every row of it is distinguishable from every other row.
Graph rampGraph() {
    Graph graph = squareGraph(resolution, worldSize);
    terrain::GraphNode& ramp = graph.addNode("ramp", "Gradient");
    ramp.parameters["direction"] = ParameterValue{0.0, ""};
    ramp.parameters["startValue"] = ParameterValue{0.0, ""};
    ramp.parameters["endValue"] = ParameterValue{1000.0, ""};
    return graph;
}

}  // namespace

/* ------------------------------------------------------- RadialGradient -- */

TEST_CASE("a radial gradient peaks at its centre and dies at its radius",
          "[nodes][radialgradient]") {
    Graph graph = squareGraph(resolution, worldSize);
    terrain::GraphNode& bell = graph.addNode("bell", "RadialGradient");
    bell.parameters["radius"] = ParameterValue{1600.0, ""};
    bell.parameters["profile"] = ParameterValue{0.0, ""};
    bell.parameters["amplitude"] = ParameterValue{1000.0, ""};
    bell.parameters["offset"] = ParameterValue{0.0, ""};

    const auto field = evaluateNode(graph, "bell");
    REQUIRE(field != nullptr);
    REQUIRE(field->at(middle, middle) > 950.0f);
    REQUIRE(field->at(0, 0) == Approx(0.0));
    REQUIRE(field->maximum() <= 1000.0f);
}

TEST_CASE("a dome stands higher than a cone at the same distance",
          "[nodes][radialgradient]") {
    Graph graph = squareGraph(resolution, worldSize);
    terrain::GraphNode& cone = graph.addNode("cone", "RadialGradient");
    cone.parameters["radius"] = ParameterValue{3200.0, ""};
    cone.parameters["profile"] = ParameterValue{0.0, ""};
    terrain::GraphNode& dome = graph.addNode("dome", "RadialGradient");
    dome.parameters["radius"] = ParameterValue{3200.0, ""};
    dome.parameters["profile"] = ParameterValue{1.0, ""};

    const auto coneField = evaluateNode(graph, "cone");
    const auto domeField = evaluateNode(graph, "dome");
    REQUIRE(domeField->at(middle, 8) > coneField->at(middle, 8));
}

/* ------------------------------------------------------------------ Shape -- */

TEST_CASE("a disc fills its radius and nothing beyond it", "[nodes][shape]") {
    Graph graph = squareGraph(resolution, worldSize);
    terrain::GraphNode& disc = graph.addNode("disc", "Shape");
    disc.parameters["shape"] = ParameterValue{0.0, ""};
    disc.parameters["radius"] = ParameterValue{1600.0, ""};
    disc.parameters["edgeFalloff"] = ParameterValue{0.0, ""};
    disc.parameters["amplitude"] = ParameterValue{100.0, ""};
    disc.parameters["offset"] = ParameterValue{0.0, ""};

    const auto field = evaluateNode(graph, "disc");
    REQUIRE(field->at(middle, middle) == Approx(100.0));
    REQUIRE(field->at(0, 0) == Approx(0.0));
    REQUIRE(allSamplesWithin(*field, 0.0f, 100.0f));
}

TEST_CASE("a square reaches its corners where a disc of the same radius cannot",
          "[nodes][shape]") {
    Graph graph = squareGraph(resolution, worldSize);
    terrain::GraphNode& disc = graph.addNode("disc", "Shape");
    disc.parameters["shape"] = ParameterValue{0.0, ""};
    disc.parameters["radius"] = ParameterValue{1600.0, ""};
    disc.parameters["edgeFalloff"] = ParameterValue{0.0, ""};
    disc.parameters["amplitude"] = ParameterValue{100.0, ""};

    terrain::GraphNode& square = graph.addNode("square", "Shape");
    square.parameters["shape"] = ParameterValue{1.0, ""};
    square.parameters["radius"] = ParameterValue{1600.0, ""};
    square.parameters["edgeFalloff"] = ParameterValue{0.0, ""};
    square.parameters["amplitude"] = ParameterValue{100.0, ""};

    // A cell diagonally out from the centre by more than the radius but less
    // than the radius times root two: inside the square, outside the disc.
    const int corner = middle + 14;
    REQUIRE(evaluateNode(graph, "disc")->at(corner, corner) == Approx(0.0));
    REQUIRE(evaluateNode(graph, "square")->at(corner, corner) == Approx(100.0));
}

/* -------------------------------------------------------------- Pattern -- */

TEST_CASE("a checker alternates every feature", "[nodes][pattern]") {
    Graph graph = squareGraph(resolution, worldSize);
    terrain::GraphNode& checker = graph.addNode("checker", "Pattern");
    checker.parameters["pattern"] = ParameterValue{0.0, ""};
    checker.parameters["featureSize"] = ParameterValue{200.0, ""};
    checker.parameters["softness"] = ParameterValue{0.0, ""};
    checker.parameters["amplitude"] = ParameterValue{1.0, ""};
    checker.parameters["offset"] = ParameterValue{0.0, ""};

    const auto field = evaluateNode(graph, "checker");
    REQUIRE(allSamplesWithin(*field, 0.0f, 1.0f));
    REQUIRE_FALSE(isConstant(*field));
    // Two cells is one feature at this world size, so the neighbour but one
    // belongs to the opposite square.
    REQUIRE(field->at(0, 0) != field->at(0, 2));
    REQUIRE(field->at(0, 0) == field->at(0, 4));
}

/* ---------------------------------------------------------------- Noise -- */

TEST_CASE("white noise stays inside its amplitude", "[nodes][noise]") {
    Graph graph = squareGraph(resolution, worldSize);
    terrain::GraphNode& grain = graph.addNode("grain", "Noise");
    grain.parameters["amplitude"] = ParameterValue{10.0, ""};
    grain.parameters["offset"] = ParameterValue{100.0, ""};

    const auto field = evaluateNode(graph, "grain");
    REQUIRE(allSamplesWithin(*field, 90.0f, 110.0f));
    REQUIRE_FALSE(isConstant(*field));
}

TEST_CASE("white noise draws the same grain at any thread count",
          "[nodes][noise][determinism]") {
    Graph graph = squareGraph(resolution, worldSize);
    graph.addNode("grain", "Noise");

    REQUIRE(hashField(*evaluateNode(graph, "grain", 1))
            == hashField(*evaluateNode(graph, "grain", 8)));
}

/* ----------------------------------------------------------------- Flip -- */

TEST_CASE("a vertical flip turns the ramp upside down", "[nodes][flip]") {
    Graph graph = rampGraph();
    terrain::GraphNode& flip = graph.addNode("flip", "Flip");
    flip.parameters["axis"] = ParameterValue{1.0, ""};
    graph.connect("ramp", 0, "flip", 0);

    const auto source = evaluateNode(graph, "ramp");
    const auto flipped = evaluateNode(graph, "flip");
    REQUIRE(flipped->at(0, 0) == Approx(source->at(resolution - 1, 0)));
    REQUIRE(flipped->at(resolution - 1, 0) == Approx(source->at(0, 0)));
}

/* --------------------------------------------------------------- Repeat -- */

TEST_CASE("repeating twice starts the field again halfway", "[nodes][repeat]") {
    Graph graph = rampGraph();
    terrain::GraphNode& repeat = graph.addNode("repeat", "Repeat");
    repeat.parameters["repeatCount"] = ParameterValue{2.0, ""};
    repeat.parameters["mirror"] = ParameterValue{0.0, ""};
    graph.connect("ramp", 0, "repeat", 0);

    const auto source = evaluateNode(graph, "ramp");
    const auto tiled = evaluateNode(graph, "repeat");
    REQUIRE(tiled->at(0, 0) == Approx(source->at(0, 0)));
    REQUIRE(tiled->at(middle, 0) == Approx(source->at(0, 0)));
}

TEST_CASE("mirroring turns every other tile so the seams close", "[nodes][repeat]") {
    Graph graph = rampGraph();
    terrain::GraphNode& repeat = graph.addNode("repeat", "Repeat");
    repeat.parameters["repeatCount"] = ParameterValue{2.0, ""};
    repeat.parameters["mirror"] = ParameterValue{1.0, ""};
    graph.connect("ramp", 0, "repeat", 0);

    const auto source = evaluateNode(graph, "ramp");
    const auto tiled = evaluateNode(graph, "repeat");
    REQUIRE(tiled->at(middle, 0) == Approx(source->at(resolution - 1, 0)));
}

/* ------------------------------------------------------------- Pixelate -- */

TEST_CASE("pixelate leaves each block dead flat", "[nodes][pixelate]") {
    Graph graph = rampGraph();
    terrain::GraphNode& blocks = graph.addNode("blocks", "Pixelate");
    blocks.parameters["blockSize"] = ParameterValue{400.0, ""};
    graph.connect("ramp", 0, "blocks", 0);

    const auto field = evaluateNode(graph, "blocks");
    REQUIRE(field->at(0, 0) == Approx(field->at(3, 3)));
    REQUIRE(field->at(0, 0) != Approx(field->at(4, 4)));
}

/* ------------------------------------------------------------------ Edge -- */

TEST_CASE("an edge fade reaches the border and leaves the middle alone",
          "[nodes][edge]") {
    Graph graph = squareGraph(resolution, worldSize);
    terrain::GraphNode& ground = graph.addNode("ground", "Constant");
    ground.parameters["value"] = ParameterValue{100.0, ""};
    terrain::GraphNode& edge = graph.addNode("edge", "Edge");
    edge.parameters["width"] = ParameterValue{500.0, ""};
    edge.parameters["edgeValue"] = ParameterValue{0.0, ""};
    graph.connect("ground", 0, "edge", 0);

    const auto field = evaluateNode(graph, "edge");
    REQUIRE(field->at(0, 0) < 10.0f);
    REQUIRE(field->at(middle, middle) == Approx(100.0));
}
