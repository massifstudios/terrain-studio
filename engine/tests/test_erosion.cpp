#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <numbers>
#include <string>

#include "node_test_support.h"

using Catch::Approx;
using terrain::Graph;
using terrain::Heightfield;
using terrain::ParameterValue;
using terrain::testing::allSamplesWithin;
using terrain::testing::evaluateNode;
using terrain::testing::evaluateOutput;
using terrain::testing::hashField;
using terrain::testing::isConstant;
using terrain::testing::squareGraph;

namespace {

constexpr int resolution = 96;
constexpr double worldSize = 9600.0;

/// Rough ground to erode, made without touching the disk. The node under test
/// is added by the caller so it can set its own parameters first.
Graph ridgedGround(double amplitudeMeters) {
    Graph graph = squareGraph(resolution, worldSize);
    terrain::GraphNode& shape = graph.addNode("shape", "RidgedNoise");
    shape.parameters["featureSize"] = ParameterValue{3000.0, ""};
    shape.parameters["octaves"] = ParameterValue{5.0, ""};
    shape.parameters["amplitude"] = ParameterValue{amplitudeMeters, ""};
    return graph;
}

double totalHeight(const Heightfield& field) {
    double total = 0.0;
    for (const float sample : field.samples()) {
        total += sample;
    }
    return total;
}

float steepestSlopeDegrees(const Heightfield& field) {
    const int size = field.width();
    const float step = static_cast<float>(field.cellSizeMeters());
    float steepest = 0.0f;
    for (int row = 0; row + 1 < size; ++row) {
        for (int column = 0; column + 1 < size; ++column) {
            const float southward = std::abs(field.at(row, column) - field.at(row + 1, column));
            const float eastward = std::abs(field.at(row, column) - field.at(row, column + 1));
            steepest = std::max(steepest, std::atan(std::max(southward, eastward) / step));
        }
    }
    return steepest * (180.0f / std::numbers::pi_v<float>);
}

/// How much relief sits at the scale of a single cell. Gaining detail the
/// source never had is what this milestone is judged on.
double fineDetail(const Heightfield& field) {
    const int size = field.width();
    double total = 0.0;
    for (int row = 1; row + 1 < size; ++row) {
        for (int column = 1; column + 1 < size; ++column) {
            const double bend = field.at(row, column + 1) + field.at(row, column - 1)
                                + field.at(row + 1, column) + field.at(row - 1, column)
                                - 4.0 * field.at(row, column);
            total += std::abs(bend);
        }
    }
    return total / ((size - 2) * (size - 2));
}

Graph rainOnRidges(double density) {
    Graph graph = ridgedGround(1200.0);
    terrain::GraphNode& rain = graph.addNode("rain", "HydraulicErosion");
    rain.parameters["density"] = ParameterValue{density, ""};
    graph.connect("shape", 0, "rain", 0);
    return graph;
}

}  // namespace

/* -------------------------------------------------------------- thermal -- */

TEST_CASE("thermal erosion neither creates nor destroys material", "[erosion][thermal]") {
    Graph graph = ridgedGround(1200.0);
    graph.addNode("slump", "ThermalErosion");
    graph.connect("shape", 0, "slump", 0);

    const auto before = evaluateNode(graph, "shape");
    const auto after = evaluateNode(graph, "slump");

    // Nothing may leave across an edge, so the only difference is the rounding
    // the fixed-point accumulator introduces.
    REQUIRE(totalHeight(*after) == Approx(totalHeight(*before)).epsilon(1.0e-4));
}

TEST_CASE("thermal erosion takes the edge off the steepest ground", "[erosion][thermal]") {
    Graph graph = ridgedGround(2500.0);
    terrain::GraphNode& slump = graph.addNode("slump", "ThermalErosion");
    slump.parameters["talusAngle"] = ParameterValue{20.0, ""};
    slump.parameters["iterations"] = ParameterValue{80.0, ""};
    graph.connect("shape", 0, "slump", 0);

    const auto before = evaluateNode(graph, "shape");
    const auto after = evaluateNode(graph, "slump");

    REQUIRE(steepestSlopeDegrees(*after) < steepestSlopeDegrees(*before));
}

TEST_CASE("a thermal mask keeps the effect where it was asked for",
          "[erosion][thermal]") {
    Graph graph = ridgedGround(1200.0);
    terrain::GraphNode& band = graph.addNode("band", "HeightMask");
    band.parameters["minimumAltitude"] = ParameterValue{-9000.0, ""};
    band.parameters["maximumAltitude"] = ParameterValue{-8999.0, ""};
    band.parameters["falloff"] = ParameterValue{0.0, ""};
    graph.connect("shape", 0, "band", 0);

    graph.addNode("slump", "ThermalErosion");
    graph.connect("shape", 0, "slump", 0);
    graph.connect("band", 0, "slump", 1);

    REQUIRE(hashField(*evaluateNode(graph, "slump")) == hashField(*evaluateNode(graph, "shape")));
}

TEST_CASE("thermal erosion gives the same surface at one thread and at eight",
          "[erosion][thermal][determinism]") {
    Graph graph = ridgedGround(1200.0);
    graph.addNode("slump", "ThermalErosion");
    graph.connect("shape", 0, "slump", 0);

    REQUIRE(hashField(*evaluateNode(graph, "slump", 1))
            == hashField(*evaluateNode(graph, "slump", 8)));
}

/* -------------------------------------------------------------- deposit -- */

TEST_CASE("deposit lays down exactly the sediment it was given", "[erosion][deposit]") {
    Graph graph = ridgedGround(1200.0);
    terrain::GraphNode& fill = graph.addNode("fill", "Deposit");
    fill.parameters["amount"] = ParameterValue{50.0, ""};
    fill.parameters["iterations"] = ParameterValue{30.0, ""};
    graph.connect("shape", 0, "fill", 0);

    const auto before = evaluateNode(graph, "shape");
    const auto after = evaluateNode(graph, "fill");

    REQUIRE(totalHeight(*after) - totalHeight(*before)
            == Approx(50.0 * before->sampleCount()).epsilon(1.0e-3));
}

TEST_CASE("deposit settles into the low ground", "[erosion][deposit]") {
    Graph graph = ridgedGround(1200.0);
    terrain::GraphNode& fill = graph.addNode("fill", "Deposit");
    fill.parameters["amount"] = ParameterValue{60.0, ""};
    fill.parameters["iterations"] = ParameterValue{40.0, ""};
    graph.connect("shape", 0, "fill", 0);

    const auto before = evaluateNode(graph, "shape");
    const auto after = evaluateNode(graph, "fill");
    const float middle = (before->minimum() + before->maximum()) * 0.5f;

    double lowGround = 0.0;
    double highGround = 0.0;
    int lowCells = 0;
    int highCells = 0;
    for (std::size_t index = 0; index < before->sampleCount(); ++index) {
        const double thickness = after->data()[index] - before->data()[index];
        if (before->data()[index] < middle) {
            lowGround += thickness;
            lowCells += 1;
            continue;
        }
        highGround += thickness;
        highCells += 1;
    }

    // Deeper on average down in the valleys than up on the ridges.
    REQUIRE(lowGround / lowCells > highGround / highCells);
}

TEST_CASE("the deposit thickness comes out as a mask", "[erosion][deposit]") {
    Graph graph = ridgedGround(1200.0);
    graph.addNode("fill", "Deposit");
    graph.connect("shape", 0, "fill", 0);

    const auto thickness = evaluateOutput(graph, "fill", 1);
    REQUIRE(thickness->semantics() == terrain::FieldSemantics::Mask);
    REQUIRE(allSamplesWithin(*thickness, 0.0f, 1.0f));
    REQUIRE_FALSE(isConstant(*thickness));
}

/* ------------------------------------------------------------ hydraulic -- */

TEST_CASE("rain with no droplets leaves the surface alone", "[erosion][hydraulic]") {
    const Graph graph = rainOnRidges(0.0);
    REQUIRE(hashField(*evaluateNode(graph, "rain")) == hashField(*evaluateNode(graph, "shape")));
}

TEST_CASE("rain cuts detail the source never had", "[erosion][hydraulic]") {
    const Graph graph = rainOnRidges(1.0);

    const auto before = evaluateNode(graph, "shape");
    const auto after = evaluateNode(graph, "rain");

    REQUIRE(fineDetail(*after) > fineDetail(*before));
}

TEST_CASE("rain on gentle ground does not build towers", "[erosion][hydraulic]") {
    // A coarse graph asks for a channel narrower than one of its own cells.
    // Left to concentrate on a single cell, a droplet drops everything it
    // carries in one place, and on ground with little relief that dwarfs the
    // terrain — the failure the brush has a floor to prevent.
    Graph graph = squareGraph(128, 12800.0);
    terrain::GraphNode& shape = graph.addNode("shape", "Perlin");
    shape.parameters["featureSize"] = ParameterValue{6000.0, ""};
    shape.parameters["octaves"] = ParameterValue{3.0, ""};
    shape.parameters["amplitude"] = ParameterValue{900.0, ""};

    terrain::GraphNode& rain = graph.addNode("rain", "HydraulicErosion");
    rain.parameters["radius"] = ParameterValue{10.0, ""};
    graph.connect("shape", 0, "rain", 0);

    const auto before = evaluateNode(graph, "shape");
    const auto after = evaluateNode(graph, "rain");

    // One-sided on purpose: rain may lower a summit and may fill a hollow, but
    // it may never raise ground above the highest it started, nor cut below the
    // lowest. Either of those is a spike, not erosion.
    const float relief = before->maximum() - before->minimum();
    REQUIRE(after->maximum() < before->maximum() + relief * 0.02f);
    REQUIRE(after->minimum() > before->minimum() - relief * 0.02f);
}

TEST_CASE("rain gives the same surface at one thread and at eight",
          "[erosion][hydraulic][determinism]") {
    const Graph graph = rainOnRidges(1.0);
    REQUIRE(hashField(*evaluateNode(graph, "rain", 1))
            == hashField(*evaluateNode(graph, "rain", 8)));
}

TEST_CASE("rain gives the same surface twice over", "[erosion][hydraulic][determinism]") {
    const Graph graph = rainOnRidges(1.0);
    REQUIRE(hashField(*evaluateNode(graph, "rain", 4))
            == hashField(*evaluateNode(graph, "rain", 4)));
}

TEST_CASE("the secondary outputs report where material moved and water went",
          "[erosion][hydraulic]") {
    const Graph graph = rainOnRidges(1.0);

    for (const int port : {1, 2, 3}) {
        const auto mask = evaluateOutput(graph, "rain", port);
        REQUIRE(mask->semantics() == terrain::FieldSemantics::Mask);
        REQUIRE(allSamplesWithin(*mask, 0.0f, 1.0f));
        REQUIRE_FALSE(isConstant(*mask));
    }
}

TEST_CASE("the erosion seed varies that node alone", "[erosion][hydraulic][determinism]") {
    const auto rainWithSeed = [](double seedValue) {
        Graph graph = rainOnRidges(1.0);
        Graph seeded = squareGraph(resolution, worldSize);
        terrain::GraphNode& shape = seeded.addNode("shape", "RidgedNoise");
        shape.parameters["featureSize"] = ParameterValue{3000.0, ""};
        shape.parameters["octaves"] = ParameterValue{5.0, ""};
        shape.parameters["amplitude"] = ParameterValue{1200.0, ""};
        terrain::GraphNode& rain = seeded.addNode("rain", "HydraulicErosion");
        rain.parameters["density"] = ParameterValue{1.0, ""};
        rain.parameters["seed"] = ParameterValue{seedValue, ""};
        seeded.connect("shape", 0, "rain", 0);
        return hashField(*evaluateNode(seeded, "rain"));
    };

    REQUIRE(rainWithSeed(0.0) != rainWithSeed(9.0));
}

/* ------------------------------------------------------------- FlowMask -- */

TEST_CASE("flow gathers downhill", "[erosion][flow]") {
    Graph graph = squareGraph(resolution, worldSize);
    terrain::GraphNode& ramp = graph.addNode("ramp", "Gradient");
    ramp.parameters["direction"] = ParameterValue{0.0, ""};
    ramp.parameters["startValue"] = ParameterValue{0.0, ""};
    ramp.parameters["endValue"] = ParameterValue{2000.0, ""};

    graph.addNode("flow", "FlowMask");
    graph.connect("ramp", 0, "flow", 0);

    const auto mask = evaluateNode(graph, "flow");
    REQUIRE(mask->semantics() == terrain::FieldSemantics::Mask);
    REQUIRE(allSamplesWithin(*mask, 0.0f, 1.0f));
    // The ramp rises northwards, so every drop ends up along the south edge.
    REQUIRE(mask->at(resolution - 1, resolution / 2) > mask->at(0, resolution / 2));
}

TEST_CASE("flow accumulation is the same however many threads are used",
          "[erosion][flow][determinism]") {
    Graph graph = ridgedGround(1200.0);
    graph.addNode("flow", "FlowMask");
    graph.connect("shape", 0, "flow", 0);

    REQUIRE(hashField(*evaluateNode(graph, "flow", 1))
            == hashField(*evaluateNode(graph, "flow", 8)));
}

/* ---------------------------------------------------------- the milestone -- */

TEST_CASE("a refinement graph adds relief to a coarse elevation model",
          "[erosion][acceptance]") {
    // The acceptance shape: import at a coarse ground resolution, then erode
    // through a slope mask so the effect lands on the hillsides.
    Graph graph = squareGraph(128, 12800.0);
    terrain::GraphNode& shape = graph.addNode("shape", "Perlin");
    shape.parameters["featureSize"] = ParameterValue{6000.0, ""};
    shape.parameters["octaves"] = ParameterValue{3.0, ""};
    shape.parameters["amplitude"] = ParameterValue{900.0, ""};

    terrain::GraphNode& steep = graph.addNode("steep", "SlopeMask");
    steep.parameters["minimumSlope"] = ParameterValue{5.0, ""};
    steep.parameters["maximumSlope"] = ParameterValue{90.0, ""};
    graph.connect("shape", 0, "steep", 0);

    terrain::GraphNode& slump = graph.addNode("slump", "ThermalErosion");
    slump.parameters["talusAngle"] = ParameterValue{34.0, ""};
    slump.parameters["iterations"] = ParameterValue{25.0, ""};
    graph.connect("shape", 0, "slump", 0);
    graph.connect("steep", 0, "slump", 1);

    terrain::GraphNode& rain = graph.addNode("rain", "HydraulicErosion");
    rain.parameters["density"] = ParameterValue{1.5, ""};
    graph.connect("slump", 0, "rain", 0);
    graph.connect("steep", 0, "rain", 1);

    const auto before = evaluateNode(graph, "shape");
    const auto after = evaluateNode(graph, "rain");

    REQUIRE(fineDetail(*after) > fineDetail(*before) * 2.0);
    // The mountain is still the same mountain, not a different one.
    const float relief = before->maximum() - before->minimum();
    REQUIRE(after->maximum() < before->maximum() + relief * 0.02f);
    REQUIRE(after->minimum() > before->minimum() - relief * 0.02f);
}
