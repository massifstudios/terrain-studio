#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <string>

#include "node_test_support.h"
#include "terrain/color_space.h"

using Catch::Approx;
using terrain::Graph;
using terrain::HslColor;
using terrain::ParameterValue;
using terrain::RgbColor;
using terrain::testing::evaluateColor;
using terrain::testing::evaluateNode;
using terrain::testing::evaluateOutput;
using terrain::testing::squareGraph;

namespace {

constexpr int resolution = 32;
constexpr double worldSize = 3200.0;

/// A colour map of one flat colour, so every assertion below reads a single
/// value rather than a gradient.
Graph flatColorGraph(const std::string& hex) {
    Graph graph = squareGraph(resolution, worldSize);
    graph.addNode("ground", "Perlin");
    terrain::GraphNode& colors = graph.addNode("colors", "ColorRamp");
    colors.parameters["ramp"] = ParameterValue{0.0, "0:" + hex + ";1:" + hex};
    graph.connect("ground", 0, "colors", 0);
    return graph;
}

}  // namespace

/* ------------------------------------------------------------ HSL maths -- */

TEST_CASE("a colour survives a trip through hue, saturation and lightness",
          "[color][hsl]") {
    const RgbColor original{0.2f, 0.7f, 0.45f};
    const RgbColor returned = terrain::toRgb(terrain::toHsl(original));

    REQUIRE(returned.red == Approx(original.red).margin(1.0e-4));
    REQUIRE(returned.green == Approx(original.green).margin(1.0e-4));
    REQUIRE(returned.blue == Approx(original.blue).margin(1.0e-4));
}

TEST_CASE("a grey has no hue and no saturation", "[color][hsl]") {
    const HslColor grey = terrain::toHsl(RgbColor{0.5f, 0.5f, 0.5f});

    REQUIRE(grey.saturation == Approx(0.0));
    REQUIRE(grey.hue == Approx(0.0));
    REQUIRE(grey.lightness == Approx(0.5));
}

/* ------------------------------------------------------------------ HSL -- */

TEST_CASE("half a turn of the hue wheel takes red to cyan", "[nodes][hsl]") {
    Graph graph = flatColorGraph("#ff0000");
    terrain::GraphNode& adjust = graph.addNode("adjust", "HSL");
    adjust.parameters["hueShift"] = ParameterValue{180.0, ""};
    graph.connect("colors", 0, "adjust", 0);

    const auto colors = evaluateColor(graph, "adjust");
    REQUIRE(colors != nullptr);
    REQUIRE(colors->at(0, 0)[0] == Approx(0.0).margin(1.0e-5));
    REQUIRE(colors->at(0, 0)[1] == Approx(1.0).margin(1.0e-5));
    REQUIRE(colors->at(0, 0)[2] == Approx(1.0).margin(1.0e-5));
}

TEST_CASE("no saturation leaves grey behind", "[nodes][hsl]") {
    Graph graph = flatColorGraph("#ff0000");
    terrain::GraphNode& adjust = graph.addNode("adjust", "HSL");
    adjust.parameters["saturation"] = ParameterValue{0.0, ""};
    graph.connect("colors", 0, "adjust", 0);

    const auto colors = evaluateColor(graph, "adjust");
    REQUIRE(colors->at(0, 0)[0] == Approx(colors->at(0, 0)[1]));
    REQUIRE(colors->at(0, 0)[1] == Approx(colors->at(0, 0)[2]));
}

/* ---------------------------------------------------------------- Gamma -- */

TEST_CASE("gamma bends the midtones and leaves the ends alone", "[nodes][gamma]") {
    Graph graph = flatColorGraph("#808080");
    terrain::GraphNode& gamma = graph.addNode("gamma", "Gamma");
    gamma.parameters["gamma"] = ParameterValue{2.0, ""};
    graph.connect("colors", 0, "gamma", 0);

    const float grey = 128.0f / 255.0f;
    const auto colors = evaluateColor(graph, "gamma");
    REQUIRE(colors->at(0, 0)[0] == Approx(grey * grey).margin(1.0e-5));
}

/* --------------------------------------------------- RGBSplit, RGBMerge -- */

TEST_CASE("splitting a colour map and merging it back returns the original",
          "[nodes][rgbsplit]") {
    Graph graph = flatColorGraph("#3399cc");
    graph.addNode("split", "RGBSplit");
    graph.addNode("merge", "RGBMerge");
    graph.connect("colors", 0, "split", 0);
    for (int channel = 0; channel < 3; ++channel) {
        graph.connect("split", channel, "merge", channel);
    }

    const auto original = evaluateColor(graph, "colors");
    const auto returned = evaluateColor(graph, "merge");
    for (int channel = 0; channel < 3; ++channel) {
        REQUIRE(returned->at(0, 0)[channel] == Approx(original->at(0, 0)[channel]));
    }
}

TEST_CASE("a split channel reads as the channel it came from", "[nodes][rgbsplit]") {
    Graph graph = flatColorGraph("#3399cc");
    graph.addNode("split", "RGBSplit");
    graph.connect("colors", 0, "split", 0);

    REQUIRE(evaluateOutput(graph, "split", 0)->at(0, 0) == Approx(0x33 / 255.0));
    REQUIRE(evaluateOutput(graph, "split", 1)->at(0, 0) == Approx(0x99 / 255.0));
    REQUIRE(evaluateOutput(graph, "split", 2)->at(0, 0) == Approx(0xcc / 255.0));
}

TEST_CASE("an unconnected channel merges as nothing", "[nodes][rgbmerge]") {
    Graph graph = flatColorGraph("#3399cc");
    graph.addNode("split", "RGBSplit");
    graph.addNode("merge", "RGBMerge");
    graph.connect("colors", 0, "split", 0);
    graph.connect("split", 0, "merge", 0);

    const auto returned = evaluateColor(graph, "merge");
    REQUIRE(returned->at(0, 0)[0] == Approx(0x33 / 255.0));
    REQUIRE(returned->at(0, 0)[1] == Approx(0.0));
    REQUIRE(returned->at(0, 0)[2] == Approx(0.0));
}

/* ------------------------------------------------------- ColorThreshold -- */

TEST_CASE("a colour threshold selects on the channel it was pointed at",
          "[nodes][colorthreshold]") {
    Graph graph = flatColorGraph("#ff0000");
    terrain::GraphNode& onRed = graph.addNode("onRed", "ColorThreshold");
    onRed.parameters["channel"] = ParameterValue{1.0, ""};
    onRed.parameters["low"] = ParameterValue{0.9, ""};
    onRed.parameters["high"] = ParameterValue{1.0, ""};
    onRed.parameters["falloff"] = ParameterValue{0.0, ""};
    graph.connect("colors", 0, "onRed", 0);

    terrain::GraphNode& onGreen = graph.addNode("onGreen", "ColorThreshold");
    onGreen.parameters["channel"] = ParameterValue{2.0, ""};
    onGreen.parameters["low"] = ParameterValue{0.9, ""};
    onGreen.parameters["high"] = ParameterValue{1.0, ""};
    onGreen.parameters["falloff"] = ParameterValue{0.0, ""};
    graph.connect("colors", 0, "onGreen", 0);

    REQUIRE(evaluateNode(graph, "onRed")->at(0, 0) == Approx(1.0));
    REQUIRE(evaluateNode(graph, "onGreen")->at(0, 0) == Approx(0.0));
}
