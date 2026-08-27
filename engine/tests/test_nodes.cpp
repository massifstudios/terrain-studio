#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <set>

#include "node_test_support.h"

using Catch::Approx;
using terrain::Graph;
using terrain::Heightfield;
using terrain::ParameterValue;
using terrain::testing::allSamplesWithin;
using terrain::testing::evaluateNode;
using terrain::testing::hashField;
using terrain::testing::isConstant;
using terrain::testing::squareGraph;

namespace {

constexpr int resolution = 64;
constexpr double worldSize = 640.0;

/// A ramp rising due north across the whole world. Rising by exactly the world
/// size over the world size makes every cell a 45 degree slope, which is what
/// the slope and aspect tests measure against.
Graph northwardRamp(double startValue, double endValue) {
    Graph graph = squareGraph(resolution, worldSize);
    terrain::GraphNode& ramp = graph.addNode("ramp", "Gradient");
    ramp.parameters["direction"] = ParameterValue{0.0, ""};
    ramp.parameters["startValue"] = ParameterValue{startValue, ""};
    ramp.parameters["endValue"] = ParameterValue{endValue, ""};
    return graph;
}

void addConstant(Graph& graph, const std::string& id, double value) {
    terrain::GraphNode& node = graph.addNode(id, "Constant");
    node.parameters["value"] = ParameterValue{value, ""};
}

/// Two constants feeding one combiner, which is all a cell-by-cell operation
/// needs in order to be checked exactly.
float combineConstants(const std::string& typeName, double first, double second) {
    Graph graph = squareGraph(16, 160.0);
    addConstant(graph, "a", first);
    addConstant(graph, "b", second);
    graph.addNode("op", typeName);
    graph.connect("a", 0, "op", 0);
    graph.connect("b", 0, "op", 1);

    return evaluateNode(graph, "op")->at(8, 8);
}

}  // namespace

/* ------------------------------------------------------------- Gradient -- */

TEST_CASE("a gradient ramps evenly from one edge to the other", "[nodes][gradient]") {
    const auto field = evaluateNode(northwardRamp(0.0, 100.0), "ramp");
    REQUIRE(field != nullptr);

    // Row 0 is north, and its cell centre sits half a cell inside the edge.
    REQUIRE(field->at(0, 0) == Approx(100.0 * 635.0 / 640.0));
    REQUIRE(field->at(resolution - 1, 0) == Approx(100.0 * 5.0 / 640.0));
    // Nothing varies across a north-facing ramp.
    REQUIRE(field->at(0, 0) == Approx(field->at(0, resolution - 1)));
}

TEST_CASE("a gradient turns with its direction", "[nodes][gradient]") {
    Graph graph = squareGraph(resolution, worldSize);
    terrain::GraphNode& ramp = graph.addNode("ramp", "Gradient");
    ramp.parameters["direction"] = ParameterValue{90.0, ""};
    ramp.parameters["startValue"] = ParameterValue{0.0, ""};
    ramp.parameters["endValue"] = ParameterValue{100.0, ""};

    const auto field = evaluateNode(graph, "ramp");
    REQUIRE(field->at(0, resolution - 1) > field->at(0, 0));
    REQUIRE(field->at(0, 0) == Approx(field->at(resolution - 1, 0)));
}

/* ------------------------------------------------------------- Combine --- */

TEST_CASE("the combiners do exactly what they say", "[nodes][combine]") {
    REQUIRE(combineConstants("Add", 40.0, 2.0) == Approx(42.0));
    REQUIRE(combineConstants("Subtract", 40.0, 2.0) == Approx(38.0));
    REQUIRE(combineConstants("Multiply", 40.0, 2.0) == Approx(80.0));
    REQUIRE(combineConstants("Min", 40.0, 2.0) == Approx(2.0));
    REQUIRE(combineConstants("Max", 40.0, 2.0) == Approx(40.0));
}

TEST_CASE("lerp blends by its amount when no mask is connected", "[nodes][combine]") {
    Graph graph = squareGraph(16, 160.0);
    addConstant(graph, "a", 0.0);
    addConstant(graph, "b", 100.0);
    terrain::GraphNode& blend = graph.addNode("blend", "Lerp");
    blend.parameters["amount"] = ParameterValue{0.25, ""};
    graph.connect("a", 0, "blend", 0);
    graph.connect("b", 0, "blend", 1);

    REQUIRE(evaluateNode(graph, "blend")->at(8, 8) == Approx(25.0));
}

/* --------------------------------------------------------------- Clamp --- */

TEST_CASE("clamp holds a field inside its limits", "[nodes][filter]") {
    Graph graph = northwardRamp(0.0, 100.0);
    terrain::GraphNode& limit = graph.addNode("limit", "Clamp");
    limit.parameters["minimumValue"] = ParameterValue{20.0, ""};
    limit.parameters["maximumValue"] = ParameterValue{80.0, ""};
    graph.connect("ramp", 0, "limit", 0);

    const auto field = evaluateNode(graph, "limit");
    REQUIRE(field->minimum() == Approx(20.0));
    REQUIRE(field->maximum() == Approx(80.0));
}

/* ------------------------------------------------------------- Terrace --- */

TEST_CASE("a fully sharp terrace leaves only step heights behind", "[nodes][filter]") {
    Graph graph = northwardRamp(0.0, 100.0);
    terrain::GraphNode& steps = graph.addNode("steps", "Terrace");
    steps.parameters["stepCount"] = ParameterValue{4.0, ""};
    steps.parameters["sharpness"] = ParameterValue{1.0, ""};
    graph.connect("ramp", 0, "steps", 0);

    const auto field = evaluateNode(graph, "steps");
    std::set<float> distinct(field->samples().begin(), field->samples().end());

    // Four treads plus the single cell that lands exactly on the top edge.
    REQUIRE(distinct.size() <= 5);
    REQUIRE(distinct.size() >= 4);
}

TEST_CASE("a terrace at zero sharpness changes nothing", "[nodes][filter]") {
    Graph graph = northwardRamp(0.0, 100.0);
    terrain::GraphNode& steps = graph.addNode("steps", "Terrace");
    steps.parameters["stepCount"] = ParameterValue{8.0, ""};
    steps.parameters["sharpness"] = ParameterValue{0.0, ""};
    graph.connect("ramp", 0, "steps", 0);

    const auto flat = evaluateNode(graph, "ramp");
    const auto stepped = evaluateNode(graph, "steps");
    for (int row = 0; row < resolution; ++row) {
        REQUIRE(stepped->at(row, 0) == Approx(flat->at(row, 0)).margin(1.0e-3));
    }
}

/* --------------------------------------------------------------- Curve --- */

TEST_CASE("the identity curve returns the field it was given", "[nodes][filter]") {
    Graph graph = northwardRamp(0.0, 100.0);
    graph.addNode("shape", "Curve");
    graph.connect("ramp", 0, "shape", 0);

    const auto original = evaluateNode(graph, "ramp");
    const auto shaped = evaluateNode(graph, "shape");
    for (int row = 0; row < resolution; ++row) {
        REQUIRE(shaped->at(row, 0) == Approx(original->at(row, 0)).margin(1.0e-2));
    }
}

TEST_CASE("an inverted curve turns a field upside down", "[nodes][filter]") {
    Graph graph = northwardRamp(0.0, 100.0);
    terrain::GraphNode& shape = graph.addNode("shape", "Curve");
    shape.parameters["curve"] = ParameterValue{0.0, "0,1;1,0"};
    graph.connect("ramp", 0, "shape", 0);

    const auto original = evaluateNode(graph, "ramp");
    const auto shaped = evaluateNode(graph, "shape");
    REQUIRE(shaped->at(0, 0) == Approx(original->minimum()).margin(1.0e-2));
    REQUIRE(shaped->at(resolution - 1, 0) == Approx(original->maximum()).margin(1.0e-2));
}

/* ---------------------------------------------------------------- Warp --- */

TEST_CASE("a warp of nothing moves nothing", "[nodes][filter]") {
    Graph graph = northwardRamp(0.0, 100.0);
    terrain::GraphNode& noise = graph.addNode("noise", "Perlin");
    noise.parameters["featureSize"] = ParameterValue{200.0, ""};
    terrain::GraphNode& warp = graph.addNode("warp", "Warp");
    warp.parameters["amount"] = ParameterValue{0.0, ""};
    graph.connect("ramp", 0, "warp", 0);
    graph.connect("noise", 0, "warp", 1);

    REQUIRE(hashField(*evaluateNode(graph, "warp")) == hashField(*evaluateNode(graph, "ramp")));
}

TEST_CASE("a warp with an amount actually displaces", "[nodes][filter]") {
    Graph graph = northwardRamp(0.0, 100.0);
    terrain::GraphNode& noise = graph.addNode("noise", "Perlin");
    noise.parameters["featureSize"] = ParameterValue{200.0, ""};
    terrain::GraphNode& warp = graph.addNode("warp", "Warp");
    warp.parameters["amount"] = ParameterValue{100.0, ""};
    graph.connect("ramp", 0, "warp", 0);
    graph.connect("noise", 0, "warp", 1);

    REQUIRE(hashField(*evaluateNode(graph, "warp")) != hashField(*evaluateNode(graph, "ramp")));
}

TEST_CASE("a warp lays no seam across the field", "[nodes][filter]") {
    // A warp needs two displacement components and is given one field, so it
    // reads that field a second time from somewhere else. Read it at a cyclic
    // shift and the wrap jumps from one edge of the field to the other, which
    // draws a discontinuity clean across the grid at the shift distance.
    Graph graph = northwardRamp(0.0, 100.0);
    terrain::GraphNode& noise = graph.addNode("noise", "Perlin");
    noise.parameters["featureSize"] = ParameterValue{2000.0, ""};
    noise.parameters["octaves"] = ParameterValue{2.0, ""};
    terrain::GraphNode& warp = graph.addNode("warp", "Warp");
    warp.parameters["amount"] = ParameterValue{400.0, ""};
    graph.connect("ramp", 0, "warp", 0);
    graph.connect("noise", 0, "warp", 1);

    const auto field = evaluateNode(graph, "warp");
    REQUIRE(field != nullptr);

    std::vector<double> rowBend(resolution, 0.0);
    std::vector<double> columnBend(resolution, 0.0);
    double totalBend = 0.0;
    for (int row = 1; row < resolution - 1; ++row) {
        for (int column = 1; column < resolution - 1; ++column) {
            const double bend = std::abs(field->at(column + 1, row) + field->at(column - 1, row)
                                         + field->at(column, row + 1) + field->at(column, row - 1)
                                         - 4.0 * field->at(column, row));
            rowBend[row] += bend;
            columnBend[column] += bend;
            totalBend += bend;
        }
    }

    // No single line may bend far harder than the field does on average. The
    // seam this guards against stood over twenty times the mean.
    const double meanLine = totalBend / (resolution - 2);
    for (int line = 1; line < resolution - 1; ++line) {
        REQUIRE(rowBend[line] < meanLine * 8.0);
        REQUIRE(columnBend[line] < meanLine * 8.0);
    }
}

/* ---------------------------------------------------------------- Masks -- */

TEST_CASE("a slope mask measures the angle of the ground", "[nodes][mask]") {
    // The ramp rises one meter per meter, which is 45 degrees everywhere.
    Graph graph = northwardRamp(0.0, worldSize);
    terrain::GraphNode& mask = graph.addNode("steep", "SlopeMask");
    mask.parameters["minimumSlope"] = ParameterValue{44.0, ""};
    mask.parameters["maximumSlope"] = ParameterValue{46.0, ""};
    mask.parameters["falloff"] = ParameterValue{0.0, ""};
    graph.connect("ramp", 0, "steep", 0);

    const auto field = evaluateNode(graph, "steep");
    REQUIRE(field->minimum() == Approx(1.0));
    REQUIRE(field->maximum() == Approx(1.0));
}

TEST_CASE("a slope mask rejects ground outside its band", "[nodes][mask]") {
    Graph graph = northwardRamp(0.0, worldSize);
    terrain::GraphNode& mask = graph.addNode("steep", "SlopeMask");
    mask.parameters["minimumSlope"] = ParameterValue{60.0, ""};
    mask.parameters["maximumSlope"] = ParameterValue{90.0, ""};
    mask.parameters["falloff"] = ParameterValue{0.0, ""};
    graph.connect("ramp", 0, "steep", 0);

    REQUIRE(evaluateNode(graph, "steep")->maximum() == Approx(0.0));
}

TEST_CASE("a height mask selects its altitude band", "[nodes][mask]") {
    Graph inside = squareGraph(16, 160.0);
    addConstant(inside, "flat", 2000.0);
    inside.addNode("band", "HeightMask");
    inside.connect("flat", 0, "band", 0);
    REQUIRE(evaluateNode(inside, "band")->minimum() == Approx(1.0));

    Graph below = squareGraph(16, 160.0);
    addConstant(below, "flat", 1000.0);
    below.addNode("band", "HeightMask");
    below.connect("flat", 0, "band", 0);
    REQUIRE(evaluateNode(below, "band")->maximum() == Approx(0.0));
}

TEST_CASE("an aspect mask finds the direction a slope faces", "[nodes][mask]") {
    // Rising towards the north means the ground looks south.
    const auto maskFacing = [](double directionDegrees) {
        Graph graph = northwardRamp(0.0, worldSize);
        terrain::GraphNode& facing = graph.addNode("facing", "AspectMask");
        facing.parameters["direction"] = ParameterValue{directionDegrees, ""};
        graph.connect("ramp", 0, "facing", 0);
        return evaluateNode(graph, "facing");
    };

    REQUIRE(maskFacing(180.0)->minimum() == Approx(1.0));
    REQUIRE(maskFacing(0.0)->maximum() == Approx(0.0));
}

TEST_CASE("a curvature mask separates hollows from rises", "[nodes][mask]") {
    Graph graph = northwardRamp(0.0, 200.0);
    terrain::GraphNode& steps = graph.addNode("steps", "Terrace");
    steps.parameters["stepCount"] = ParameterValue{8.0, ""};
    steps.parameters["sharpness"] = ParameterValue{0.9, ""};
    graph.connect("ramp", 0, "steps", 0);

    terrain::GraphNode& hollows = graph.addNode("hollows", "CurvatureMask");
    hollows.parameters["mode"] = ParameterValue{0.0, ""};
    graph.connect("steps", 0, "hollows", 0);

    terrain::GraphNode& rises = graph.addNode("rises", "CurvatureMask");
    rises.parameters["mode"] = ParameterValue{1.0, ""};
    graph.connect("steps", 0, "rises", 0);

    const auto hollowField = evaluateNode(graph, "hollows");
    const auto riseField = evaluateNode(graph, "rises");

    REQUIRE(allSamplesWithin(*hollowField, 0.0f, 1.0f));
    REQUIRE_FALSE(isConstant(*hollowField));
    REQUIRE(hashField(*hollowField) != hashField(*riseField));
}

TEST_CASE("a mask scales a filter's effect", "[nodes][mask]") {
    Graph graph = northwardRamp(0.0, 100.0);
    terrain::GraphNode& band = graph.addNode("band", "HeightMask");
    band.parameters["minimumAltitude"] = ParameterValue{-1000.0, ""};
    band.parameters["maximumAltitude"] = ParameterValue{-999.0, ""};
    band.parameters["falloff"] = ParameterValue{0.0, ""};
    graph.connect("ramp", 0, "band", 0);

    terrain::GraphNode& limit = graph.addNode("limit", "Clamp");
    limit.parameters["minimumValue"] = ParameterValue{40.0, ""};
    limit.parameters["maximumValue"] = ParameterValue{60.0, ""};
    graph.connect("ramp", 0, "limit", 0);
    graph.connect("band", 0, "limit", 1);

    // The mask is zero everywhere, so the clamp must have no effect at all.
    REQUIRE(hashField(*evaluateNode(graph, "limit")) == hashField(*evaluateNode(graph, "ramp")));
}

/* ----------------------------------------------------------- Generators -- */

TEST_CASE("perlin stays inside its amplitude", "[nodes][generate]") {
    Graph graph = squareGraph(resolution, worldSize);
    terrain::GraphNode& noise = graph.addNode("noise", "Perlin");
    noise.parameters["featureSize"] = ParameterValue{200.0, ""};
    noise.parameters["amplitude"] = ParameterValue{300.0, ""};

    const auto field = evaluateNode(graph, "noise");
    REQUIRE(allSamplesWithin(*field, -300.0f, 300.0f));
    REQUIRE_FALSE(isConstant(*field));
}

TEST_CASE("ridged noise never dips below its base", "[nodes][generate]") {
    Graph graph = squareGraph(resolution, worldSize);
    terrain::GraphNode& noise = graph.addNode("noise", "RidgedNoise");
    noise.parameters["featureSize"] = ParameterValue{300.0, ""};
    noise.parameters["amplitude"] = ParameterValue{500.0, ""};

    const auto field = evaluateNode(graph, "noise");
    REQUIRE(allSamplesWithin(*field, 0.0f, 500.0f));
    REQUIRE_FALSE(isConstant(*field));
}

TEST_CASE("voronoi plates are flat inside a cell", "[nodes][generate]") {
    Graph graph = squareGraph(resolution, worldSize);
    terrain::GraphNode& noise = graph.addNode("noise", "Voronoi");
    noise.parameters["featureSize"] = ParameterValue{160.0, ""};
    noise.parameters["mode"] = ParameterValue{2.0, ""};
    noise.parameters["amplitude"] = ParameterValue{100.0, ""};

    const auto field = evaluateNode(graph, "noise");
    REQUIRE(allSamplesWithin(*field, 0.0f, 100.0f));

    // Sixteen cells across the world, so far fewer distinct heights than cells.
    const std::set<float> distinct(field->samples().begin(), field->samples().end());
    REQUIRE(distinct.size() < 64);
    REQUIRE(distinct.size() > 1);
}

TEST_CASE("a node's own seed varies that node alone", "[nodes][generate][determinism]") {
    const auto noiseWithSeed = [](double seedValue) {
        Graph graph = squareGraph(resolution, worldSize);
        terrain::GraphNode& noise = graph.addNode("noise", "Perlin");
        noise.parameters["featureSize"] = ParameterValue{200.0, ""};
        noise.parameters["seed"] = ParameterValue{seedValue, ""};
        return hashField(*evaluateNode(graph, "noise"));
    };

    REQUIRE(noiseWithSeed(0.0) == noiseWithSeed(0.0));
    REQUIRE(noiseWithSeed(0.0) != noiseWithSeed(17.0));
}

TEST_CASE("generators give the same field at one thread and at eight",
          "[nodes][generate][determinism]") {
    Graph graph = squareGraph(128, 1280.0);
    graph.addNode("perlin", "Perlin");
    graph.addNode("ridged", "RidgedNoise");
    graph.addNode("cells", "Voronoi");
    graph.addNode("mix", "Add");
    graph.addNode("total", "Add");
    graph.addNode("rough", "Warp");
    graph.addNode("steep", "SlopeMask");

    graph.connect("perlin", 0, "mix", 0);
    graph.connect("ridged", 0, "mix", 1);
    graph.connect("mix", 0, "total", 0);
    graph.connect("cells", 0, "total", 1);
    graph.connect("total", 0, "rough", 0);
    graph.connect("perlin", 0, "rough", 1);
    graph.connect("rough", 0, "steep", 0);

    REQUIRE(hashField(*evaluateNode(graph, "steep", 1))
            == hashField(*evaluateNode(graph, "steep", 8)));
}

TEST_CASE("a generator describes the same landscape at any resolution",
          "[nodes][generate][determinism]") {
    // Sampling is in meters, so doubling the resolution adds detail rather
    // than producing a different landscape.
    const auto coarse = evaluateNode(
        [] {
            Graph graph = squareGraph(64, 20000.0);
            terrain::GraphNode& noise = graph.addNode("noise", "Perlin");
            noise.parameters["featureSize"] = ParameterValue{5000.0, ""};
            noise.parameters["octaves"] = ParameterValue{3.0, ""};
            noise.parameters["amplitude"] = ParameterValue{1000.0, ""};
            return graph;
        }(),
        "noise");
    const auto fine = evaluateNode(
        [] {
            Graph graph = squareGraph(128, 20000.0);
            terrain::GraphNode& noise = graph.addNode("noise", "Perlin");
            noise.parameters["featureSize"] = ParameterValue{5000.0, ""};
            noise.parameters["octaves"] = ParameterValue{3.0, ""};
            noise.parameters["amplitude"] = ParameterValue{1000.0, ""};
            return graph;
        }(),
        "noise");

    // The north-west corner cell centres sit at different places, so compare
    // the same world position: a quarter of the way in, on both grids.
    REQUIRE(fine->at(32, 32) == Approx(coarse->at(16, 16)).margin(60.0));
    REQUIRE(fine->minimum() == Approx(coarse->minimum()).margin(150.0));
}
