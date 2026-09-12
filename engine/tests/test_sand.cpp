#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "node_test_support.h"

using terrain::Graph;
using terrain::Heightfield;
using terrain::ParameterValue;
using terrain::testing::evaluateNode;
using terrain::testing::hashField;
using terrain::testing::squareGraph;

namespace {

/// A field of one constant value, which is what a ripple pattern wants under
/// it when the test is about the pattern and nothing else.
terrain::GraphNode& addFlatGround(Graph& graph, const std::string& id, double altitudeMeters) {
    terrain::GraphNode& flat = graph.addNode(id, "Perlin");
    flat.parameters["amplitude"] = ParameterValue{0.0, ""};
    flat.parameters["offset"] = ParameterValue{altitudeMeters, ""};
    return flat;
}

/// Sand over flat ground, with the dials that scatter the pattern turned off
/// so what each test measures is the one thing it names.
terrain::GraphNode& addPlainSand(Graph& graph, const std::string& id) {
    addFlatGround(graph, id + "Ground", 100.0);
    terrain::GraphNode& sand = graph.addNode(id, "Sand");
    sand.parameters["scale"] = ParameterValue{200.0, ""};
    sand.parameters["chaos"] = ParameterValue{0.0, ""};
    sand.parameters["secondaryScale"] = ParameterValue{0.0, ""};
    sand.parameters["warpByTerrain"] = ParameterValue{0.0, ""};
    sand.parameters["height"] = ParameterValue{2.0, ""};
    graph.connect(id + "Ground", 0, id, 0);
    return sand;
}

constexpr int duneTypeCount = 4;

terrain::GraphNode& addDuneSea(Graph& graph, const std::string& id, int duneType) {
    terrain::GraphNode& dunes = graph.addNode(id, "DuneSea");
    dunes.parameters["scale"] = ParameterValue{400.0, ""};
    dunes.parameters["duneType"] = ParameterValue{static_cast<double>(duneType), ""};
    dunes.parameters["chaos"] = ParameterValue{0.0, ""};
    dunes.parameters["undulation"] = ParameterValue{0.0, ""};
    dunes.parameters["direction"] = ParameterValue{90.0, ""};
    dunes.parameters["height"] = ParameterValue{80.0, ""};
    dunes.parameters["softness"] = ParameterValue{0.0, ""};
    dunes.parameters["sharpness"] = ParameterValue{0.0, ""};
    dunes.parameters["floor"] = ParameterValue{0.0, ""};
    return dunes;
}

float relief(const Heightfield& field) {
    return field.maximum() - field.minimum();
}

/// The pattern a filter added, cell by cell.
std::vector<float> difference(const Heightfield& after, const Heightfield& before) {
    std::vector<float> pattern(after.sampleCount());
    for (std::size_t index = 0; index < pattern.size(); ++index) {
        pattern[index] = after.samples()[index] - before.samples()[index];
    }
    return pattern;
}

float meanOf(const std::vector<float>& values) {
    double total = 0.0;
    for (const float value : values) {
        total += value;
    }
    return static_cast<float>(total / values.size());
}

float spanOf(const std::vector<float>& values) {
    const auto bounds = std::minmax_element(values.begin(), values.end());
    return *bounds.second - *bounds.first;
}

/// Mean step between neighbouring cells along the rows — how fast the field
/// changes when walking east.
float meanEastwardStep(const Heightfield& field) {
    double total = 0.0;
    for (int row = 0; row < field.height(); ++row) {
        for (int column = 1; column < field.width(); ++column) {
            total += std::abs(field.at(row, column) - field.at(row, column - 1));
        }
    }
    return static_cast<float>(total / (field.height() * (field.width() - 1)));
}

/// The same walking north, which on a grid whose row 0 is north means walking
/// up the rows.
float meanNorthwardStep(const Heightfield& field) {
    double total = 0.0;
    for (int row = 1; row < field.height(); ++row) {
        for (int column = 0; column < field.width(); ++column) {
            total += std::abs(field.at(row, column) - field.at(row - 1, column));
        }
    }
    return static_cast<float>(total / ((field.height() - 1) * field.width()));
}

/// The mean rise and the mean fall met walking east along every row. A dune
/// with its slip face downwind rises slowly and drops fast.
struct EastwardSteps {
    float meanRise = 0.0f;
    float meanFall = 0.0f;
};

EastwardSteps eastwardSteps(const Heightfield& field) {
    double rise = 0.0;
    double fall = 0.0;
    int rises = 0;
    int falls = 0;
    for (int row = 0; row < field.height(); ++row) {
        for (int column = 1; column < field.width(); ++column) {
            const float step = field.at(row, column) - field.at(row, column - 1);
            rise += std::max(step, 0.0f);
            fall += std::max(-step, 0.0f);
            rises += static_cast<int>(step > 0.0f);
            falls += static_cast<int>(step < 0.0f);
        }
    }
    return EastwardSteps{static_cast<float>(rise / std::max(rises, 1)),
                         static_cast<float>(fall / std::max(falls, 1))};
}

/**
 * How far a field drawn at twice the resolution drifts from the same field
 * drawn coarsely, once each block of four fine cells is averaged back down.
 * The two block centres sit on the same ground, so a generator that samples in
 * world meters agrees here whatever the cell size.
 */
float meanDownsampledDrift(const Heightfield& coarse, const Heightfield& fine) {
    double total = 0.0;
    for (int row = 0; row < coarse.height(); ++row) {
        for (int column = 0; column < coarse.width(); ++column) {
            const float block =
                (fine.at(row * 2, column * 2) + fine.at(row * 2, column * 2 + 1)
                 + fine.at(row * 2 + 1, column * 2) + fine.at(row * 2 + 1, column * 2 + 1))
                * 0.25f;
            total += std::abs(block - coarse.at(row, column));
        }
    }
    return static_cast<float>(total / coarse.sampleCount());
}

}  // namespace

/* ------------------------------------------------------------------ Sand -- */

TEST_CASE("sand lays ripples as tall as the height it is given", "[sand]") {
    Graph graph = squareGraph(128, 2560.0);
    addPlainSand(graph, "sand");

    const auto ground = evaluateNode(graph, "sandGround");
    const auto sanded = evaluateNode(graph, "sand");
    REQUIRE(sanded != nullptr);

    const std::vector<float> pattern = difference(*sanded, *ground);
    REQUIRE(spanOf(pattern) <= 2.0f);
    REQUIRE(spanOf(pattern) > 1.4f);
}

TEST_CASE("doubling the sand height doubles the relief it adds", "[sand]") {
    Graph graph = squareGraph(128, 2560.0);
    addPlainSand(graph, "sand");
    const float lowRelief = relief(*evaluateNode(graph, "sand"));

    graph.find("sand")->parameters["height"] = ParameterValue{4.0, ""};
    const float highRelief = relief(*evaluateNode(graph, "sand"));
    REQUIRE(highRelief > lowRelief * 1.99f);
    REQUIRE(highRelief < lowRelief * 2.01f);
}

TEST_CASE("sand ripples ride on the ground rather than lifting it", "[sand]") {
    Graph graph = squareGraph(128, 2560.0);
    addPlainSand(graph, "sand");

    const std::vector<float> pattern =
        difference(*evaluateNode(graph, "sand"), *evaluateNode(graph, "sandGround"));
    REQUIRE(std::abs(meanOf(pattern)) < 0.02f);
}

TEST_CASE("sand ripples run across the wind they are given", "[sand]") {
    Graph graph = squareGraph(128, 2560.0);
    addPlainSand(graph, "eastward");
    graph.find("eastward")->parameters["direction"] = ParameterValue{90.0, ""};
    addPlainSand(graph, "northward");
    graph.find("northward")->parameters["direction"] = ParameterValue{0.0, ""};

    const auto eastward = evaluateNode(graph, "eastward");
    const auto northward = evaluateNode(graph, "northward");

    REQUIRE(meanEastwardStep(*eastward) > meanNorthwardStep(*eastward) * 5.0f);
    REQUIRE(meanNorthwardStep(*northward) > meanEastwardStep(*northward) * 5.0f);
}

TEST_CASE("a mask held at zero leaves the surface sand never reached", "[sand]") {
    Graph graph = squareGraph(64, 2560.0);
    addPlainSand(graph, "sand");
    addFlatGround(graph, "noMask", 0.0);
    graph.connect("noMask", 0, "sand", 1);

    const auto ground = evaluateNode(graph, "sandGround");
    const auto sanded = evaluateNode(graph, "sand");
    REQUIRE(hashField(*sanded) == hashField(*ground));
}

TEST_CASE("sand bends its ripples along the ground it lies on", "[sand]") {
    Graph graph = squareGraph(128, 2560.0);
    graph.addNode("hills", "Perlin").parameters["featureSize"] = ParameterValue{1200.0, ""};
    terrain::GraphNode& sand = graph.addNode("sand", "Sand");
    sand.parameters["scale"] = ParameterValue{200.0, ""};
    sand.parameters["chaos"] = ParameterValue{0.0, ""};
    sand.parameters["warpByTerrain"] = ParameterValue{1.0, ""};
    graph.connect("hills", 0, "sand", 0);

    terrain::GraphNode& straight = graph.addNode("straight", "Sand");
    straight.parameters = sand.parameters;
    straight.parameters["warpByTerrain"] = ParameterValue{0.0, ""};
    graph.connect("hills", 0, "straight", 0);

    REQUIRE(hashField(*evaluateNode(graph, "sand")) != hashField(*evaluateNode(graph, "straight")));
}

TEST_CASE("sand draws the same ripples at any thread count", "[sand]") {
    Graph graph = squareGraph(128, 2560.0);
    addPlainSand(graph, "sand");
    graph.find("sand")->parameters["chaos"] = ParameterValue{0.5, ""};
    graph.find("sand")->parameters["secondaryScale"] = ParameterValue{0.4, ""};

    REQUIRE(hashField(*evaluateNode(graph, "sand", 1))
            == hashField(*evaluateNode(graph, "sand", 8)));
}

TEST_CASE("rolling the sand seed changes the ripples", "[sand]") {
    Graph graph = squareGraph(128, 2560.0);
    addPlainSand(graph, "sand");
    graph.find("sand")->parameters["chaos"] = ParameterValue{0.5, ""};
    const std::uint64_t first = hashField(*evaluateNode(graph, "sand"));

    graph.find("sand")->parameters["seed"] = ParameterValue{7.0, ""};
    REQUIRE(hashField(*evaluateNode(graph, "sand")) != first);
}

TEST_CASE("sand draws the same ripple field at any resolution", "[sand]") {
    Graph coarse = squareGraph(128, 5120.0);
    addPlainSand(coarse, "sand");
    coarse.find("sand")->parameters["scale"] = ParameterValue{800.0, ""};
    coarse.find("sand")->parameters["softness"] = ParameterValue{1.0, ""};
    Graph fine = coarse;
    fine.settings.resolution = 256;

    const auto coarseField = evaluateNode(coarse, "sand");
    const auto fineField = evaluateNode(fine, "sand");
    REQUIRE(meanDownsampledDrift(*coarseField, *fineField) < 0.005f);
}

/* --------------------------------------------------------------- DuneSea -- */

TEST_CASE("every kind of dune stands on its floor and rises to its height", "[sand]") {
    for (int duneType = 0; duneType < duneTypeCount; ++duneType) {
        INFO("dune type " << duneType);
        Graph graph = squareGraph(192, 3200.0);
        addDuneSea(graph, "dunes", duneType);
        graph.find("dunes")->parameters["floor"] = ParameterValue{25.0, ""};

        const auto field = evaluateNode(graph, "dunes");
        REQUIRE(field != nullptr);
        REQUIRE(field->minimum() >= 25.0f - 1.0e-3f);
        REQUIRE(field->minimum() < 25.0f + 8.0f);
        REQUIRE(field->maximum() <= 25.0f + 80.0f + 1.0e-3f);
        REQUIRE(field->maximum() > 25.0f + 64.0f);
    }
}

TEST_CASE("a transverse dune drops down its lee face and climbs its windward one",
          "[sand]") {
    Graph graph = squareGraph(192, 3200.0);
    addDuneSea(graph, "dunes", 0);

    const EastwardSteps steps = eastwardSteps(*evaluateNode(graph, "dunes"));
    REQUIRE(steps.meanFall > steps.meanRise * 3.0f);
}

TEST_CASE("a mask held at zero leaves the dune field flat on its floor", "[sand]") {
    Graph graph = squareGraph(64, 3200.0);
    addDuneSea(graph, "dunes", 0);
    graph.find("dunes")->parameters["floor"] = ParameterValue{12.0, ""};
    addFlatGround(graph, "noMask", 0.0);
    graph.connect("noMask", 0, "dunes", 0);

    const auto field = evaluateNode(graph, "dunes");
    REQUIRE(field->maximum() - field->minimum() < 1.0e-4f);
    REQUIRE(field->minimum() > 12.0f - 1.0e-4f);
}

TEST_CASE("a dune sea draws the same field at any resolution", "[sand]") {
    Graph coarse = squareGraph(128, 6400.0);
    addDuneSea(coarse, "dunes", 0);
    coarse.find("dunes")->parameters["scale"] = ParameterValue{1600.0, ""};
    coarse.find("dunes")->parameters["softness"] = ParameterValue{1.0, ""};
    coarse.find("dunes")->parameters["undulation"] = ParameterValue{2.0, ""};
    Graph fine = coarse;
    fine.settings.resolution = 256;

    const auto coarseField = evaluateNode(coarse, "dunes");
    const auto fineField = evaluateNode(fine, "dunes");
    REQUIRE(meanDownsampledDrift(*coarseField, *fineField) < 0.15f);
}

TEST_CASE("a dune sea draws the same field at any thread count", "[sand]") {
    for (int duneType = 0; duneType < duneTypeCount; ++duneType) {
        INFO("dune type " << duneType);
        Graph graph = squareGraph(128, 3200.0);
        addDuneSea(graph, "dunes", duneType);
        graph.find("dunes")->parameters["chaos"] = ParameterValue{3.0, ""};
        graph.find("dunes")->parameters["undulation"] = ParameterValue{2.0, ""};

        REQUIRE(hashField(*evaluateNode(graph, "dunes", 1))
                == hashField(*evaluateNode(graph, "dunes", 8)));
    }
}

TEST_CASE("rolling the dune sea seed changes the field", "[sand]") {
    for (int duneType = 0; duneType < duneTypeCount; ++duneType) {
        INFO("dune type " << duneType);
        Graph graph = squareGraph(128, 3200.0);
        addDuneSea(graph, "dunes", duneType);
        graph.find("dunes")->parameters["chaos"] = ParameterValue{2.0, ""};
        const std::uint64_t first = hashField(*evaluateNode(graph, "dunes"));

        graph.find("dunes")->parameters["seed"] = ParameterValue{31.0, ""};
        REQUIRE(hashField(*evaluateNode(graph, "dunes")) != first);
    }
}
