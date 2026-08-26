#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <vector>

#include "node_test_support.h"

using Catch::Approx;
using terrain::Graph;
using terrain::Heightfield;
using terrain::ParameterValue;
using terrain::testing::evaluateNode;
using terrain::testing::squareGraph;

namespace {

constexpr int resolution = 64;
constexpr double worldSize = 6400.0;

terrain::GraphNode& addRamp(Graph& graph, const std::string& id, double endValue) {
    terrain::GraphNode& ramp = graph.addNode(id, "Gradient");
    ramp.parameters["direction"] = ParameterValue{0.0, ""};
    ramp.parameters["startValue"] = ParameterValue{0.0, ""};
    ramp.parameters["endValue"] = ParameterValue{endValue, ""};
    return ramp;
}

/// A cone: most of its cells sit low down, because area grows with radius.
/// That lopsidedness is what the histogram nodes are there to move.
terrain::GraphNode& addCone(Graph& graph, const std::string& id) {
    terrain::GraphNode& cone = graph.addNode(id, "RadialGradient");
    cone.parameters["radius"] = ParameterValue{3200.0, ""};
    cone.parameters["profile"] = ParameterValue{0.0, ""};
    cone.parameters["amplitude"] = ParameterValue{1000.0, ""};
    cone.parameters["offset"] = ParameterValue{0.0, ""};
    return cone;
}

float medianOf(const Heightfield& field) {
    std::vector<float> sorted(field.samples().begin(), field.samples().end());
    std::sort(sorted.begin(), sorted.end());
    return sorted[sorted.size() / 2];
}

/// Where the median sits inside the field's own range, as [0, 1].
float medianPosition(const Heightfield& field) {
    return (medianOf(field) - field.minimum()) / (field.maximum() - field.minimum());
}

}  // namespace

/* ------------------------------------------------------------ Autolevel -- */

TEST_CASE("autolevel stretches a field onto the floor and ceiling asked for",
          "[nodes][autolevel]") {
    Graph graph = squareGraph(resolution, worldSize);
    addRamp(graph, "ramp", 100.0);
    terrain::GraphNode& levels = graph.addNode("levels", "Autolevel");
    levels.parameters["targetLow"] = ParameterValue{-50.0, ""};
    levels.parameters["targetHigh"] = ParameterValue{50.0, ""};
    graph.connect("ramp", 0, "levels", 0);

    const auto field = evaluateNode(graph, "levels");
    REQUIRE(field->minimum() == Approx(-50.0).margin(0.1));
    REQUIRE(field->maximum() == Approx(50.0).margin(0.1));
}

TEST_CASE("autolevel ignores the tail the percentiles cut off", "[nodes][autolevel]") {
    Graph graph = squareGraph(resolution, worldSize);
    addRamp(graph, "ramp", 100.0);
    terrain::GraphNode& levels = graph.addNode("levels", "Autolevel");
    levels.parameters["lowPercentile"] = ParameterValue{10.0, ""};
    levels.parameters["highPercentile"] = ParameterValue{90.0, ""};
    levels.parameters["targetLow"] = ParameterValue{0.0, ""};
    levels.parameters["targetHigh"] = ParameterValue{1.0, ""};
    graph.connect("ramp", 0, "levels", 0);

    // A tenth of the ramp at each end is flattened onto its limit, so the
    // untouched middle covers the whole of the new range.
    const auto field = evaluateNode(graph, "levels");
    REQUIRE(field->minimum() == Approx(0.0));
    REQUIRE(field->maximum() == Approx(1.0));
    REQUIRE(field->at(0, 0) == Approx(1.0));
    REQUIRE(field->at(resolution - 1, 0) == Approx(0.0));
}

/* ------------------------------------------------------------- Equalize -- */

TEST_CASE("equalize moves a crowded field's median to the middle", "[nodes][equalize]") {
    Graph graph = squareGraph(resolution, worldSize);
    addCone(graph, "cone");
    graph.addNode("even", "Equalize");
    graph.connect("cone", 0, "even", 0);

    const auto before = evaluateNode(graph, "cone");
    const auto after = evaluateNode(graph, "even");
    REQUIRE(medianPosition(*before) < 0.35f);
    REQUIRE(medianPosition(*after) == Approx(0.5).margin(0.05));
}

TEST_CASE("equalize leaves the range where it found it", "[nodes][equalize]") {
    Graph graph = squareGraph(resolution, worldSize);
    addCone(graph, "cone");
    graph.addNode("even", "Equalize");
    graph.connect("cone", 0, "even", 0);

    const auto before = evaluateNode(graph, "cone");
    const auto after = evaluateNode(graph, "even");
    REQUIRE(after->minimum() == Approx(before->minimum()).margin(1.0));
    REQUIRE(after->maximum() == Approx(before->maximum()).margin(1.0));
}

/* ---------------------------------------------------------------- Match -- */

TEST_CASE("match takes on the reference's range", "[nodes][match]") {
    Graph graph = squareGraph(resolution, worldSize);
    addRamp(graph, "ramp", 100.0);
    addRamp(graph, "reference", 1000.0);
    graph.addNode("matched", "Match");
    graph.connect("ramp", 0, "matched", 0);
    graph.connect("reference", 0, "matched", 1);

    const auto field = evaluateNode(graph, "matched");
    const auto reference = evaluateNode(graph, "reference");
    REQUIRE(field->minimum() == Approx(reference->minimum()).margin(1.0));
    REQUIRE(field->maximum() == Approx(reference->maximum()).margin(1.0));
    // Both fields are evenly spread, so every cell lands at ten times itself.
    REQUIRE(field->at(32, 0) == Approx(10.0 * evaluateNode(graph, "ramp")->at(32, 0)).margin(2.0));
}

TEST_CASE("match at no amount changes nothing", "[nodes][match]") {
    Graph graph = squareGraph(resolution, worldSize);
    addRamp(graph, "ramp", 100.0);
    addRamp(graph, "reference", 1000.0);
    terrain::GraphNode& matched = graph.addNode("matched", "Match");
    matched.parameters["amount"] = ParameterValue{0.0, ""};
    graph.connect("ramp", 0, "matched", 0);
    graph.connect("reference", 0, "matched", 1);

    REQUIRE(evaluateNode(graph, "matched")->maximum() == Approx(100.0).margin(1.0));
}

/* ------------------------------------------------------------- SoftClip -- */

TEST_CASE("a soft clip approaches its limit without reaching it", "[nodes][softclip]") {
    Graph graph = squareGraph(resolution, worldSize);
    addRamp(graph, "ramp", 1000.0);
    terrain::GraphNode& clip = graph.addNode("clip", "SoftClip");
    clip.parameters["threshold"] = ParameterValue{500.0, ""};
    clip.parameters["softness"] = ParameterValue{100.0, ""};
    clip.parameters["side"] = ParameterValue{0.0, ""};
    graph.connect("ramp", 0, "clip", 0);

    const auto source = evaluateNode(graph, "ramp");
    const auto field = evaluateNode(graph, "clip");
    REQUIRE(field->maximum() > 500.0f);
    REQUIRE(field->maximum() < 600.0f);
    // Everything under the threshold passes through untouched.
    REQUIRE(field->at(resolution - 1, 0) == Approx(source->at(resolution - 1, 0)));
}

TEST_CASE("a soft clip holds the other side just as well", "[nodes][softclip]") {
    Graph graph = squareGraph(resolution, worldSize);
    addRamp(graph, "ramp", 1000.0);
    terrain::GraphNode& clip = graph.addNode("clip", "SoftClip");
    clip.parameters["threshold"] = ParameterValue{500.0, ""};
    clip.parameters["softness"] = ParameterValue{100.0, ""};
    clip.parameters["side"] = ParameterValue{1.0, ""};
    graph.connect("ramp", 0, "clip", 0);

    const auto field = evaluateNode(graph, "clip");
    REQUIRE(field->minimum() > 400.0f);
    REQUIRE(field->minimum() < 500.0f);
    REQUIRE(field->maximum() == Approx(evaluateNode(graph, "ramp")->maximum()));
}

/* -------------------------------------------------------------- Sharpen -- */

TEST_CASE("sharpening a flat field leaves it flat", "[nodes][sharpen]") {
    Graph graph = squareGraph(resolution, worldSize);
    terrain::GraphNode& ground = graph.addNode("ground", "Constant");
    ground.parameters["value"] = ParameterValue{250.0, ""};
    terrain::GraphNode& sharp = graph.addNode("sharp", "Sharpen");
    sharp.parameters["radius"] = ParameterValue{300.0, ""};
    graph.connect("ground", 0, "sharp", 0);

    const auto field = evaluateNode(graph, "sharp");
    REQUIRE(field->minimum() == Approx(250.0));
    REQUIRE(field->maximum() == Approx(250.0));
}

TEST_CASE("sharpening widens the range a blur would have narrowed",
          "[nodes][sharpen]") {
    Graph graph = squareGraph(resolution, worldSize);
    graph.addNode("ground", "Perlin");
    terrain::GraphNode& sharp = graph.addNode("sharp", "Sharpen");
    sharp.parameters["radius"] = ParameterValue{300.0, ""};
    sharp.parameters["amount"] = ParameterValue{1.0, ""};
    graph.connect("ground", 0, "sharp", 0);

    const auto source = evaluateNode(graph, "ground");
    const auto field = evaluateNode(graph, "sharp");
    REQUIRE(field->maximum() - field->minimum() > source->maximum() - source->minimum());
}

/* -------------------------------------------------------------- Compare -- */

TEST_CASE("compare reads the gap between two fields in meters", "[nodes][compare]") {
    Graph graph = squareGraph(resolution, worldSize);
    terrain::GraphNode& first = graph.addNode("first", "Constant");
    first.parameters["value"] = ParameterValue{10.0, ""};
    terrain::GraphNode& second = graph.addNode("second", "Constant");
    second.parameters["value"] = ParameterValue{4.0, ""};
    terrain::GraphNode& compare = graph.addNode("compare", "Compare");
    compare.parameters["scale"] = ParameterValue{0.0, ""};
    graph.connect("first", 0, "compare", 0);
    graph.connect("second", 0, "compare", 1);

    REQUIRE(evaluateNode(graph, "compare")->at(0, 0) == Approx(6.0));
}

TEST_CASE("a normalised comparison puts the widest gap at one", "[nodes][compare]") {
    Graph graph = squareGraph(resolution, worldSize);
    addRamp(graph, "ramp", 1000.0);
    terrain::GraphNode& flat = graph.addNode("flat", "Constant");
    flat.parameters["value"] = ParameterValue{0.0, ""};
    terrain::GraphNode& compare = graph.addNode("compare", "Compare");
    compare.parameters["scale"] = ParameterValue{1.0, ""};
    graph.connect("ramp", 0, "compare", 0);
    graph.connect("flat", 0, "compare", 1);

    const auto field = evaluateNode(graph, "compare");
    REQUIRE(field->maximum() == Approx(1.0));
    REQUIRE(field->minimum() >= 0.0f);
}
