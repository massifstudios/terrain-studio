#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

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
constexpr double worldSize = 12800.0;
constexpr int middle = resolution / 2;

/// A mountain with its two stochastic dials at zero, which leaves the analytic
/// shape on its own and makes every height below predictable.
terrain::GraphNode& addPlainMountain(Graph& graph, const std::string& id) {
    terrain::GraphNode& mountain = graph.addNode(id, "Mountain");
    mountain.parameters["radius"] = ParameterValue{4000.0, ""};
    mountain.parameters["height"] = ParameterValue{1000.0, ""};
    mountain.parameters["peakSharpness"] = ParameterValue{1.0, ""};
    mountain.parameters["irregularity"] = ParameterValue{0.0, ""};
    mountain.parameters["detailStrength"] = ParameterValue{0.0, ""};
    mountain.parameters["baseAltitude"] = ParameterValue{0.0, ""};
    return mountain;
}

/// The mean step between neighbouring cells across a row band: how rough the
/// surface is at the finest scale the grid can hold.
float meanNeighbourStep(const terrain::Heightfield& field, int size) {
    double total = 0.0;
    int steps = 0;
    for (int row = 0; row < size; ++row) {
        for (int column = 1; column < size; ++column) {
            total += std::abs(field.at(column, row) - field.at(column - 1, row));
            ++steps;
        }
    }
    return static_cast<float>(total / steps);
}

/// A range with its stochastic dials at zero and every summit at full height.
terrain::GraphNode& addPlainRange(Graph& graph, const std::string& id) {
    terrain::GraphNode& range = graph.addNode(id, "MountainRange");
    range.parameters["bearing"] = ParameterValue{0.0, ""};
    range.parameters["length"] = ParameterValue{10000.0, ""};
    range.parameters["width"] = ParameterValue{6000.0, ""};
    range.parameters["peakCount"] = ParameterValue{4.0, ""};
    range.parameters["height"] = ParameterValue{2000.0, ""};
    range.parameters["heightVariation"] = ParameterValue{0.0, ""};
    range.parameters["irregularity"] = ParameterValue{0.0, ""};
    range.parameters["detailStrength"] = ParameterValue{0.0, ""};
    return range;
}

}  // namespace

/* ------------------------------------------------------------- Mountain -- */

TEST_CASE("a mountain summits at its centre and reaches the ground at its radius",
          "[nodes][mountain]") {
    Graph graph = squareGraph(resolution, worldSize);
    addPlainMountain(graph, "peak");

    const auto field = evaluateNode(graph, "peak");
    REQUIRE(field != nullptr);
    REQUIRE(field->at(middle, middle) > 990.0f);
    REQUIRE(field->at(0, 0) == Approx(0.0));
    REQUIRE(allSamplesWithin(*field, 0.0f, 1000.0f));
}

TEST_CASE("a mountain sits on the altitude it is given", "[nodes][mountain]") {
    Graph graph = squareGraph(resolution, worldSize);
    terrain::GraphNode& mountain = addPlainMountain(graph, "peak");
    mountain.parameters["baseAltitude"] = ParameterValue{250.0, ""};

    const auto field = evaluateNode(graph, "peak");
    REQUIRE(field->minimum() == Approx(250.0));
    REQUIRE(field->maximum() == Approx(1250.0).margin(10.0));
}

TEST_CASE("a sharper peak carries less mass on its flanks", "[nodes][mountain]") {
    Graph graph = squareGraph(resolution, worldSize);
    addPlainMountain(graph, "blunt");
    terrain::GraphNode& sharp = addPlainMountain(graph, "sharp");
    sharp.parameters["peakSharpness"] = ParameterValue{3.0, ""};

    const auto bluntField = evaluateNode(graph, "blunt");
    const auto sharpField = evaluateNode(graph, "sharp");
    REQUIRE(sharpField->at(middle, middle + 12) < bluntField->at(middle, middle + 12));
    REQUIRE(sharpField->at(middle, middle) == Approx(bluntField->at(middle, middle)).margin(15.0));
}

TEST_CASE("detail roughens the slopes without lifting the summit",
          "[nodes][mountain]") {
    Graph graph = squareGraph(resolution, worldSize);
    terrain::GraphNode& rough = addPlainMountain(graph, "rough");
    rough.parameters["detailStrength"] = ParameterValue{1.0, ""};

    const auto field = evaluateNode(graph, "rough");
    REQUIRE(field->maximum() <= 1000.0f);
    REQUIRE_FALSE(isConstant(*field));
}

TEST_CASE("detail leaves the summit alone and bites hardest on the flanks",
          "[nodes][mountain]") {
    Graph graph = squareGraph(resolution, worldSize);
    addPlainMountain(graph, "plain");
    terrain::GraphNode& rough = addPlainMountain(graph, "rough");
    rough.parameters["detailStrength"] = ParameterValue{1.0, ""};

    const auto plainField = evaluateNode(graph, "plain");
    const auto roughField = evaluateNode(graph, "rough");
    const float atSummit = plainField->at(middle, middle) - roughField->at(middle, middle);
    const float onFlank = plainField->at(middle, middle + 10) - roughField->at(middle, middle + 10);
    REQUIRE(atSummit < 20.0f);
    REQUIRE(onFlank > atSummit);
}

TEST_CASE("detail carves no deeper than the spacing between its ridges allows",
          "[nodes][mountain]") {
    // A valley a few hundred metres wide cannot be a kilometre deep; the walls
    // would stand at an angle no rock holds, and a field of such walls reads
    // as spikes however the ridges themselves are shaped. So the carve follows
    // the detail size, and the same detail size cuts the same depth in metres
    // whether it is cut into a hill or into a giant.
    constexpr int fineResolution = 256;
    constexpr double detailSizeMeters = 600.0;
    constexpr double deepestAllowed = detailSizeMeters * 0.5;

    for (const double height : {1000.0, 8000.0}) {
        Graph graph = squareGraph(fineResolution, worldSize);
        terrain::GraphNode& plain = addPlainMountain(graph, "plain");
        plain.parameters["height"] = ParameterValue{height, ""};
        plain.parameters["featureSize"] = ParameterValue{detailSizeMeters, ""};
        terrain::GraphNode& rough = addPlainMountain(graph, "rough");
        rough.parameters["height"] = ParameterValue{height, ""};
        rough.parameters["featureSize"] = ParameterValue{detailSizeMeters, ""};
        rough.parameters["detailStrength"] = ParameterValue{1.0, ""};

        const auto plainField = evaluateNode(graph, "plain");
        const auto roughField = evaluateNode(graph, "rough");

        float deepestCut = 0.0f;
        for (int row = 0; row < fineResolution; ++row) {
            for (int column = 0; column < fineResolution; ++column) {
                deepestCut = std::max(deepestCut,
                                      plainField->at(column, row) - roughField->at(column, row));
            }
        }
        REQUIRE(deepestCut <= deepestAllowed + 1.0);
        REQUIRE(deepestCut > deepestAllowed * 0.5);
    }
}

TEST_CASE("drawing the detail out into spurs changes the slopes", "[nodes][mountain]") {
    Graph graph = squareGraph(resolution, worldSize);
    terrain::GraphNode& round = addPlainMountain(graph, "round");
    round.parameters["detailStrength"] = ParameterValue{1.0, ""};
    round.parameters["spurElongation"] = ParameterValue{0.0, ""};
    terrain::GraphNode& spurred = addPlainMountain(graph, "spurred");
    spurred.parameters["detailStrength"] = ParameterValue{1.0, ""};
    spurred.parameters["spurElongation"] = ParameterValue{0.9, ""};

    REQUIRE(hashField(*evaluateNode(graph, "round"))
            != hashField(*evaluateNode(graph, "spurred")));
}

TEST_CASE("a mountain draws the same shape at any thread count",
          "[nodes][mountain][determinism]") {
    Graph graph = squareGraph(resolution, worldSize);
    graph.addNode("peak", "Mountain");

    REQUIRE(hashField(*evaluateNode(graph, "peak", 1))
            == hashField(*evaluateNode(graph, "peak", 8)));
}

TEST_CASE("the relief a mountain draws follows the height it is given",
          "[nodes][mountain]") {
    // Ridge-to-valley relief is what the height parameter promises: doubling it
    // doubles the distance from the summit to the plane it stands on, and the
    // summit itself still reaches the height asked for.
    constexpr int fineResolution = 256;
    for (const double height : {800.0, 1600.0, 3200.0}) {
        Graph graph = squareGraph(fineResolution, worldSize);
        terrain::GraphNode& mountain = graph.addNode("peak", "Mountain");
        mountain.parameters["height"] = ParameterValue{height, ""};

        const auto field = evaluateNode(graph, "peak");
        const float relief = field->maximum() - field->minimum();
        REQUIRE(field->minimum() == Approx(0.0));
        REQUIRE(relief <= height);
        REQUIRE(relief > height * 0.9);
    }
}

TEST_CASE("scaling a mountain widens its footprint", "[nodes][mountain]") {
    Graph graph = squareGraph(resolution, worldSize);
    addPlainMountain(graph, "small");
    terrain::GraphNode& large = addPlainMountain(graph, "large");
    large.parameters["scale"] = ParameterValue{1.6, ""};

    const auto smallField = evaluateNode(graph, "small");
    const auto largeField = evaluateNode(graph, "large");
    // Eight cells past the small mountain's radius of 4000 m there is nothing
    // left of it, while the scaled one still stands.
    const int beyondFoot = middle + 4000 / static_cast<int>(worldSize / resolution) + 8;
    REQUIRE(smallField->at(middle, beyondFoot) == Approx(0.0));
    REQUIRE(largeField->at(middle, beyondFoot) > 50.0f);
}

TEST_CASE("a bulkier mountain stands further out from its summit",
          "[nodes][mountain]") {
    Graph graph = squareGraph(resolution, worldSize);
    terrain::GraphNode& slender = addPlainMountain(graph, "slender");
    slender.parameters["bulk"] = ParameterValue{0.0, ""};
    terrain::GraphNode& broad = addPlainMountain(graph, "broad");
    broad.parameters["bulk"] = ParameterValue{2.0, ""};

    const auto slenderField = evaluateNode(graph, "slender");
    const auto broadField = evaluateNode(graph, "broad");
    REQUIRE(broadField->at(middle, middle + 10) > slenderField->at(middle, middle + 10));
    REQUIRE(broadField->maximum() == Approx(slenderField->maximum()).margin(15.0));
}

TEST_CASE("reducing detail drops the finest ridges and keeps the mass",
          "[nodes][mountain]") {
    constexpr int fineResolution = 256;
    Graph graph = squareGraph(fineResolution, worldSize);
    terrain::GraphNode& detailed = graph.addNode("detailed", "Mountain");
    detailed.parameters["detailStrength"] = ParameterValue{1.0, ""};
    terrain::GraphNode& reduced = graph.addNode("reduced", "Mountain");
    reduced.parameters["detailStrength"] = ParameterValue{1.0, ""};
    reduced.parameters["reduceDetails"] = ParameterValue{1.0, ""};

    const auto detailedField = evaluateNode(graph, "detailed");
    const auto reducedField = evaluateNode(graph, "reduced");
    REQUIRE(meanNeighbourStep(*reducedField, fineResolution)
            < meanNeighbourStep(*detailedField, fineResolution) * 0.8f);
    REQUIRE(reducedField->maximum() == Approx(detailedField->maximum()).margin(30.0));
}

TEST_CASE("rolling a mountain's seed changes it", "[nodes][mountain]") {
    Graph graph = squareGraph(resolution, worldSize);
    graph.addNode("first", "Mountain");
    terrain::GraphNode& second = graph.addNode("second", "Mountain");
    second.parameters["seed"] = ParameterValue{7.0, ""};

    REQUIRE(hashField(*evaluateNode(graph, "first"))
            != hashField(*evaluateNode(graph, "second")));
}

/* -------------------------------------------------------- MountainRange -- */

TEST_CASE("a range stands along its bearing and nowhere else",
          "[nodes][mountainrange]") {
    Graph graph = squareGraph(resolution, worldSize);
    terrain::GraphNode& range = graph.addNode("range", "MountainRange");
    range.parameters["bearing"] = ParameterValue{0.0, ""};
    range.parameters["length"] = ParameterValue{9000.0, ""};
    range.parameters["width"] = ParameterValue{1500.0, ""};
    range.parameters["peakCount"] = ParameterValue{4.0, ""};
    range.parameters["height"] = ParameterValue{1500.0, ""};
    range.parameters["irregularity"] = ParameterValue{0.0, ""};
    range.parameters["detailStrength"] = ParameterValue{0.0, ""};

    const auto field = evaluateNode(graph, "range");
    // The spine runs north–south through the middle, so the east and west
    // edges of the world are well clear of it.
    REQUIRE(field->at(middle, 0) == Approx(0.0));
    REQUIRE(field->at(middle, resolution - 1) == Approx(0.0));
    REQUIRE(field->maximum() > 500.0f);
    REQUIRE(field->maximum() <= 1500.0f);
}

TEST_CASE("changing the peak count changes the range", "[nodes][mountainrange]") {
    Graph graph = squareGraph(resolution, worldSize);
    terrain::GraphNode& few = graph.addNode("few", "MountainRange");
    few.parameters["peakCount"] = ParameterValue{2.0, ""};
    terrain::GraphNode& many = graph.addNode("many", "MountainRange");
    many.parameters["peakCount"] = ParameterValue{12.0, ""};

    REQUIRE(hashField(*evaluateNode(graph, "few"))
            != hashField(*evaluateNode(graph, "many")));
}

TEST_CASE("drawing a range's detail out into ribs changes its flanks",
          "[nodes][mountainrange]") {
    Graph graph = squareGraph(resolution, worldSize);
    terrain::GraphNode& round = graph.addNode("round", "MountainRange");
    round.parameters["detailStrength"] = ParameterValue{1.0, ""};
    round.parameters["spurElongation"] = ParameterValue{0.0, ""};
    terrain::GraphNode& ribbed = graph.addNode("ribbed", "MountainRange");
    ribbed.parameters["detailStrength"] = ParameterValue{1.0, ""};
    ribbed.parameters["spurElongation"] = ParameterValue{0.9, ""};

    REQUIRE(hashField(*evaluateNode(graph, "round"))
            != hashField(*evaluateNode(graph, "ribbed")));
}

TEST_CASE("a range draws the same chain at any thread count",
          "[nodes][mountainrange][determinism]") {
    Graph graph = squareGraph(resolution, worldSize);
    graph.addNode("range", "MountainRange");

    REQUIRE(hashField(*evaluateNode(graph, "range", 1))
            == hashField(*evaluateNode(graph, "range", 8)));
}

TEST_CASE("valleys cut a range's flanks deep while its summit keeps its height",
          "[nodes][mountainrange]") {
    constexpr int fineResolution = 128;
    Graph graph = squareGraph(fineResolution, worldSize);
    addPlainRange(graph, "smooth");
    terrain::GraphNode& carved = addPlainRange(graph, "carved");
    carved.parameters["detailStrength"] = ParameterValue{1.0, ""};

    const auto smoothField = evaluateNode(graph, "smooth");
    const auto carvedField = evaluateNode(graph, "carved");

    // The valleys take more than half the mass somewhere on the flanks: relief
    // at full depth, not crumple laid over a dome.
    float deepestShare = 0.0f;
    for (int row = 0; row < fineResolution; ++row) {
        for (int column = 0; column < fineResolution; ++column) {
            const float mass = smoothField->at(column, row);
            if (mass < 500.0f) {
                continue;
            }
            deepestShare = std::max(deepestShare, 1.0f - carvedField->at(column, row) / mass);
        }
    }
    REQUIRE(deepestShare > 0.5f);
    REQUIRE(carvedField->maximum() > smoothField->maximum() * 0.9f);
    REQUIRE(carvedField->maximum() <= smoothField->maximum());
}

TEST_CASE("most of a range's flank is valley and the ridge lines are the exception",
          "[nodes][mountainrange]") {
    // Ridge lines are thin; the ground between them is where the valleys are
    // cut. If the median cell kept most of its mass the field would be the
    // other way up: canyons along the ridge lines and a plateau between them.
    constexpr int fineResolution = 128;
    Graph graph = squareGraph(fineResolution, worldSize);
    addPlainRange(graph, "smooth");
    terrain::GraphNode& carved = addPlainRange(graph, "carved");
    carved.parameters["detailStrength"] = ParameterValue{1.0, ""};

    const auto smoothField = evaluateNode(graph, "smooth");
    const auto carvedField = evaluateNode(graph, "carved");

    std::vector<float> keptShares;
    for (int row = 0; row < fineResolution; ++row) {
        for (int column = 0; column < fineResolution; ++column) {
            const float mass = smoothField->at(column, row);
            if (mass < 500.0f) {
                continue;
            }
            keptShares.push_back(carvedField->at(column, row) / mass);
        }
    }
    REQUIRE(keptShares.size() > 100);
    std::sort(keptShares.begin(), keptShares.end());
    REQUIRE(keptShares[keptShares.size() / 2] < 0.6f);
}

TEST_CASE("a range's ridges run out across its flanks rather than only along its spine",
          "[nodes][mountainrange]") {
    // Walking along the range half-way down one flank crosses transverse
    // ridges and the valleys between them, so the height there rises and falls
    // many times. On a plain wall it would only follow the crest.
    constexpr int fineResolution = 128;
    Graph graph = squareGraph(fineResolution, worldSize);
    terrain::GraphNode& range = graph.addNode("range", "MountainRange");
    range.parameters["bearing"] = ParameterValue{0.0, ""};
    range.parameters["length"] = ParameterValue{12000.0, ""};
    range.parameters["width"] = ParameterValue{8000.0, ""};
    range.parameters["peakCount"] = ParameterValue{4.0, ""};
    range.parameters["height"] = ParameterValue{2000.0, ""};
    range.parameters["heightVariation"] = ParameterValue{0.0, ""};
    range.parameters["saddleHeight"] = ParameterValue{1.0, ""};
    range.parameters["irregularity"] = ParameterValue{0.0, ""};
    range.parameters["detailStrength"] = ParameterValue{1.0, ""};

    const auto field = evaluateNode(graph, "range");
    const int flankColumn = fineResolution / 2 + fineResolution / 8;
    int turns = 0;
    bool rising = false;
    for (int row = fineResolution / 4 + 1; row < fineResolution * 3 / 4; ++row) {
        const bool nowRising = field->at(flankColumn, row) > field->at(flankColumn, row - 1);
        if (nowRising != rising) {
            ++turns;
        }
        rising = nowRising;
    }
    REQUIRE(turns >= 6);
}

TEST_CASE("nothing in a range lies below the altitude it sits on",
          "[nodes][mountainrange]") {
    Graph graph = squareGraph(resolution, worldSize);
    terrain::GraphNode& range = graph.addNode("range", "MountainRange");
    range.parameters["length"] = ParameterValue{9000.0, ""};
    range.parameters["width"] = ParameterValue{1500.0, ""};
    range.parameters["height"] = ParameterValue{1500.0, ""};
    range.parameters["baseAltitude"] = ParameterValue{100.0, ""};

    const auto field = evaluateNode(graph, "range");
    REQUIRE(field->minimum() == Approx(100.0));
    REQUIRE(field->maximum() <= 1600.0f);
}
