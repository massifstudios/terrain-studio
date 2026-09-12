#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "node_test_support.h"
#include "terrain/color_ramp.h"
#include "terrain/color_space.h"
#include "terrain/color_strip.h"
#include "terrain/registry.h"
#include "terrain/satmap_catalogue.h"
#include "terrain/texture_catalogue.h"
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
Graph satMapOverRamp(double library, double item, double reverse, double enhance) {
    Graph graph = squareGraph(resolution, worldSize);
    terrain::GraphNode& ground = graph.addNode("ground", "Gradient");
    ground.parameters["direction"] = ParameterValue{0.0, ""};
    ground.parameters["startValue"] = ParameterValue{0.0, ""};
    ground.parameters["endValue"] = ParameterValue{1000.0, ""};

    terrain::GraphNode& colors = graph.addNode("colors", "SatMap");
    colors.parameters["library"] = ParameterValue{library, ""};
    colors.parameters["item"] = ParameterValue{item, ""};
    colors.parameters["reverse"] = ParameterValue{reverse, ""};
    colors.parameters["enhance"] = ParameterValue{enhance, ""};
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

/* --------------------------------------------------------------- SatMap -- */

const terrain::SatMapCatalogue& catalogue() {
    return terrain::SatMapCatalogue::shared();
}

int libraryNamed(const std::string& name) {
    const std::vector<terrain::SatMapLibrary>& libraries = catalogue().libraries();
    for (std::size_t index = 0; index < libraries.size(); ++index) {
        if (libraries[index].name == name) {
            return static_cast<int>(index);
        }
    }
    return -1;
}

TEST_CASE("the source tree's satmap libraries are found without configuration",
          "[texture][satmap]") {
    const std::vector<terrain::SatMapLibrary>& libraries = catalogue().libraries();
    REQUIRE(libraries.size() >= 6);
    for (const terrain::SatMapLibrary& library : libraries) {
        REQUIRE(!library.strips.empty());
    }
    for (std::size_t index = 1; index < libraries.size(); ++index) {
        REQUIRE(libraries[index - 1].name < libraries[index].name);
    }
}

TEST_CASE("a satellite strip reads dark shades first and blends between columns",
          "[texture][satmap]") {
    // Rock strips are ordered by elevation, so each column keeps the spread of
    // shades that height shows; a strip ordered by luminance has none to keep.
    const std::shared_ptr<const terrain::ColorStrip> strip =
        catalogue().strip(libraryNamed("rock"), 0);
    REQUIRE(strip != nullptr);
    REQUIRE(strip->width() == 1024);
    REQUIRE(strip->rowCount() == 16);

    const terrain::RgbColor darkest = strip->sample(0.5f, 0);
    const terrain::RgbColor brightest = strip->sample(0.5f, strip->rowCount() - 1);
    REQUIRE(terrain::luminance(darkest) < terrain::luminance(brightest));

    const terrain::RgbColor average = strip->sampleAverage(0.5f);
    REQUIRE(average.red >= std::min(darkest.red, brightest.red) - 1e-5f);
    REQUIRE(average.red <= std::max(darkest.red, brightest.red) + 1e-5f);
}

TEST_CASE("a satmap reads its strip from the foot of the field to its crest",
          "[texture][satmap]") {
    const std::shared_ptr<const terrain::ColorStrip> strip = catalogue().strip(0, 0);
    const auto colors = evaluateColor(satMapOverRamp(0.0, 0.0, 0.0, 1.0), "colors");
    REQUIRE(colors != nullptr);

    REQUIRE(colors->at(0, 0)[0] == Approx(strip->sampleAverage(1.0f).red).margin(0.02));
    REQUIRE(colors->at(resolution - 1, 0)[0]
            == Approx(strip->sampleAverage(0.0f).red).margin(0.02));
}

TEST_CASE("reversing a satmap reads the strip high ground first", "[texture][satmap]") {
    const std::shared_ptr<const terrain::ColorStrip> strip = catalogue().strip(0, 0);
    const auto colors = evaluateColor(satMapOverRamp(0.0, 0.0, 1.0, 1.0), "colors");

    REQUIRE(colors->at(0, 0)[0] == Approx(strip->sampleAverage(0.0f).red).margin(0.02));
    REQUIRE(colors->at(resolution - 1, 0)[0]
            == Approx(strip->sampleAverage(1.0f).red).margin(0.02));
}

TEST_CASE("every satmap library and strip paints a usable map", "[texture][satmap]") {
    const std::vector<terrain::SatMapLibrary>& libraries = catalogue().libraries();
    for (std::size_t library = 0; library < libraries.size(); ++library) {
        for (std::size_t item = 0; item < libraries[library].strips.size(); ++item) {
            const Graph graph = satMapOverRamp(static_cast<double>(library),
                                               static_cast<double>(item), 0.0, 1.0);
            const auto colors = evaluateColor(graph, "colors");
            REQUIRE(colors != nullptr);
            REQUIRE(channelsWithinUnit(*colors));
            REQUIRE(colors->at(0, 0)[1] != colors->at(resolution - 1, 0)[1]);
        }
    }
}

TEST_CASE("a satmap's map index counts round past the last strip", "[texture][satmap]") {
    const double stripCount = static_cast<double>(catalogue().libraries()[1].strips.size());
    const auto first = evaluateColor(satMapOverRamp(1.0, 0.0, 0.0, 1.0), "colors");
    const auto wrapped = evaluateColor(satMapOverRamp(1.0, stripCount, 0.0, 1.0), "colors");
    REQUIRE(first->at(resolution / 3, 5)[0] == Approx(wrapped->at(resolution / 3, 5)[0]));
}

TEST_CASE("a library that does not exist is an error, not a black map", "[texture][satmap]") {
    REQUIRE(evaluateColor(satMapOverRamp(99.0, 0.0, 0.0, 1.0), "colors") == nullptr);
}

TEST_CASE("clipping a satmap to one point paints that colour everywhere",
          "[texture][satmap]") {
    Graph graph = satMapOverRamp(0.0, 0.0, 0.0, 1.0);
    graph.find("colors")->parameters["rangeStart"] = ParameterValue{0.5, ""};
    graph.find("colors")->parameters["rangeEnd"] = ParameterValue{0.5, ""};
    const auto colors = evaluateColor(graph, "colors");
    const terrain::RgbColor expected = catalogue().strip(0, 0)->sampleAverage(0.5f);

    REQUIRE(colors->at(0, 0)[1] == Approx(expected.green).margin(1e-5));
    REQUIRE(colors->at(resolution - 1, resolution - 1)[1] == Approx(expected.green).margin(1e-5));
}

TEST_CASE("a strip of the artist's own replaces the library", "[texture][satmap]") {
    ScratchFile stripFile("own_strip.png");
    const std::array<std::uint8_t, 6> pixels{255, 0, 0, 0, 0, 255};
    terrain::formats::writePngRgb8(stripFile.path(), 2, 1, pixels);

    Graph graph = satMapOverRamp(0.0, 0.0, 0.0, 1.0);
    graph.find("colors")->parameters["strip"] = ParameterValue{0.0, stripFile.path().string()};
    const auto colors = evaluateColor(graph, "colors");
    REQUIRE(colors != nullptr);

    REQUIRE(colors->at(resolution - 1, 0)[0] == Approx(1.0).margin(0.03));
    REQUIRE(colors->at(0, 0)[2] == Approx(1.0).margin(0.03));
}

TEST_CASE("roughness scatters the read but stays the same across threads and runs",
          "[texture][satmap][determinism]") {
    Graph graph = satMapOverRamp(0.0, 0.0, 0.0, 1.0);
    graph.find("colors")->parameters["rough"] = ParameterValue{4.0, ""};
    const auto smooth = evaluateColor(satMapOverRamp(0.0, 0.0, 0.0, 1.0), "colors");
    const auto roughOnce = evaluateColor(graph, "colors", 0, 1);
    const auto roughAgain = evaluateColor(graph, "colors", 0, 8);

    int differing = 0;
    for (std::size_t index = 0; index < smooth->samples().size(); ++index) {
        REQUIRE(roughOnce->samples()[index] == roughAgain->samples()[index]);
        if (smooth->samples()[index] != roughOnce->samples()[index]) {
            ++differing;
        }
    }
    REQUIRE(differing > static_cast<int>(smooth->samples().size() / 2));
}

TEST_CASE("grading a satmap to full lightness paints white", "[texture][satmap]") {
    Graph graph = satMapOverRamp(0.0, 0.0, 0.0, 1.0);
    graph.find("colors")->parameters["lightness"] = ParameterValue{1.0, ""};
    const auto colors = evaluateColor(graph, "colors");
    REQUIRE(colors->at(5, 5)[0] == Approx(1.0).margin(1e-4));
    REQUIRE(colors->at(5, 5)[2] == Approx(1.0).margin(1e-4));
}

/* ------------------------------------------------- shared colour lookup -- */

/// A black-to-white ramp over a ramp that lingers low: the share of cells
/// reading the dark half of the gradient says how the field was spread.
float darkShareOverSkewedRamp(double enhance, double bias) {
    Graph graph = squareGraph(resolution, worldSize);
    terrain::GraphNode& ground = graph.addNode("ground", "Gradient");
    ground.parameters["direction"] = ParameterValue{0.0, ""};
    ground.parameters["startValue"] = ParameterValue{0.0, ""};
    ground.parameters["endValue"] = ParameterValue{1000.0, ""};
    terrain::GraphNode& skew = graph.addNode("skew", "Curve");
    skew.parameters["curve"] = ParameterValue{0.0, "0,0;0.5,0.08;1,1"};
    terrain::GraphNode& colors = graph.addNode("colors", "ColorRamp");
    colors.parameters["ramp"] = ParameterValue{0.0, "0:#000000;1:#ffffff"};
    colors.parameters["enhance"] = ParameterValue{enhance, ""};
    colors.parameters["bias"] = ParameterValue{bias, ""};
    graph.connect("ground", 0, "skew", 0);
    graph.connect("skew", 0, "colors", 0);

    const auto field = evaluateColor(graph, "colors");
    int darkCells = 0;
    for (int row = 0; row < resolution; ++row) {
        for (int column = 0; column < resolution; ++column) {
            if (field->at(row, column)[0] <= 0.5f) {
                ++darkCells;
            }
        }
    }
    return static_cast<float>(darkCells) / (resolution * resolution);
}

TEST_CASE("equalizing gives every colour the same share of ground", "[texture][ramp]") {
    // The skewed ramp keeps most cells low, so over its own range the high
    // colours barely appear; by rank they take their even share.
    REQUIRE(darkShareOverSkewedRamp(1.0, 0.0) > 0.7f);
    REQUIRE(darkShareOverSkewedRamp(2.0, 0.0) == Approx(0.5).margin(0.05));
}

TEST_CASE("bias leans the gradient towards one end", "[texture][ramp]") {
    REQUIRE(darkShareOverSkewedRamp(2.0, -1.0) > 0.75f);
    REQUIRE(darkShareOverSkewedRamp(2.0, 1.0) < 0.15f);
}

TEST_CASE("reading a mask with no enhancement keeps its own values", "[texture][ramp]") {
    Graph graph = rampOverRamp("0:#000000;1:#ffffff");
    graph.find("ground")->parameters["endValue"] = ParameterValue{0.5, ""};
    graph.find("colors")->parameters["enhance"] = ParameterValue{0.0, ""};
    const auto colors = evaluateColor(graph, "colors");
    REQUIRE(colors->at(0, 0)[0] == Approx(0.5).margin(0.02));
}

TEST_CASE("a reversed ramp reads high ground first", "[texture][ramp]") {
    Graph graph = rampOverRamp("0:#000000;1:#ffffff");
    graph.find("colors")->parameters["reverse"] = ParameterValue{1.0, ""};
    const auto colors = evaluateColor(graph, "colors");
    REQUIRE(colors->at(0, 0)[0] == Approx(0.0).margin(0.02));
    REQUIRE(colors->at(resolution - 1, 0)[0] == Approx(1.0).margin(0.02));
}

TEST_CASE("a ramp's hue turn goes round the wheel", "[texture][ramp]") {
    Graph graph = rampOverRamp("0:#ff0000;1:#ff0000");
    graph.find("colors")->parameters["hueShift"] = ParameterValue{120.0, ""};
    const auto colors = evaluateColor(graph, "colors");
    REQUIRE(colors->at(5, 5)[0] == Approx(0.0).margin(1e-4));
    REQUIRE(colors->at(5, 5)[1] == Approx(1.0).margin(1e-4));
}

/* --------------------------------------------------------- SplatWeights -- */

/// A ramp running row by row from 0 m to 1000 m, with a lower band on splat
/// input 0 (layer 2) and a higher band on input 1 (layer 3), overlapping
/// between 300 m and 400 m.
Graph splatOverRamp() {
    Graph graph = squareGraph(resolution, worldSize);
    terrain::GraphNode& ground = graph.addNode("ground", "Gradient");
    ground.parameters["direction"] = ParameterValue{0.0, ""};
    ground.parameters["startValue"] = ParameterValue{0.0, ""};
    ground.parameters["endValue"] = ParameterValue{1000.0, ""};

    addMask(graph, "low", 0.0, 400.0);
    addMask(graph, "high", 300.0, 1000.0);
    graph.connect("ground", 0, "low", 0);
    graph.connect("ground", 0, "high", 0);

    graph.addNode("splat", "SplatWeights");
    graph.connect("low", 0, "splat", 0);
    graph.connect("high", 0, "splat", 1);
    return graph;
}

TEST_CASE("splat weights add to one in every cell", "[texture][nodes]") {
    Graph graph = splatOverRamp();

    const auto base = evaluateOutput(graph, "splat", 0);
    const auto second = evaluateOutput(graph, "splat", 1);
    const auto third = evaluateOutput(graph, "splat", 2);
    const auto fourth = evaluateOutput(graph, "splat", 3);

    for (std::size_t index = 0; index < base->sampleCount(); ++index) {
        const float total = base->data()[index] + second->data()[index] + third->data()[index]
                            + fourth->data()[index];
        REQUIRE(total == Approx(1.0));
    }
    REQUIRE(fourth->maximum() == Approx(0.0));
}

TEST_CASE("a higher layer covers a lower one where both claim the ground", "[texture][nodes]") {
    Graph graph = splatOverRamp();

    const auto ground = evaluateNode(graph, "ground");
    const auto base = evaluateOutput(graph, "splat", 0);
    const auto second = evaluateOutput(graph, "splat", 1);
    const auto third = evaluateOutput(graph, "splat", 2);

    for (int row = 0; row < resolution; ++row) {
        const float altitude = ground->at(row, 0);
        if (altitude < 300.0f) {
            REQUIRE(second->at(row, 0) == Approx(1.0));
            continue;
        }
        if (altitude <= 400.0f) {
            // The overlap goes wholly to layer 3, not half to each.
            REQUIRE(third->at(row, 0) == Approx(1.0));
            REQUIRE(second->at(row, 0) == Approx(0.0));
            continue;
        }
        REQUIRE(third->at(row, 0) == Approx(1.0));
    }
    REQUIRE(base->maximum() == Approx(0.0));
}

TEST_CASE("a partial cover leaves the rest to the layers below", "[texture][nodes]") {
    Graph graph = squareGraph(resolution, worldSize);
    terrain::GraphNode& half = graph.addNode("half", "Constant");
    half.parameters["value"] = ParameterValue{0.5, ""};
    terrain::GraphNode& full = graph.addNode("full", "Constant");
    full.parameters["value"] = ParameterValue{1.0, ""};

    graph.addNode("splat", "SplatWeights");
    graph.connect("full", 0, "splat", 0);
    graph.connect("half", 0, "splat", 2);

    REQUIRE(evaluateOutput(graph, "splat", 3)->minimum() == Approx(0.5));
    REQUIRE(evaluateOutput(graph, "splat", 1)->minimum() == Approx(0.5));
    REQUIRE(evaluateOutput(graph, "splat", 0)->maximum() == Approx(0.0));
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

TEST_CASE("the source tree's textures are found without configuration", "[texture][splat]") {
    const std::vector<std::string>& names = terrain::TextureCatalogue::shared().names();
    REQUIRE(names.size() >= 6);
    REQUIRE(std::find(names.begin(), names.end(), "snow_02.jpg") != names.end());
    for (std::size_t index = 1; index < names.size(); ++index) {
        REQUIRE(names[index - 1] < names[index]);
    }
}

TEST_CASE("a directory without textures leaves the catalogue empty", "[texture][splat]") {
    const terrain::TextureCatalogue catalogue =
        terrain::TextureCatalogue::scan({std::filesystem::temp_directory_path() / "no_such_dir"});
    REQUIRE(catalogue.names().empty());
}

TEST_CASE("splat weights name one texture per layer, each starting on its own",
          "[texture][splat]") {
    terrain::NodeRegistry registry;
    terrain::registerBuiltinNodes(registry);
    const terrain::NodeType* splat = registry.find("SplatWeights");
    REQUIRE(splat != nullptr);
    REQUIRE(splat->parameters.size() == 4);

    const std::vector<std::string>& textures = terrain::TextureCatalogue::shared().names();
    for (std::size_t layer = 0; layer < splat->parameters.size(); ++layer) {
        const terrain::ParameterDescriptor& parameter = splat->parameters[layer];
        REQUIRE(parameter.type == terrain::ParameterType::Texture);
        REQUIRE(parameter.enumOptions.size() == textures.size());
        REQUIRE(parameter.defaultText == textures[layer]);
        REQUIRE(parameter.enumOptions[layer].name == textures[layer]);
    }
    REQUIRE(splat->parameters[0].name == "baseTexture");
    REQUIRE(splat->parameters[0].enumOptions[0].name == "aerial_grass_rock.jpg");
    REQUIRE(splat->parameters[0].enumOptions[0].label == "Aerial grass rock");
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
