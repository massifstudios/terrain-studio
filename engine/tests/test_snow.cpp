#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
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
/// Twenty meter cells, which is fine enough that a settle duration of a few
/// tens of meters is more than one slumping pass.
constexpr double worldSize = 1920.0;

/// A clean cone: steep flanks that end in flat ground, which is what lets snow
/// slide off one and pile up on the other.
Graph coneGround(double heightMeters, double radiusMeters) {
    Graph graph = squareGraph(resolution, worldSize);
    terrain::GraphNode& cone = graph.addNode("ground", "Mountain");
    cone.parameters["height"] = ParameterValue{heightMeters, ""};
    cone.parameters["radius"] = ParameterValue{radiusMeters, ""};
    cone.parameters["centreEast"] = ParameterValue{0.5, ""};
    cone.parameters["centreNorth"] = ParameterValue{0.5, ""};
    cone.parameters["irregularity"] = ParameterValue{0.0, ""};
    cone.parameters["detailStrength"] = ParameterValue{0.0, ""};
    cone.parameters["peakSharpness"] = ParameterValue{1.0, ""};
    cone.parameters["baseAltitude"] = ParameterValue{0.0, ""};
    return graph;
}

Graph rampGround(double riseMeters) {
    Graph graph = squareGraph(resolution, worldSize);
    terrain::GraphNode& ramp = graph.addNode("ground", "Gradient");
    ramp.parameters["direction"] = ParameterValue{0.0, ""};
    ramp.parameters["startValue"] = ParameterValue{0.0, ""};
    ramp.parameters["endValue"] = ParameterValue{riseMeters, ""};
    return graph;
}

/// Snow over the whole terrain, with nothing held back by altitude, by the
/// steepness of the ground or by the thaw. What is left is the settling.
terrain::GraphNode& plainSnow(Graph& graph, const std::string& type) {
    terrain::GraphNode& snow = graph.addNode("snow", type);
    snow.parameters["snowLine"] = ParameterValue{-9000.0, ""};
    snow.parameters["snowLineFalloff"] = ParameterValue{0.0, ""};
    snow.parameters["melt"] = ParameterValue{0.0, ""};
    graph.connect("ground", 0, "snow", 0);
    return snow;
}

double totalDepth(const Heightfield& ground, const Heightfield& surface) {
    double total = 0.0;
    for (std::size_t index = 0; index < ground.sampleCount(); ++index) {
        total += surface.data()[index] - ground.data()[index];
    }
    return total;
}

float slopeDegreesAt(const Heightfield& field, int row, int column) {
    const int size = field.width();
    const int north = std::max(row - 1, 0);
    const int south = std::min(row + 1, size - 1);
    const int west = std::max(column - 1, 0);
    const int east = std::min(column + 1, size - 1);
    const double run = field.cellSizeMeters();
    const double alongRows =
        (field.at(south, column) - field.at(north, column)) / ((south - north) * run);
    const double alongColumns = (field.at(row, east) - field.at(row, west)) / ((east - west) * run);
    const double gradient = std::hypot(alongRows, alongColumns);
    return static_cast<float>(std::atan(gradient) * 180.0 / std::numbers::pi);
}

/// Mean snow depth over the cells whose ground slope falls in a band.
double meanDepthOnSlopes(const Heightfield& ground, const Heightfield& depth, float lowDegrees,
                         float highDegrees) {
    double total = 0.0;
    int cells = 0;
    for (int row = 0; row < ground.width(); ++row) {
        for (int column = 0; column < ground.width(); ++column) {
            const float slope = slopeDegreesAt(ground, row, column);
            if (slope < lowDegrees || slope > highDegrees) {
                continue;
            }
            total += depth.at(row, column);
            cells += 1;
        }
    }
    REQUIRE(cells > 0);
    return total / cells;
}

/// Mean snow depth over the northern or the southern half of the grid, which
/// on a cone are the north-facing and the south-facing flanks.
double meanDepthOverRows(const Heightfield& depth, int firstRow, int lastRow) {
    const int size = depth.width();
    double total = 0.0;
    for (int row = firstRow; row < lastRow; ++row) {
        for (int column = 0; column < size; ++column) {
            total += depth.at(row, column);
        }
    }
    return total / ((lastRow - firstRow) * size);
}

double meanSample(const Heightfield& field) {
    double total = 0.0;
    for (const float sample : field.samples()) {
        total += sample;
    }
    return total / field.sampleCount();
}

}  // namespace

/* =================================================================== Snow == */

TEST_CASE("no snow settles below the snow line", "[snow]") {
    Graph graph = rampGround(1000.0);
    terrain::GraphNode& snow = graph.addNode("snow", "Snow");
    snow.parameters["snowLine"] = ParameterValue{600.0, ""};
    snow.parameters["snowLineFalloff"] = ParameterValue{0.0, ""};
    snow.parameters["melt"] = ParameterValue{0.0, ""};
    // A ramp this gentle is nowhere near sliding, so nothing can creep down
    // across the line after it has landed.
    snow.parameters["slipOffAngle"] = ParameterValue{60.0, ""};
    graph.connect("ground", 0, "snow", 0);

    const auto ground = evaluateNode(graph, "ground");
    const auto depth = evaluateOutput(graph, "snow", 2);

    double belowTheLine = 0.0;
    double aboveTheLine = 0.0;
    for (std::size_t index = 0; index < ground->sampleCount(); ++index) {
        if (ground->data()[index] < 600.0f) {
            belowTheLine += depth->data()[index];
            continue;
        }
        aboveTheLine += depth->data()[index];
    }

    REQUIRE(belowTheLine == Approx(0.0).margin(1.0e-6));
    REQUIRE(aboveTheLine > 0.0);
}

TEST_CASE("snow slides off a face steeper than the slip off angle", "[snow]") {
    // The cone's flanks stand at about 68 degrees, so a slip off angle below
    // that lets the snow go and one above it holds everything that fell.
    const auto depthsWithSlipOff = [](double angleDegrees) {
        Graph graph = coneGround(1500.0, 600.0);
        terrain::GraphNode& snow = plainSnow(graph, "Snow");
        snow.parameters["intensity"] = ParameterValue{3.0, ""};
        snow.parameters["slipOffAngle"] = ParameterValue{angleDegrees, ""};
        snow.parameters["adheredSnowMass"] = ParameterValue{0.3, ""};
        // Every cell takes the same snowfall, so what is left on the flanks is
        // the settling's doing and not the snowfall's.
        snow.parameters["verticality"] = ParameterValue{1.0, ""};
        const auto ground = evaluateNode(graph, "ground");
        const auto depth = evaluateOutput(graph, "snow", 2);
        return std::make_pair(meanDepthOnSlopes(*ground, *depth, 45.0f, 90.0f),
                              meanDepthOnSlopes(*ground, *depth, 0.0f, 5.0f));
    };

    const auto sliding = depthsWithSlipOff(30.0);
    const auto holding = depthsWithSlipOff(80.0);

    REQUIRE(holding.first == Approx(3.0).epsilon(1.0e-3));
    REQUIRE(sliding.first < holding.first * 0.6);
    // What came off the flanks is lying on the flat ground around them.
    REQUIRE(sliding.second > 3.0);
}

TEST_CASE("the adhered mass is what a steep face keeps", "[snow]") {
    const auto flankDepthWith = [](double adheredMeters) {
        Graph graph = coneGround(1500.0, 600.0);
        terrain::GraphNode& snow = plainSnow(graph, "Snow");
        snow.parameters["intensity"] = ParameterValue{3.0, ""};
        snow.parameters["slipOffAngle"] = ParameterValue{30.0, ""};
        snow.parameters["adheredSnowMass"] = ParameterValue{adheredMeters, ""};
        snow.parameters["verticality"] = ParameterValue{1.0, ""};
        const auto ground = evaluateNode(graph, "ground");
        const auto depth = evaluateOutput(graph, "snow", 2);
        return meanDepthOnSlopes(*ground, *depth, 45.0f, 90.0f);
    };

    REQUIRE(flankDepthWith(1.0) > flankDepthWith(0.0) + 0.5);
}

TEST_CASE("settling moves the snow about without losing any of it", "[snow]") {
    Graph graph = coneGround(1500.0, 600.0);
    terrain::GraphNode& snow = plainSnow(graph, "Snow");
    snow.parameters["intensity"] = ParameterValue{3.0, ""};
    snow.parameters["verticality"] = ParameterValue{1.0, ""};

    const auto ground = evaluateNode(graph, "ground");
    const auto surface = evaluateNode(graph, "snow");

    // Nothing may leave across an edge, so the only difference is the rounding
    // the fixed-point accumulator introduces.
    REQUIRE(totalDepth(*ground, *surface) == Approx(3.0 * ground->sampleCount()).epsilon(1.0e-3));
}

TEST_CASE("the thaw takes snow away", "[snow]") {
    const auto depthWithMelt = [](double melt) {
        Graph graph = coneGround(1500.0, 600.0);
        terrain::GraphNode& snow = plainSnow(graph, "Snow");
        snow.parameters["melt"] = ParameterValue{melt, ""};
        snow.parameters["meltRemnants"] = ParameterValue{0.0, ""};
        const auto ground = evaluateNode(graph, "ground");
        const auto surface = evaluateNode(graph, "snow");
        return totalDepth(*ground, *surface);
    };

    REQUIRE(depthWithMelt(0.5) < depthWithMelt(0.0) * 0.75);
    REQUIRE(depthWithMelt(0.5) > 0.0);
}

TEST_CASE("melt remnants leave patches the thaw did not reach", "[snow]") {
    const auto depthWithRemnants = [](double remnants) {
        Graph graph = coneGround(1500.0, 600.0);
        terrain::GraphNode& snow = plainSnow(graph, "Snow");
        snow.parameters["melt"] = ParameterValue{1.0, ""};
        snow.parameters["meltRemnants"] = ParameterValue{remnants, ""};
        const auto ground = evaluateNode(graph, "ground");
        const auto surface = evaluateNode(graph, "snow");
        return totalDepth(*ground, *surface);
    };

    REQUIRE(depthWithRemnants(0.0) == Approx(0.0).margin(1.0e-3));
    REQUIRE(depthWithRemnants(0.5) > 0.0);
}

TEST_CASE("directional melt leaves more snow on the shaded side", "[snow]") {
    Graph graph = coneGround(1500.0, 600.0);
    terrain::GraphNode& snow = plainSnow(graph, "Snow");
    snow.parameters["meltType"] = ParameterValue{1.0, ""};
    snow.parameters["melt"] = ParameterValue{0.8, ""};
    snow.parameters["meltRemnants"] = ParameterValue{0.0, ""};
    snow.parameters["direction"] = ParameterValue{180.0, ""};

    const auto depth = evaluateOutput(graph, "snow", 2);

    // The sun stands in the south, so the north-facing half keeps its snow.
    const int size = depth->width();
    REQUIRE(meanDepthOverRows(*depth, 0, size / 2)
            > meanDepthOverRows(*depth, size / 2, size) * 1.15);
}

TEST_CASE("snow reports where it lies and how deep it is", "[snow]") {
    Graph graph = coneGround(1500.0, 600.0);
    plainSnow(graph, "Snow");

    const auto mask = evaluateOutput(graph, "snow", 1);
    REQUIRE(mask->semantics() == terrain::FieldSemantics::Mask);
    REQUIRE(allSamplesWithin(*mask, 0.0f, 1.0f));
    REQUIRE_FALSE(isConstant(*mask));

    const auto depth = evaluateOutput(graph, "snow", 2);
    REQUIRE(depth->semantics() == terrain::FieldSemantics::Mask);
    REQUIRE(depth->maximum() > 1.0f);
    REQUIRE(depth->minimum() >= 0.0f);
}

TEST_CASE("a mask keeps the snow where it was asked for", "[snow]") {
    Graph graph = coneGround(1500.0, 600.0);
    terrain::GraphNode& nowhere = graph.addNode("nowhere", "Constant");
    nowhere.parameters["value"] = ParameterValue{0.0, ""};
    plainSnow(graph, "Snow");
    graph.connect("nowhere", 0, "snow", 1);

    const auto depth = evaluateOutput(graph, "snow", 2);
    REQUIRE(depth->maximum() == Approx(0.0).margin(1.0e-6));
}

TEST_CASE("snow gives the same surface at one thread and at eight", "[snow][determinism]") {
    Graph graph = coneGround(1500.0, 600.0);
    plainSnow(graph, "Snow");

    REQUIRE(hashField(*evaluateNode(graph, "snow", 1))
            == hashField(*evaluateNode(graph, "snow", 8)));
}

TEST_CASE("snow gives the same surface twice over", "[snow][determinism]") {
    Graph graph = coneGround(1500.0, 600.0);
    plainSnow(graph, "Snow");

    REQUIRE(hashField(*evaluateNode(graph, "snow", 4))
            == hashField(*evaluateNode(graph, "snow", 4)));
}

TEST_CASE("the snow seed varies the melt patches alone", "[snow][determinism]") {
    const auto snowWithSeed = [](double seedValue, double remnants) {
        Graph graph = coneGround(1500.0, 600.0);
        terrain::GraphNode& snow = plainSnow(graph, "Snow");
        snow.parameters["melt"] = ParameterValue{0.6, ""};
        snow.parameters["meltRemnants"] = ParameterValue{remnants, ""};
        snow.parameters["seed"] = ParameterValue{seedValue, ""};
        return hashField(*evaluateNode(graph, "snow"));
    };

    REQUIRE(snowWithSeed(0.0, 0.5) != snowWithSeed(9.0, 0.5));
    // With nothing left behind there is no pattern for the seed to vary.
    REQUIRE(snowWithSeed(0.0, 0.0) == snowWithSeed(9.0, 0.0));
}

/* ============================================================== Snowfield == */

namespace {

terrain::GraphNode& plainSnowfield(Graph& graph) {
    terrain::GraphNode& field = plainSnow(graph, "Snowfield");
    field.parameters["surfaceDetails"] = ParameterValue{0.0, ""};
    return field;
}

}  // namespace

TEST_CASE("every cascade brings its own fall of snow", "[snow][snowfield]") {
    // Settling only moves snow about, so twice the spells of snowfall must
    // leave exactly twice the snow on the ground.
    const auto depthWithCascades = [](double cascades) {
        Graph graph = coneGround(1500.0, 600.0);
        terrain::GraphNode& field = plainSnowfield(graph);
        field.parameters["cascades"] = ParameterValue{cascades, ""};
        field.parameters["intensity"] = ParameterValue{2.0, ""};
        const auto ground = evaluateNode(graph, "ground");
        const auto surface = evaluateNode(graph, "snow");
        return totalDepth(*ground, *surface);
    };

    REQUIRE(depthWithCascades(4.0) == Approx(depthWithCascades(2.0) * 2.0).epsilon(1.0e-3));
    REQUIRE(depthWithCascades(2.0) > 0.0);
}

TEST_CASE("tapering leaves less snow with every spell", "[snow][snowfield]") {
    const auto depthWithTaper = [](double taper) {
        Graph graph = coneGround(1500.0, 600.0);
        terrain::GraphNode& field = plainSnowfield(graph);
        field.parameters["taperSnowfall"] = ParameterValue{taper, ""};
        field.parameters["taperAmount"] = ParameterValue{0.5, ""};
        const auto ground = evaluateNode(graph, "ground");
        const auto surface = evaluateNode(graph, "snow");
        return totalDepth(*ground, *surface);
    };

    REQUIRE(depthWithTaper(1.0) < depthWithTaper(0.0) * 0.75);
}

TEST_CASE("furrows lower the snow where the flow gathers", "[snow][snowfield]") {
    const auto depthWithFlows = [](double flows) {
        Graph graph = coneGround(1500.0, 600.0);
        terrain::GraphNode& field = plainSnowfield(graph);
        field.parameters["flows"] = ParameterValue{flows, ""};
        field.parameters["flowsDepth"] = ParameterValue{1.0, ""};
        return evaluateOutput(graph, "snow", 0);
    };

    Graph flowGraph = coneGround(1500.0, 600.0);
    flowGraph.addNode("flow", "FlowMask");
    flowGraph.connect("ground", 0, "flow", 0);
    const auto flow = evaluateNode(flowGraph, "flow");

    const auto without = depthWithFlows(0.0);
    const auto with = depthWithFlows(1.0);

    double gathered = 0.0;
    double elsewhere = 0.0;
    int gatheredCells = 0;
    int elsewhereCells = 0;
    for (std::size_t index = 0; index < flow->sampleCount(); ++index) {
        const double cut = without->data()[index] - with->data()[index];
        REQUIRE(cut >= -1.0e-6);
        if (flow->data()[index] > 0.8f) {
            gathered += cut;
            gatheredCells += 1;
            continue;
        }
        elsewhere += cut;
        elsewhereCells += 1;
    }

    REQUIRE(gatheredCells > 0);
    REQUIRE(gathered / gatheredCells > elsewhere / elsewhereCells * 2.0);
}

TEST_CASE("sharp buildup leaves the last fall of snow where it landed", "[snow][snowfield]") {
    const auto flankDepthWith = [](double sharp) {
        Graph graph = coneGround(1500.0, 600.0);
        terrain::GraphNode& field = plainSnowfield(graph);
        field.parameters["sharpBuildup"] = ParameterValue{sharp, ""};
        field.parameters["verticality"] = ParameterValue{1.0, ""};
        field.parameters["slipOffAngle"] = ParameterValue{30.0, ""};
        const auto ground = evaluateNode(graph, "ground");
        const auto surface = evaluateNode(graph, "snow");
        return meanDepthOnSlopes(*ground, *surface, 45.0f, 90.0f)
               - meanDepthOnSlopes(*ground, *ground, 45.0f, 90.0f);
    };

    REQUIRE(flankDepthWith(1.0) > flankDepthWith(0.0));
}

TEST_CASE("wind drives the snow onto the slopes it faces", "[snow][snowfield]") {
    Graph graph = coneGround(1500.0, 600.0);
    terrain::GraphNode& field = plainSnowfield(graph);
    field.parameters["direction"] = ParameterValue{1.0, ""};
    field.parameters["settleDuration"] = ParameterValue{20.0, ""};

    const auto ground = evaluateNode(graph, "ground");
    const auto surface = evaluateNode(graph, "snow");

    Graph bare = coneGround(1500.0, 600.0);
    const auto reference = evaluateNode(bare, "ground");
    REQUIRE(hashField(*ground) == hashField(*reference));

    // Snow driven north gathers on the north-facing flank.
    double north = 0.0;
    double south = 0.0;
    const int size = ground->width();
    for (int row = 0; row < size; ++row) {
        for (int column = 0; column < size; ++column) {
            const double depth = surface->at(row, column) - ground->at(row, column);
            if (row < size / 2) {
                north += depth;
                continue;
            }
            south += depth;
        }
    }

    REQUIRE(north > south);
}

TEST_CASE("surface details roughen deep snow and leave bare ground alone",
          "[snow][snowfield]") {
    const auto surfaceWith = [](double details) {
        Graph graph = coneGround(1500.0, 600.0);
        terrain::GraphNode& field = plainSnowfield(graph);
        field.parameters["surfaceDetails"] = ParameterValue{details, ""};
        field.parameters["snowLine"] = ParameterValue{800.0, ""};
        field.parameters["snowLineFalloff"] = ParameterValue{0.0, ""};
        return evaluateNode(graph, "snow");
    };

    const auto ground = evaluateNode(coneGround(1500.0, 600.0), "ground");
    const auto smooth = surfaceWith(0.0);
    const auto rough = surfaceWith(1.0);

    bool differsWhereDeep = false;
    for (std::size_t index = 0; index < ground->sampleCount(); ++index) {
        const double depth = smooth->data()[index] - ground->data()[index];
        const double difference = std::abs(rough->data()[index] - smooth->data()[index]);
        if (depth < 1.0e-6) {
            REQUIRE(difference == Approx(0.0).margin(1.0e-6));
            continue;
        }
        differsWhereDeep = differsWhereDeep || difference > 1.0e-3;
    }
    REQUIRE(differsWhereDeep);
}

TEST_CASE("a snowfield gives the same surface at one thread and at eight",
          "[snow][snowfield][determinism]") {
    Graph graph = coneGround(1500.0, 600.0);
    plainSnowfield(graph);

    REQUIRE(hashField(*evaluateNode(graph, "snow", 1))
            == hashField(*evaluateNode(graph, "snow", 8)));
}

TEST_CASE("a snowfield gives the same surface twice over", "[snow][snowfield][determinism]") {
    Graph graph = coneGround(1500.0, 600.0);
    plainSnowfield(graph);

    REQUIRE(hashField(*evaluateNode(graph, "snow", 4))
            == hashField(*evaluateNode(graph, "snow", 4)));
}

TEST_CASE("the snowfield seed varies that node alone", "[snow][snowfield][determinism]") {
    const auto fieldWithSeed = [](double seedValue) {
        Graph graph = coneGround(1500.0, 600.0);
        terrain::GraphNode& field = plainSnow(graph, "Snowfield");
        field.parameters["surfaceDetails"] = ParameterValue{0.5, ""};
        field.parameters["seed"] = ParameterValue{seedValue, ""};
        return hashField(*evaluateNode(graph, "snow"));
    };

    REQUIRE(fieldWithSeed(0.0) != fieldWithSeed(9.0));
}

/* ================================================================ Dusting == */

namespace {

terrain::GraphNode& plainDusting(Graph& graph) {
    terrain::GraphNode& dusting = graph.addNode("dusting", "Dusting");
    dusting.parameters["snowLine"] = ParameterValue{-9000.0, ""};
    dusting.parameters["falloff"] = ParameterValue{0.0, ""};
    graph.connect("ground", 0, "dusting", 0);
    return dusting;
}

}  // namespace

TEST_CASE("a dusting keeps off the steep ground", "[snow][dusting]") {
    Graph graph = coneGround(1500.0, 600.0);
    terrain::GraphNode& dusting = plainDusting(graph);
    dusting.parameters["slopeCoverageStart"] = ParameterValue{20.0, ""};
    dusting.parameters["slopeCoverageEnd"] = ParameterValue{40.0, ""};

    const auto ground = evaluateNode(graph, "ground");
    const auto lying = evaluateNode(graph, "dusting");

    REQUIRE(lying->semantics() == terrain::FieldSemantics::Mask);
    REQUIRE(allSamplesWithin(*lying, 0.0f, 1.0f));
    REQUIRE(meanDepthOnSlopes(*ground, *lying, 45.0f, 90.0f) == Approx(0.0).margin(1.0e-6));
    REQUIRE(meanDepthOnSlopes(*ground, *lying, 0.0f, 10.0f) > 0.1);
}

TEST_CASE("a dusting settles no lower than the snow line", "[snow][dusting]") {
    Graph graph = rampGround(1000.0);
    terrain::GraphNode& dusting = plainDusting(graph);
    dusting.parameters["snowLine"] = ParameterValue{600.0, ""};
    dusting.parameters["falloff"] = ParameterValue{0.0, ""};

    const auto ground = evaluateNode(graph, "ground");
    const auto lying = evaluateNode(graph, "dusting");

    for (std::size_t index = 0; index < ground->sampleCount(); ++index) {
        if (ground->data()[index] >= 600.0f) {
            continue;
        }
        REQUIRE(lying->data()[index] == Approx(0.0).margin(1.0e-6));
    }
    REQUIRE(lying->maximum() > 0.0f);
}

TEST_CASE("coverage decides how much ground a dusting reaches", "[snow][dusting]") {
    const auto meanCoverAt = [](double coverage) {
        Graph graph = coneGround(1500.0, 600.0);
        terrain::GraphNode& dusting = plainDusting(graph);
        dusting.parameters["coverage"] = ParameterValue{coverage, ""};
        return meanSample(*evaluateNode(graph, "dusting"));
    };

    REQUIRE(meanCoverAt(0.2) < meanCoverAt(0.9));
    REQUIRE(meanCoverAt(0.0) == Approx(0.0).margin(1.0e-6));
}

TEST_CASE("peaks put more of a dusting on what stands proud", "[snow][dusting]") {
    const auto meanCoverAt = [](double peaks) {
        Graph graph = coneGround(1500.0, 600.0);
        terrain::GraphNode& dusting = plainDusting(graph);
        dusting.parameters["peaks"] = ParameterValue{peaks, ""};
        return meanSample(*evaluateNode(graph, "dusting"));
    };

    REQUIRE(meanCoverAt(1.0) > meanCoverAt(0.0));
}

TEST_CASE("the dusting surface is the ground raised by its depth", "[snow][dusting]") {
    Graph graph = coneGround(1500.0, 600.0);
    terrain::GraphNode& dusting = plainDusting(graph);
    dusting.parameters["depth"] = ParameterValue{2.0, ""};

    const auto ground = evaluateNode(graph, "ground");
    const auto lying = evaluateOutput(graph, "dusting", 0);
    const auto surface = evaluateOutput(graph, "dusting", 1);

    REQUIRE(surface->semantics() == terrain::FieldSemantics::Heightfield);
    for (std::size_t index = 0; index < ground->sampleCount(); ++index) {
        const double raised = surface->data()[index] - ground->data()[index];
        REQUIRE(raised == Approx(lying->data()[index] * 2.0).margin(1.0e-4));
    }
}

TEST_CASE("a dusting lies over the snow that is already there", "[snow][dusting]") {
    const auto meanCoverWith = [](double include) {
        Graph graph = coneGround(1500.0, 600.0);
        terrain::GraphNode& already = graph.addNode("already", "Constant");
        already.parameters["value"] = ParameterValue{1.0, ""};
        terrain::GraphNode& dusting = plainDusting(graph);
        dusting.parameters["includeExistingSnow"] = ParameterValue{include, ""};
        graph.connect("already", 0, "dusting", 2);
        return meanSample(*evaluateNode(graph, "dusting"));
    };

    REQUIRE(meanCoverWith(1.0) == Approx(1.0).margin(1.0e-6));
    REQUIRE(meanCoverWith(0.0) < 1.0);
}

TEST_CASE("a dusting gives the same cover at one thread and at eight",
          "[snow][dusting][determinism]") {
    Graph graph = coneGround(1500.0, 600.0);
    terrain::GraphNode& dusting = plainDusting(graph);
    dusting.parameters["flow"] = ParameterValue{0.5, ""};
    dusting.parameters["gritty"] = ParameterValue{1.0, ""};

    REQUIRE(hashField(*evaluateNode(graph, "dusting", 1))
            == hashField(*evaluateNode(graph, "dusting", 8)));
    REQUIRE(hashField(*evaluateNode(graph, "dusting", 4))
            == hashField(*evaluateNode(graph, "dusting", 4)));
}

TEST_CASE("the dusting seed varies that node alone", "[snow][dusting][determinism]") {
    const auto dustingWithSeed = [](double seedValue) {
        Graph graph = coneGround(1500.0, 600.0);
        terrain::GraphNode& dusting = plainDusting(graph);
        dusting.parameters["seed"] = ParameterValue{seedValue, ""};
        return hashField(*evaluateNode(graph, "dusting"));
    };

    REQUIRE(dustingWithSeed(0.0) != dustingWithSeed(9.0));
}
