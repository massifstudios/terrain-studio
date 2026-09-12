#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <string>

#include "node_test_support.h"
#include "terrain/color_field.h"

using Catch::Approx;
using terrain::ColorField;
using terrain::Graph;
using terrain::Heightfield;
using terrain::ParameterValue;
using terrain::testing::allSamplesWithin;
using terrain::testing::evaluateColor;
using terrain::testing::evaluateNode;
using terrain::testing::hashField;
using terrain::testing::isConstant;
using terrain::testing::squareGraph;

namespace {

constexpr int resolution = 64;
constexpr double worldSize = 6400.0;
constexpr int summitRow = resolution / 2;
constexpr int summitColumn = resolution / 2;

/// A single massif on an otherwise flat plain: a summit in the middle, flanks
/// steeper than any settle angle, and open ground all around it.
Graph mountainGround() {
    Graph graph = squareGraph(resolution, worldSize);
    terrain::GraphNode& ground = graph.addNode("ground", "Mountain");
    ground.parameters["height"] = ParameterValue{1200.0, ""};
    ground.parameters["radius"] = ParameterValue{1900.0, ""};
    return graph;
}

/// The same massif turned upside down, so the middle is a pit walled in on
/// every side and the ground around it is open sky.
Graph pitGround() {
    Graph graph = squareGraph(resolution, worldSize);
    terrain::GraphNode& massif = graph.addNode("massif", "Mountain");
    massif.parameters["height"] = ParameterValue{1200.0, ""};
    massif.parameters["radius"] = ParameterValue{1900.0, ""};
    terrain::GraphNode& plain = graph.addNode("plain", "Constant");
    plain.parameters["value"] = ParameterValue{0.0, ""};

    graph.addNode("ground", "Subtract");
    graph.connect("plain", 0, "ground", 0);
    graph.connect("massif", 0, "ground", 1);
    return graph;
}

terrain::GraphNode& addDerived(Graph& graph, const std::string& typeName) {
    terrain::GraphNode& node = graph.addNode("derived", typeName);
    graph.connect("ground", 0, "derived", 0);
    return node;
}

double meanOf(const Heightfield& field) {
    double total = 0.0;
    for (const float sample : field.samples()) {
        total += sample;
    }
    return total / field.samples().size();
}

/// The mask averaged over the cells whose ground height falls inside a band,
/// so a test can ask what a node did to the high ground and to the low ground
/// separately.
double meanWhereHeightBetween(const Heightfield& mask, const Heightfield& ground, float low,
                              float high) {
    double total = 0.0;
    int counted = 0;
    for (int row = 0; row < mask.width(); ++row) {
        for (int column = 0; column < mask.width(); ++column) {
            const float height = ground.at(row, column);
            if (height < low || height > high) {
                continue;
            }
            total += mask.at(row, column);
            ++counted;
        }
    }
    REQUIRE(counted > 0);
    return total / counted;
}

/// Positive in a hollow, negative over a rise: the discrete Laplacian, worked
/// out here so a test does not have to trust the engine's own reading.
double hollownessAt(const Heightfield& field, int row, int column) {
    return field.at(row, column + 1) + field.at(row, column - 1) + field.at(row + 1, column)
           + field.at(row - 1, column) - 4.0 * field.at(row, column);
}

/// How much more of the mask sits in the hollows than over the rises.
double hollowToRiseRatio(const Heightfield& mask, const Heightfield& ground) {
    double hollowTotal = 1.0e-9;
    double riseTotal = 1.0e-9;
    for (int row = 1; row + 1 < mask.width(); ++row) {
        for (int column = 1; column + 1 < mask.width(); ++column) {
            if (hollownessAt(ground, row, column) > 0.0) {
                hollowTotal += mask.at(row, column);
                continue;
            }
            riseTotal += mask.at(row, column);
        }
    }
    return hollowTotal / riseTotal;
}

std::uint64_t hashColors(const ColorField& colors) {
    terrain::Hasher hasher;
    for (const float sample : colors.samples()) {
        hasher.combine(static_cast<double>(sample));
    }
    return hasher.digest();
}

/// A flat grey colour map over the massif, so anything that is not grey in the
/// result came from the weathering and from nothing else.
Graph weatheringGraph() {
    Graph graph = mountainGround();
    terrain::GraphNode& colors = graph.addNode("colors", "ColorRamp");
    colors.parameters["ramp"] = ParameterValue{0.0, "0:#808080;1:#808080"};
    graph.connect("ground", 0, "colors", 0);

    terrain::GraphNode& worn = graph.addNode("worn", "Weathering");
    worn.parameters["scale"] = ParameterValue{600.0, ""};
    worn.parameters["creep"] = ParameterValue{0.0, ""};
    worn.parameters["amount"] = ParameterValue{1.0, ""};
    worn.parameters["dirt"] = ParameterValue{0.0, ""};
    graph.connect("colors", 0, "worn", 0);
    graph.connect("ground", 0, "worn", 1);
    return graph;
}

}  // namespace

/* ------------------------------------------------------------ TextureBase -- */

TEST_CASE("the texture base stays inside the mask range", "[derive][texturebase]") {
    Graph graph = mountainGround();
    addDerived(graph, "TextureBase");

    const auto mask = evaluateNode(graph, "derived");
    REQUIRE(mask != nullptr);
    REQUIRE(allSamplesWithin(*mask, 0.0f, 1.0f));
}

TEST_CASE("the texture base follows the weights it is given", "[derive][texturebase]") {
    Graph graph = mountainGround();
    terrain::GraphNode& base = addDerived(graph, "TextureBase");
    for (const char* weight : {"slope", "flows", "soil", "patches", "chaos", "peaks"}) {
        base.parameters[weight] = ParameterValue{0.0, ""};
    }

    // With nothing weighted there is nothing to say, and the node says it flatly.
    REQUIRE(isConstant(*evaluateNode(graph, "derived")));

    base.parameters["slope"] = ParameterValue{1.0, ""};
    const auto steepOnly = evaluateNode(graph, "derived");
    REQUIRE(steepOnly->at(summitRow, summitColumn - 12) > steepOnly->at(0, 0));

    base.parameters["slope"] = ParameterValue{0.0, ""};
    base.parameters["patches"] = ParameterValue{1.0, ""};
    REQUIRE(hashField(*evaluateNode(graph, "derived")) != hashField(*steepOnly));
}

TEST_CASE("reversing the texture base swaps its ends", "[derive][texturebase]") {
    Graph graph = mountainGround();
    terrain::GraphNode& base = addDerived(graph, "TextureBase");
    const auto forward = evaluateNode(graph, "derived");

    base.parameters["reverse"] = ParameterValue{1.0, ""};
    const auto reversed = evaluateNode(graph, "derived");
    for (int row = 0; row < resolution; row += 7) {
        REQUIRE(reversed->at(row, row) == Approx(1.0f - forward->at(row, row)).margin(1.0e-5));
    }
}

TEST_CASE("the texture base is the same at one thread and at eight", "[derive][texturebase]") {
    Graph graph = mountainGround();
    addDerived(graph, "TextureBase");

    REQUIRE(hashField(*evaluateNode(graph, "derived", 1))
            == hashField(*evaluateNode(graph, "derived", 8)));
}

/* ---------------------------------------------------------------- RockMap -- */

TEST_CASE("rock coverage grows with the coverage dial", "[derive][rockmap]") {
    Graph graph = mountainGround();
    terrain::GraphNode& rock = addDerived(graph, "RockMap");

    rock.parameters["coverage"] = ParameterValue{0.2, ""};
    const auto sparse = evaluateNode(graph, "derived");
    rock.parameters["coverage"] = ParameterValue{0.8, ""};
    const auto dense = evaluateNode(graph, "derived");

    REQUIRE(allSamplesWithin(*sparse, 0.0f, 1.0f));
    REQUIRE(allSamplesWithin(*dense, 0.0f, 1.0f));
    REQUIRE(meanOf(*dense) > meanOf(*sparse));
}

TEST_CASE("the rock map keeps to the steep ground", "[derive][rockmap]") {
    Graph graph = mountainGround();
    terrain::GraphNode& rock = addDerived(graph, "RockMap");
    rock.parameters["slopeInfluence"] = ParameterValue{1.0, ""};

    const auto mask = evaluateNode(graph, "derived");
    const auto ground = evaluateNode(graph, "ground");
    // The plain outside the massif is not steep and bulges nowhere.
    REQUIRE(meanWhereHeightBetween(*mask, *ground, 400.0f, 1200.0f)
            > meanWhereHeightBetween(*mask, *ground, -1.0f, 1.0f));
}

TEST_CASE("the rock map draws different formations from a different seed", "[derive][rockmap]") {
    Graph graph = mountainGround();
    terrain::GraphNode& rock = addDerived(graph, "RockMap");
    const auto first = evaluateNode(graph, "derived");

    rock.parameters["seed"] = ParameterValue{7.0, ""};
    REQUIRE(hashField(*evaluateNode(graph, "derived")) != hashField(*first));
}

TEST_CASE("the rock map is the same at one thread and at eight", "[derive][rockmap]") {
    Graph graph = mountainGround();
    addDerived(graph, "RockMap");

    REQUIRE(hashField(*evaluateNode(graph, "derived", 1))
            == hashField(*evaluateNode(graph, "derived", 8)));
}

/* ------------------------------------------------------------------- Soil -- */

TEST_CASE("soil gathers in the low ground", "[derive][soil]") {
    Graph graph = mountainGround();
    addDerived(graph, "Soil");

    const auto mask = evaluateNode(graph, "derived");
    const auto ground = evaluateNode(graph, "ground");
    REQUIRE(allSamplesWithin(*mask, 0.0f, 1.0f));
    REQUIRE(meanWhereHeightBetween(*mask, *ground, -1.0f, 100.0f)
            > meanWhereHeightBetween(*mask, *ground, 600.0f, 1200.0f));
}

TEST_CASE("a negative soil bias moves the mask into the hollows", "[derive][soil]") {
    Graph graph = mountainGround();
    terrain::GraphNode& soil = addDerived(graph, "Soil");
    const auto ground = evaluateNode(graph, "ground");

    soil.parameters["bias"] = ParameterValue{-1.0, ""};
    const double towardsHollows = hollowToRiseRatio(*evaluateNode(graph, "derived"), *ground);
    soil.parameters["bias"] = ParameterValue{1.0, ""};
    const double towardsOpenGround = hollowToRiseRatio(*evaluateNode(graph, "derived"), *ground);

    REQUIRE(towardsHollows > towardsOpenGround);
}

TEST_CASE("soil is the same at one thread and at eight", "[derive][soil]") {
    Graph graph = mountainGround();
    addDerived(graph, "Soil");

    REQUIRE(hashField(*evaluateNode(graph, "derived", 1))
            == hashField(*evaluateNode(graph, "derived", 8)));
}

/* -------------------------------------------------------------- Occlusion -- */

TEST_CASE("occlusion is deep in a pit and absent on the open plain", "[derive][occlusion]") {
    Graph graph = pitGround();
    terrain::GraphNode& occlusion = addDerived(graph, "Occlusion");
    occlusion.parameters["reach"] = ParameterValue{300.0, ""};

    const auto mask = evaluateNode(graph, "derived");
    REQUIRE(allSamplesWithin(*mask, 0.0f, 1.0f));
    REQUIRE(mask->at(summitRow, summitColumn) > 0.3f);
    REQUIRE(mask->at(1, 1) < 0.05f);
}

TEST_CASE("inverting occlusion selects the exposed ground", "[derive][occlusion]") {
    Graph graph = pitGround();
    terrain::GraphNode& occlusion = addDerived(graph, "Occlusion");
    occlusion.parameters["reach"] = ParameterValue{300.0, ""};
    const auto enclosed = evaluateNode(graph, "derived");

    occlusion.parameters["invert"] = ParameterValue{1.0, ""};
    const auto exposed = evaluateNode(graph, "derived");
    for (int row = 0; row < resolution; row += 7) {
        REQUIRE(exposed->at(row, row) == Approx(1.0f - enclosed->at(row, row)).margin(1.0e-5));
    }
}

TEST_CASE("occlusion is the same at one thread and at eight", "[derive][occlusion]") {
    Graph graph = pitGround();
    addDerived(graph, "Occlusion");

    REQUIRE(hashField(*evaluateNode(graph, "derived", 1))
            == hashField(*evaluateNode(graph, "derived", 8)));
}

/* ------------------------------------------------------------------ Peaks -- */

TEST_CASE("the peak mask is strongest at the summit", "[derive][peaks]") {
    Graph graph = mountainGround();
    addDerived(graph, "Peaks");

    const auto mask = evaluateNode(graph, "derived");
    const auto ground = evaluateNode(graph, "ground");
    REQUIRE(allSamplesWithin(*mask, 0.0f, 1.0f));
    REQUIRE(meanWhereHeightBetween(*mask, *ground, 900.0f, 1200.0f) > 0.3);
    // Nothing on the plain stands above the ground around it.
    REQUIRE(mask->at(1, 1) < 0.05f);
}

TEST_CASE("a higher peak falloff narrows the mask", "[derive][peaks]") {
    Graph graph = mountainGround();
    terrain::GraphNode& peaks = addDerived(graph, "Peaks");

    peaks.parameters["falloff"] = ParameterValue{0.1, ""};
    const double spread = meanOf(*evaluateNode(graph, "derived"));
    peaks.parameters["falloff"] = ParameterValue{0.9, ""};
    const double concentrated = meanOf(*evaluateNode(graph, "derived"));

    REQUIRE(concentrated < spread);
}

TEST_CASE("a precise peak mask reads over a shorter distance", "[derive][peaks]") {
    Graph graph = mountainGround();
    terrain::GraphNode& peaks = addDerived(graph, "Peaks");
    const auto broad = evaluateNode(graph, "derived");

    peaks.parameters["precise"] = ParameterValue{1.0, ""};
    REQUIRE(hashField(*evaluateNode(graph, "derived")) != hashField(*broad));
}

TEST_CASE("peaks is the same at one thread and at eight", "[derive][peaks]") {
    Graph graph = mountainGround();
    addDerived(graph, "Peaks");

    REQUIRE(hashField(*evaluateNode(graph, "derived", 1))
            == hashField(*evaluateNode(graph, "derived", 8)));
}

/* ------------------------------------------------------------- Weathering -- */

TEST_CASE("weathering at no amount returns the colour untouched", "[weathering]") {
    Graph graph = weatheringGraph();
    graph.find("worn")->parameters["amount"] = ParameterValue{0.0, ""};

    const auto worn = evaluateColor(graph, "worn");
    const auto original = evaluateColor(graph, "colors");
    REQUIRE(hashColors(*worn) == hashColors(*original));
}

TEST_CASE("weathering brightens what protrudes and inverse turns it round",
          "[weathering]") {
    Graph graph = weatheringGraph();
    const auto original = evaluateColor(graph, "colors");
    const float plain = original->at(summitRow, summitColumn)[0];

    const auto exposed = evaluateColor(graph, "worn");
    REQUIRE(exposed->at(summitRow, summitColumn)[0] > plain);

    graph.find("worn")->parameters["inverse"] = ParameterValue{1.0, ""};
    const auto sheltered = evaluateColor(graph, "worn");
    REQUIRE(sheltered->at(summitRow, summitColumn)[0] < plain);
}

TEST_CASE("weathering leaves the ground its mask holds back", "[weathering]") {
    Graph graph = weatheringGraph();
    terrain::GraphNode& nothing = graph.addNode("nothing", "Constant");
    nothing.parameters["value"] = ParameterValue{0.0, ""};
    graph.connect("nothing", 0, "worn", 2);

    REQUIRE(hashColors(*evaluateColor(graph, "worn"))
            == hashColors(*evaluateColor(graph, "colors")));
}

TEST_CASE("dirt darkens the recesses further", "[weathering]") {
    Graph graph = weatheringGraph();
    graph.find("worn")->parameters["dirt"] = ParameterValue{0.0, ""};
    const auto clean = evaluateColor(graph, "worn");

    graph.find("worn")->parameters["dirt"] = ParameterValue{1.0, ""};
    graph.find("worn")->parameters["darker"] = ParameterValue{1.0, ""};
    const auto grimy = evaluateColor(graph, "worn");

    double cleanTotal = 0.0;
    double grimyTotal = 0.0;
    for (std::size_t index = 0; index < clean->samples().size(); ++index) {
        cleanTotal += clean->samples()[index];
        grimyTotal += grimy->samples()[index];
    }
    REQUIRE(grimyTotal < cleanTotal);
}

TEST_CASE("weathering is the same at one thread and at eight", "[weathering]") {
    const Graph graph = weatheringGraph();

    REQUIRE(hashColors(*evaluateColor(graph, "worn", 0, 1))
            == hashColors(*evaluateColor(graph, "worn", 0, 8)));
}
