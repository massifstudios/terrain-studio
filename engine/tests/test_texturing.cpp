#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <set>

#include "node_test_support.h"
#include "terrain/color_ramp.h"
#include "terrain/formats/png_image.h"
#include "terrain/formats/point_files.h"
#include "terrain/formats/texture_maps.h"
#include "test_support.h"

using Catch::Approx;
using terrain::ColorRamp;
using terrain::Graph;
using terrain::ParameterValue;
using terrain::testing::evaluateColor;
using terrain::testing::evaluateNode;
using terrain::testing::evaluateOutput;
using terrain::testing::evaluatePoints;
using terrain::testing::ScratchFile;
using terrain::testing::squareGraph;

namespace {

constexpr int resolution = 64;
constexpr double worldSize = 6400.0;

/// A ramp over a north-facing gradient: the field's own range spans the ramp.
Graph rampOverRamp(const std::string& rampText) {
    Graph graph = squareGraph(resolution, worldSize);
    terrain::GraphNode& ground = graph.addNode("ground", "Gradient");
    ground.parameters["direction"] = ParameterValue{0.0, ""};
    ground.parameters["startValue"] = ParameterValue{0.0, ""};
    ground.parameters["endValue"] = ParameterValue{1000.0, ""};

    terrain::GraphNode& colors = graph.addNode("colors", "ColorRamp");
    colors.parameters["ramp"] = ParameterValue{0.0, rampText};
    graph.connect("ground", 0, "colors", 0);
    return graph;
}

terrain::GraphNode& addMask(Graph& graph, const std::string& id, double low, double high) {
    terrain::GraphNode& mask = graph.addNode(id, "HeightMask");
    mask.parameters["minimumAltitude"] = ParameterValue{low, ""};
    mask.parameters["maximumAltitude"] = ParameterValue{high, ""};
    mask.parameters["falloff"] = ParameterValue{0.0, ""};
    return mask;
}

/// A satellite map over a north-facing gradient, so row 0 is the high end.
Graph satMapOverRamp(double library, double index, double reverse, double spread) {
    Graph graph = squareGraph(resolution, worldSize);
    terrain::GraphNode& ground = graph.addNode("ground", "Gradient");
    ground.parameters["direction"] = ParameterValue{0.0, ""};
    ground.parameters["startValue"] = ParameterValue{0.0, ""};
    ground.parameters["endValue"] = ParameterValue{1000.0, ""};

    terrain::GraphNode& colors = graph.addNode("colors", "SatMap");
    colors.parameters["library"] = ParameterValue{library, ""};
    colors.parameters["index"] = ParameterValue{index, ""};
    colors.parameters["reverse"] = ParameterValue{reverse, ""};
    colors.parameters["spread"] = ParameterValue{spread, ""};
    graph.connect("ground", 0, "colors", 0);
    return graph;
}

bool channelsWithinUnit(const terrain::ColorField& colors) {
    for (const float sample : colors.samples()) {
        if (sample < 0.0f || sample > 1.0f) {
            return false;
        }
    }
    return true;
}

}  // namespace

/* ------------------------------------------------------------ ColorRamp -- */

TEST_CASE("a ramp reads the hex it was given", "[texture][ramp]") {
    const ColorRamp ramp = ColorRamp::parse("0:#000000;1:#ff8000");

    REQUIRE(ramp.sample(0.0f).red == Approx(0.0));
    REQUIRE(ramp.sample(1.0f).red == Approx(1.0));
    REQUIRE(ramp.sample(1.0f).green == Approx(128.0 / 255.0));
    REQUIRE(ramp.sample(1.0f).blue == Approx(0.0));
}

TEST_CASE("a ramp interpolates between its stops", "[texture][ramp]") {
    const ColorRamp ramp = ColorRamp::parse("0:#000000;1:#ffffff");
    REQUIRE(ramp.sample(0.5f).red == Approx(0.5));
}

TEST_CASE("a ramp reads flat outside its ends", "[texture][ramp]") {
    const ColorRamp ramp = ColorRamp::parse("0.25:#ff0000;0.75:#0000ff");
    REQUIRE(ramp.sample(0.0f).red == Approx(1.0));
    REQUIRE(ramp.sample(1.0f).blue == Approx(1.0));
}

TEST_CASE("an unusable ramp falls back to black and white", "[texture][ramp]") {
    for (const char* text : {"", "nonsense", "0:#zzz"}) {
        const ColorRamp ramp = ColorRamp::parse(text);
        REQUIRE(ramp.stops().size() == 2);
        REQUIRE(ramp.sample(1.0f).red == Approx(1.0));
    }
}

TEST_CASE("the colour ramp node paints the whole field", "[texture][nodes]") {
    const auto colors = evaluateColor(rampOverRamp("0:#000000;1:#ffffff"), "colors");
    REQUIRE(colors != nullptr);
    REQUIRE(colors->width() == resolution);

    // The gradient rises northwards, so row 0 takes the top of the ramp.
    REQUIRE(colors->at(0, 0)[0] > colors->at(resolution - 1, 0)[0]);
    REQUIRE(colors->at(0, 0)[0] == Approx(1.0).margin(0.02));
    REQUIRE(colors->at(resolution - 1, 0)[0] == Approx(0.0).margin(0.02));
}

/* ------------------------------------------------------------- MixColor -- */

TEST_CASE("mixing colour maps follows the amount", "[texture][nodes]") {
    Graph graph = rampOverRamp("0:#000000;1:#000000");
    terrain::GraphNode& white = graph.addNode("white", "ColorRamp");
    white.parameters["ramp"] = ParameterValue{0.0, "0:#ffffff;1:#ffffff"};
    graph.connect("ground", 0, "white", 0);

    terrain::GraphNode& blend = graph.addNode("blend", "MixColor");
    blend.parameters["amount"] = ParameterValue{0.25, ""};
    graph.connect("colors", 0, "blend", 0);
    graph.connect("white", 0, "blend", 1);

    const auto mixed = evaluateColor(graph, "blend");
    REQUIRE(mixed->at(10, 10)[0] == Approx(0.25));
    REQUIRE(mixed->at(10, 10)[2] == Approx(0.25));
}

/* --------------------------------------------------------- SplatWeights -- */

/* --------------------------------------------------------------- SatMap -- */

TEST_CASE("a satmap reads its gradient from the foot of the field to its crest",
          "[texture][satmap]") {
    const Graph graph = satMapOverRamp(0.0, 0.0, 0.0, 0.0);
    const auto colors = evaluateColor(graph, "colors");

    // The first rock gradient runs #2b2724 to #c9c4bd.
    REQUIRE(colors->at(0, 0)[0] == Approx(0xc9 / 255.0).margin(0.02));
    REQUIRE(colors->at(resolution - 1, 0)[0] == Approx(0x2b / 255.0).margin(0.02));
}

TEST_CASE("reversing a satmap reads the gradient high ground first", "[texture][satmap]") {
    const Graph graph = satMapOverRamp(0.0, 0.0, 1.0, 0.0);
    const auto colors = evaluateColor(graph, "colors");

    REQUIRE(colors->at(0, 0)[0] == Approx(0x2b / 255.0).margin(0.02));
    REQUIRE(colors->at(resolution - 1, 0)[0] == Approx(0xc9 / 255.0).margin(0.02));
}

TEST_CASE("every satmap library and map paints a usable gradient", "[texture][satmap]") {
    for (int library = 0; library < 6; ++library) {
        for (int index = 0; index < 5; ++index) {
            const Graph graph = satMapOverRamp(library, index, 0.0, 0.0);
            const auto colors = evaluateColor(graph, "colors");
            REQUIRE(colors != nullptr);
            REQUIRE(channelsWithinUnit(*colors));
            REQUIRE(colors->at(0, 0)[1] != colors->at(resolution - 1, 0)[1]);
        }
    }
}

TEST_CASE("a satmap's map index counts round past the last gradient", "[texture][satmap]") {
    const auto first = evaluateColor(satMapOverRamp(1.0, 0.0, 0.0, 0.0), "colors");
    const auto wrapped = evaluateColor(satMapOverRamp(1.0, 4.0, 0.0, 0.0), "colors");
    REQUIRE(first->at(resolution / 3, 5)[0] == Approx(wrapped->at(resolution / 3, 5)[0]));
}

TEST_CASE("an equalized satmap gives every colour the same share of ground",
          "[texture][satmap]") {
    // A mountain crowds most of its cells near the foot, so over its own range
    // the high colours barely appear; by rank they take their even share.
    for (const double spread : {0.0, 1.0}) {
        Graph graph = squareGraph(resolution, worldSize);
        graph.addNode("ground", "Mountain");
        terrain::GraphNode& colors = graph.addNode("colors", "SatMap");
        colors.parameters["spread"] = ParameterValue{spread, ""};
        graph.connect("ground", 0, "colors", 0);

        const auto field = evaluateColor(graph, "colors");
        int brightCells = 0;
        for (int row = 0; row < resolution; ++row) {
            for (int column = 0; column < resolution; ++column) {
                if (field->at(row, column)[0] > 0x8f / 255.0f) {
                    ++brightCells;
                }
            }
        }
        const float brightShare = static_cast<float>(brightCells) / (resolution * resolution);
        if (spread == 0.0) {
            REQUIRE(brightShare < 0.15f);
            continue;
        }
        REQUIRE(brightShare > 0.25f);
    }
}

TEST_CASE("splat weights add to one in every cell", "[texture][nodes]") {
    Graph graph = squareGraph(resolution, worldSize);
    terrain::GraphNode& ground = graph.addNode("ground", "Gradient");
    ground.parameters["startValue"] = ParameterValue{0.0, ""};
    ground.parameters["endValue"] = ParameterValue{1000.0, ""};

    addMask(graph, "low", 0.0, 400.0);
    addMask(graph, "high", 300.0, 1000.0);
    graph.connect("ground", 0, "low", 0);
    graph.connect("ground", 0, "high", 0);

    graph.addNode("splat", "SplatWeights");
    graph.connect("low", 0, "splat", 0);
    graph.connect("high", 0, "splat", 1);

    const auto base = evaluateOutput(graph, "splat", 0);
    const auto second = evaluateOutput(graph, "splat", 1);
    const auto third = evaluateOutput(graph, "splat", 2);
    const auto fourth = evaluateOutput(graph, "splat", 3);

    for (std::size_t index = 0; index < base->sampleCount(); ++index) {
        const float total = base->data()[index] + second->data()[index] + third->data()[index]
                            + fourth->data()[index];
        REQUIRE(total == Approx(1.0));
    }
    // In the overlap both layers claim a share; outside it one takes all.
    REQUIRE(second->at(0, 0) == Approx(1.0));
    REQUIRE(base->at(resolution - 1, 0) == Approx(1.0));
}

TEST_CASE("ground nothing claims falls to the base layer", "[texture][nodes]") {
    Graph graph = squareGraph(resolution, worldSize);
    terrain::GraphNode& ground = graph.addNode("ground", "Gradient");
    ground.parameters["startValue"] = ParameterValue{0.0, ""};
    ground.parameters["endValue"] = ParameterValue{1000.0, ""};
    addMask(graph, "none", -9000.0, -8999.0);
    graph.connect("ground", 0, "none", 0);

    graph.addNode("splat", "SplatWeights");
    graph.connect("none", 0, "splat", 1);

    REQUIRE(evaluateOutput(graph, "splat", 0)->minimum() == Approx(1.0));
}

/* ----------------------------------------------------------- GrowthMask -- */

TEST_CASE("growth stops above the treeline and on cliffs", "[vegetation][nodes]") {
    Graph graph = squareGraph(resolution, worldSize);
    terrain::GraphNode& ground = graph.addNode("ground", "Gradient");
    ground.parameters["direction"] = ParameterValue{0.0, ""};
    ground.parameters["startValue"] = ParameterValue{0.0, ""};
    ground.parameters["endValue"] = ParameterValue{3000.0, ""};

    terrain::GraphNode& growth = graph.addNode("growth", "GrowthMask");
    growth.parameters["minimumAltitude"] = ParameterValue{600.0, ""};
    growth.parameters["maximumAltitude"] = ParameterValue{2100.0, ""};
    growth.parameters["altitudeFalloff"] = ParameterValue{0.0, ""};
    growth.parameters["maximumSlope"] = ParameterValue{35.0, ""};
    growth.parameters["slopeFalloff"] = ParameterValue{0.0, ""};
    graph.connect("ground", 0, "growth", 0);

    const auto mask = evaluateNode(graph, "growth");
    const auto altitude = evaluateNode(graph, "ground");

    for (int row = 0; row < resolution; ++row) {
        const float here = altitude->at(row, 0);
        if (here < 600.0f || here > 2100.0f) {
            REQUIRE(mask->at(row, 0) == Approx(0.0));
        }
    }
    REQUIRE(mask->maximum() == Approx(1.0));
}

TEST_CASE("moisture only ever holds growth back", "[vegetation][nodes]") {
    Graph graph = squareGraph(resolution, worldSize);
    terrain::GraphNode& ground = graph.addNode("ground", "Gradient");
    ground.parameters["startValue"] = ParameterValue{800.0, ""};
    ground.parameters["endValue"] = ParameterValue{1600.0, ""};

    addMask(graph, "dry", -9000.0, -8999.0);
    graph.connect("ground", 0, "dry", 0);

    graph.addNode("plain", "GrowthMask");
    graph.connect("ground", 0, "plain", 0);

    terrain::GraphNode& damp = graph.addNode("damp", "GrowthMask");
    damp.parameters["moisture"] = ParameterValue{1.0, ""};
    graph.connect("ground", 0, "damp", 0);
    graph.connect("dry", 0, "damp", 1);

    // With full influence and no moisture anywhere, nothing grows.
    REQUIRE(evaluateNode(graph, "plain")->maximum() > 0.5f);
    REQUIRE(evaluateNode(graph, "damp")->maximum() == Approx(0.0));
}

/* -------------------------------------------------------------- Scatter -- */

namespace {

Graph scatterOverGround(double spacingMeters, double seedValue) {
    Graph graph = squareGraph(resolution, worldSize);
    terrain::GraphNode& ground = graph.addNode("ground", "Constant");
    ground.parameters["value"] = ParameterValue{1200.0, ""};

    terrain::GraphNode& everywhere = graph.addNode("everywhere", "HeightMask");
    everywhere.parameters["minimumAltitude"] = ParameterValue{0.0, ""};
    everywhere.parameters["maximumAltitude"] = ParameterValue{5000.0, ""};
    graph.connect("ground", 0, "everywhere", 0);

    terrain::GraphNode& trees = graph.addNode("trees", "Scatter");
    trees.parameters["spacing"] = ParameterValue{spacingMeters, ""};
    trees.parameters["seed"] = ParameterValue{seedValue, ""};
    graph.connect("everywhere", 0, "trees", 0);
    graph.connect("ground", 0, "trees", 1);
    return graph;
}

}  // namespace

TEST_CASE("no two scattered instances stand closer than the spacing",
          "[vegetation][scatter]") {
    const double spacing = 200.0;
    const auto points = evaluatePoints(scatterOverGround(spacing, 0.0), "trees");
    REQUIRE(points != nullptr);
    REQUIRE(points->size() > 100);

    for (std::size_t first = 0; first < points->size(); ++first) {
        for (std::size_t second = first + 1; second < points->size(); ++second) {
            const double east =
                points->points()[first].eastingMeters - points->points()[second].eastingMeters;
            const double north =
                points->points()[first].northingMeters - points->points()[second].northingMeters;
            REQUIRE(east * east + north * north >= spacing * spacing * 0.999);
        }
    }
}

TEST_CASE("scattered instances stay inside the world and on the ground",
          "[vegetation][scatter]") {
    const auto points = evaluatePoints(scatterOverGround(300.0, 0.0), "trees");

    for (const TerrainPoint& point : points->points()) {
        REQUIRE(point.eastingMeters >= 0.0f);
        REQUIRE(point.eastingMeters <= worldSize);
        REQUIRE(point.northingMeters >= 0.0f);
        REQUIRE(point.northingMeters <= worldSize);
        REQUIRE(point.altitudeMeters == Approx(1200.0));
        REQUIRE(point.rotationDegrees >= 0.0f);
        REQUIRE(point.rotationDegrees <= 360.0f);
        REQUIRE(point.scale >= 0.8f);
        REQUIRE(point.scale <= 1.25f);
    }
}

TEST_CASE("every instance has an id of its own", "[vegetation][scatter]") {
    const auto points = evaluatePoints(scatterOverGround(300.0, 0.0), "trees");
    std::set<std::uint64_t> ids;
    for (const TerrainPoint& point : points->points()) {
        ids.insert(point.id);
    }
    REQUIRE(ids.size() == points->size());
}

TEST_CASE("a scatter is the same however many threads run the graph",
          "[vegetation][scatter][determinism]") {
    const Graph graph = scatterOverGround(300.0, 0.0);
    const auto single = evaluatePoints(graph, "trees", 1);
    const auto many = evaluatePoints(graph, "trees", 8);

    REQUIRE(single->size() == many->size());
    for (std::size_t index = 0; index < single->size(); ++index) {
        REQUIRE(single->points()[index].id == many->points()[index].id);
        REQUIRE(single->points()[index].eastingMeters
                == many->points()[index].eastingMeters);
    }
}

TEST_CASE("a different seed scatters differently", "[vegetation][scatter][determinism]") {
    const auto first = evaluatePoints(scatterOverGround(300.0, 0.0), "trees");
    const auto second = evaluatePoints(scatterOverGround(300.0, 11.0), "trees");

    REQUIRE(first->points()[0].eastingMeters != second->points()[0].eastingMeters);
}

TEST_CASE("density decides where instances go", "[vegetation][scatter]") {
    Graph graph = squareGraph(resolution, worldSize);
    terrain::GraphNode& ground = graph.addNode("ground", "Gradient");
    ground.parameters["direction"] = ParameterValue{0.0, ""};
    ground.parameters["startValue"] = ParameterValue{0.0, ""};
    ground.parameters["endValue"] = ParameterValue{1000.0, ""};

    // Only the northern half of the world is habitable.
    addMask(graph, "north", 500.0, 1000.0);
    graph.connect("ground", 0, "north", 0);

    terrain::GraphNode& trees = graph.addNode("trees", "Scatter");
    trees.parameters["spacing"] = ParameterValue{150.0, ""};
    graph.connect("north", 0, "trees", 0);

    const auto points = evaluatePoints(graph, "trees");
    REQUIRE(points->size() > 20);
    for (const TerrainPoint& point : points->points()) {
        REQUIRE(point.northingMeters > worldSize * 0.45);
    }
}

/* -------------------------------------------------------------- writers -- */

TEST_CASE("a splat map packs its layers into the channels", "[texture][formats]") {
    terrain::Heightfield red(2, 2);
    terrain::Heightfield blue(2, 2);
    red.fill(1.0f);
    blue.fill(0.5f);

    ScratchFile output("splat.png");
    const std::array<const terrain::Heightfield*, 4> layers{&red, nullptr, &blue, nullptr};
    terrain::formats::writeSplatPng(output.path(), layers);

    const terrain::formats::PngImage image = terrain::formats::readPng(output.path());
    REQUIRE(image.channels == 4);
    REQUIRE(image.bitDepth == 8);
    REQUIRE(image.samples[0] == 255);
    REQUIRE(image.samples[1] == 0);
    REQUIRE(image.samples[2] == 128);
    REQUIRE(image.samples[3] == 0);
}

TEST_CASE("a colour map is written exactly as it was held", "[texture][formats]") {
    terrain::ColorField colors(2, 2);
    colors.at(0, 0)[0] = 1.0f;
    colors.at(0, 0)[1] = 128.0f / 255.0f;
    colors.at(0, 0)[2] = 0.0f;

    ScratchFile output("color.png");
    terrain::formats::writeColorPng(output.path(), colors);

    const terrain::formats::PngImage image = terrain::formats::readPng(output.path());
    REQUIRE(image.channels == 3);
    REQUIRE(image.samples[0] == 255);
    REQUIRE(image.samples[1] == 128);
    REQUIRE(image.samples[2] == 0);
}

TEST_CASE("a point set writes the columns its consumers expect", "[vegetation][formats]") {
    terrain::PointSet points;
    points.add(TerrainPoint{100.0f, 250.0f, 1200.0f, 45.0f, 1.5f, 7});

    ScratchFile output("points.csv");
    terrain::formats::writePointsCsv(output.path(), points, terrain::GeoReference{});

    const auto bytes = terrain::testing::readAllBytes(output.path());
    const std::string text(bytes.begin(), bytes.end());
    REQUIRE(text.starts_with("x_meters,y_meters,altitude_meters,rotation_degrees,scale,id\n"));
    REQUIRE(text.find("100,-250,1200,45,1.5,7") != std::string::npos);
}

TEST_CASE("a point set can leave as JSON instead", "[vegetation][formats]") {
    terrain::PointSet points;
    points.add(TerrainPoint{10.0f, 20.0f, 30.0f, 0.0f, 1.0f, 3});

    ScratchFile output("points.json");
    terrain::formats::writePointsJson(output.path(), points, terrain::GeoReference{});

    const auto bytes = terrain::testing::readAllBytes(output.path());
    const std::string text(bytes.begin(), bytes.end());
    REQUIRE(text.find("\"count\":1") != std::string::npos);
    REQUIRE(text.find("\"altitude\":30") != std::string::npos);
}

TEST_CASE("the exporters write only during a build", "[texture][nodes]") {
    ScratchFile colorFile("exported-color.png");
    Graph graph = rampOverRamp("0:#000000;1:#ffffff");
    terrain::GraphNode& writer = graph.addNode("write", "ExportColor");
    writer.parameters["path"] = ParameterValue{0.0, colorFile.path().string()};
    graph.connect("colors", 0, "write", 0);

    const terrain::NodeRegistry registry = terrain::testing::builtinRegistry();
    terrain::Evaluator evaluator(registry);
    evaluator.evaluate(graph, "write", 0, terrain::EvaluationOptions{});
    REQUIRE_FALSE(std::filesystem::exists(colorFile.path()));

    terrain::EvaluationOptions build;
    build.isFinalBuild = true;
    const terrain::EvaluationOutcome outcome = evaluator.evaluate(graph, "write", 0, build);
    REQUIRE(outcome.errors.empty());
    REQUIRE(std::filesystem::exists(colorFile.path()));
}
