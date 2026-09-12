#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <numbers>
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

/// A mountain with its bend and its fine relief at zero, which leaves the
/// mass and the ridge tree on their own and makes every height below
/// predictable.
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

/// The mean bend between neighbouring cells across the field: how much fine
/// crease the surface carries, which a smooth slope, however steep, does not
/// add to.
float meanNeighbourBend(const terrain::Heightfield& field, int size) {
    double total = 0.0;
    int bends = 0;
    for (int row = 0; row < size; ++row) {
        for (int column = 1; column + 1 < size; ++column) {
            total += std::abs(field.at(column + 1, row) - 2.0f * field.at(column, row)
                              + field.at(column - 1, row));
            ++bends;
        }
    }
    return static_cast<float>(total / bends);
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


/// Node ids seed the node, so two mountains meant to differ in one parameter
/// alone have to be built in separate graphs under the same id. Two nodes in
/// one graph are two different mountains, however their parameters are set.
std::shared_ptr<const terrain::Heightfield> mountainField(
    int gridResolution, const std::function<void(terrain::GraphNode&)>& adjust) {
    Graph graph = squareGraph(gridResolution, worldSize);
    terrain::GraphNode& mountain = graph.addNode("peak", "Mountain");
    adjust(mountain);
    return evaluateNode(graph, "peak");
}

/// The same, with the stochastic dials at zero so the skeleton stands alone.
std::shared_ptr<const terrain::Heightfield> plainMountainField(
    int gridResolution, const std::function<void(terrain::GraphNode&)>& adjust) {
    Graph graph = squareGraph(gridResolution, worldSize);
    terrain::GraphNode& mountain = addPlainMountain(graph, "peak");
    adjust(mountain);
    return evaluateNode(graph, "peak");
}

void leaveAsItIs(terrain::GraphNode&) {}

struct Cell {
    int column = 0;
    int row = 0;
};

Cell highestCell(const terrain::Heightfield& field, int size) {
    Cell highest;
    for (int row = 0; row < size; ++row) {
        for (int column = 0; column < size; ++column) {
            if (field.at(column, row) >= field.at(highest.column, highest.row)) {
                highest = Cell{column, row};
            }
        }
    }
    return highest;
}

/// The mean height on a ring about a cell, in metres.
float meanHeightOnRing(const terrain::Heightfield& field, int size, const Cell& centre,
                       double lowRadiusCells, double highRadiusCells) {
    double total = 0.0;
    int counted = 0;
    for (int row = 0; row < size; ++row) {
        for (int column = 0; column < size; ++column) {
            const double distance = std::hypot(column - centre.column, row - centre.row);
            if (distance < lowRadiusCells || distance > highRadiusCells) {
                continue;
            }
            total += field.at(column, row);
            ++counted;
        }
    }
    return static_cast<float>(total / std::max(counted, 1));
}

/// The spread of heights around a ring about a cell, in metres.
float heightSpreadOnRing(const terrain::Heightfield& field, int size, const Cell& centre,
                         double lowRadiusCells, double highRadiusCells) {
    float lowest = std::numeric_limits<float>::max();
    float highest = std::numeric_limits<float>::lowest();
    for (int row = 0; row < size; ++row) {
        for (int column = 0; column < size; ++column) {
            const double distance = std::hypot(column - centre.column, row - centre.row);
            if (distance < lowRadiusCells || distance > highRadiusCells) {
                continue;
            }
            lowest = std::min(lowest, field.at(column, row));
            highest = std::max(highest, field.at(column, row));
        }
    }
    return highest - lowest;
}

float heightOnRing(const terrain::Heightfield& field, int size, const Cell& centre,
                   double radiusCells, double angleRadians) {
    const int column = static_cast<int>(
        std::lround(centre.column + radiusCells * std::sin(angleRadians)));
    const int row =
        static_cast<int>(std::lround(centre.row - radiusCells * std::cos(angleRadians)));
    return field.at(std::clamp(column, 0, size - 1), std::clamp(row, 0, size - 1));
}

/**
 * How many separate stretches of a ring stand above the middle of the ring's
 * own relief: how many ridges the ring crosses.
 *
 * Counted as arcs rather than as local highs, so a cell of sampling noise on a
 * flank cannot be mistaken for a ridge of its own.
 */
int ridgeCrossingsOnRing(const terrain::Heightfield& field, int size, const Cell& centre,
                         double radiusCells) {
    constexpr int sampleCount = 180;
    std::vector<float> heights;
    heights.reserve(sampleCount);
    for (int sample = 0; sample < sampleCount; ++sample) {
        const double angle = 2.0 * std::numbers::pi * sample / sampleCount;
        heights.push_back(heightOnRing(field, size, centre, radiusCells, angle));
    }

    const auto [lowest, highest] = std::minmax_element(heights.begin(), heights.end());
    const float midHeight = (*lowest + *highest) * 0.5f;
    int crossings = 0;
    for (int sample = 0; sample < sampleCount; ++sample) {
        const bool rises = heights[(sample + sampleCount - 1) % sampleCount] <= midHeight
                           && heights[sample] > midHeight;
        if (rises) {
            ++crossings;
        }
    }
    return crossings;
}

/// The mean step between neighbouring cells across a band of distances from
/// a centre: how steep the ground is there, whichever way it faces.
float meanStepInBand(const terrain::Heightfield& field, int size, const Cell& centre,
                     double innerCells, double outerCells) {
    double total = 0.0;
    int steps = 0;
    for (int row = 0; row < size; ++row) {
        for (int column = 1; column < size; ++column) {
            const double distance = std::hypot(column - centre.column, row - centre.row);
            if (distance < innerCells || distance > outerCells) {
                continue;
            }
            total += std::abs(field.at(column, row) - field.at(column - 1, row));
            ++steps;
        }
    }
    return static_cast<float>(total / std::max(steps, 1));
}

}  // namespace

/* ------------------------------------------------------------- Mountain -- */

TEST_CASE("a mountain summits on its crest and reaches the ground at its radius",
          "[nodes][mountain]") {
    // The summit stands on a crest through the middle of the massif rather than
    // on the centre cell, so it is the distance from the centre that is
    // checked, not the height of one particular cell. The crest is a line,
    // so the grid reaches its height only as closely as a cell allows.
    constexpr int fineResolution = 256;
    const auto field = plainMountainField(fineResolution, leaveAsItIs);
    REQUIRE(field != nullptr);
    const Cell summit = highestCell(*field, fineResolution);
    const int centre = fineResolution / 2;
    const double fromCentre = std::hypot(summit.column - centre, summit.row - centre)
                              * worldSize / fineResolution;
    REQUIRE(fromCentre < 4000.0 * 0.4);
    REQUIRE(field->maximum() > 900.0f);
    REQUIRE(field->at(0, 0) == Approx(0.0));
    REQUIRE(allSamplesWithin(*field, 0.0f, 1000.0f));
}

TEST_CASE("a mountain sits on the altitude it is given", "[nodes][mountain]") {
    constexpr int fineResolution = 256;
    const auto field = plainMountainField(fineResolution, [](terrain::GraphNode& mountain) {
        mountain.parameters["baseAltitude"] = ParameterValue{250.0, ""};
    });
    REQUIRE(field->minimum() == Approx(250.0));
    // The summit is a point on a ridge line, so the grid reaches it only as
    // closely as a cell allows; nothing may stand above it.
    REQUIRE(field->maximum() <= 1250.0f);
    REQUIRE(field->maximum() > 1150.0f);
}

TEST_CASE("a sharper peak carries less mass on its flanks", "[nodes][mountain]") {
    const auto bluntField = plainMountainField(resolution, leaveAsItIs);
    const auto sharpField = plainMountainField(resolution, [](terrain::GraphNode& mountain) {
        mountain.parameters["peakSharpness"] = ParameterValue{3.0, ""};
    });

    const Cell summit = highestCell(*bluntField, resolution);
    const double mid = 4000.0 * 0.5 / (worldSize / resolution);
    REQUIRE(meanHeightOnRing(*sharpField, resolution, summit, mid - 1.0, mid + 1.0)
            < meanHeightOnRing(*bluntField, resolution, summit, mid - 1.0, mid + 1.0));
    REQUIRE(sharpField->maximum() == Approx(bluntField->maximum()).margin(15.0));
}

TEST_CASE("detail roughens the slopes without lifting the summit",
          "[nodes][mountain]") {
    const auto field = plainMountainField(resolution, [](terrain::GraphNode& mountain) {
        mountain.parameters["detailStrength"] = ParameterValue{1.0, ""};
    });
    REQUIRE(field->maximum() <= 1000.0f);
    REQUIRE_FALSE(isConstant(*field));
}

TEST_CASE("detail leaves the summit alone and bites hardest on the flanks",
          "[nodes][mountain]") {
    constexpr int fineResolution = 256;
    const auto plainField = plainMountainField(fineResolution, leaveAsItIs);
    const auto roughField = plainMountainField(fineResolution, [](terrain::GraphNode& mountain) {
        mountain.parameters["detailStrength"] = ParameterValue{1.0, ""};
    });

    const Cell summit = highestCell(*plainField, fineResolution);
    const float atSummit =
        plainField->at(summit.column, summit.row) - roughField->at(summit.column, summit.row);
    const double mid = 4000.0 * 0.5 / (worldSize / fineResolution);
    const float onFlank =
        meanHeightOnRing(*plainField, fineResolution, summit, mid - 1.0, mid + 1.0)
        - meanHeightOnRing(*roughField, fineResolution, summit, mid - 1.0, mid + 1.0);
    // The grid meets the crest within a cell rather than on it, so a little of
    // the carve reaches the summit cell; what matters is that the flanks lose
    // several times as much.
    REQUIRE(atSummit < 80.0f);
    REQUIRE(onFlank > atSummit * 3.0f);
}

TEST_CASE("drawing the detail out into gullies changes the slopes", "[nodes][mountain]") {
    // The fine relief is left out of any grid too coarse to hold it, so this
    // needs cells smaller than the coarsest flank cell.
    constexpr int fineResolution = 256;
    const auto roundField = plainMountainField(fineResolution, [](terrain::GraphNode& mountain) {
        mountain.parameters["detailStrength"] = ParameterValue{1.0, ""};
        mountain.parameters["spurElongation"] = ParameterValue{0.0, ""};
    });
    const auto spurredField = plainMountainField(fineResolution, [](terrain::GraphNode& mountain) {
        mountain.parameters["detailStrength"] = ParameterValue{1.0, ""};
        mountain.parameters["spurElongation"] = ParameterValue{0.9, ""};
    });
    REQUIRE(hashField(*roundField) != hashField(*spurredField));
}

TEST_CASE("a mountain falls away from its summit at once", "[nodes][mountain]") {
    // A mass drawn as a falloff from a point has no gradient at that point, so
    // it stands under a dome however much detail is laid over it. Hung off a
    // crest instead, the ground gives way as soon as it leaves the ridge.
    constexpr int fineResolution = 256;
    const auto field = mountainField(fineResolution, leaveAsItIs);
    const Cell summit = highestCell(*field, fineResolution);
    const double ring = 250.0 / (worldSize / fineResolution);
    const float fall = field->at(summit.column, summit.row)
                       - meanHeightOnRing(*field, fineResolution, summit, ring - 1.0, ring + 1.0);
    REQUIRE(fall > 1200.0f * 0.07f);
}

TEST_CASE("the flanks carry ridges and the valleys between them", "[nodes][mountain]") {
    // With the detail and the bend both at zero the skeleton is all there is,
    // so a ring around the massif crosses ridge and valley only if the mass is
    // hung off ridge lines rather than off the centre.
    constexpr int fineResolution = 256;
    const auto field = plainMountainField(fineResolution, leaveAsItIs);
    const Cell centre{fineResolution / 2, fineResolution / 2};
    const double ring = 2000.0 / (worldSize / fineResolution);
    REQUIRE(heightSpreadOnRing(*field, fineResolution, centre, ring - 1.0, ring + 1.0)
            > 1000.0f * 0.15f);
}

TEST_CASE("a mountain draws the same shape at any thread count",
          "[nodes][mountain][determinism]") {
    Graph graph = squareGraph(resolution, worldSize);
    graph.addNode("peak", "Mountain");

    const std::uint64_t once = hashField(*evaluateNode(graph, "peak", 1));
    REQUIRE(once == hashField(*evaluateNode(graph, "peak", 1)));
    REQUIRE(once == hashField(*evaluateNode(graph, "peak", 4)));
    REQUIRE(once == hashField(*evaluateNode(graph, "peak", 8)));
}

TEST_CASE("a mountain's flanks carry a tree of spurs, not a handful of arms",
          "[nodes][mountain]") {
    // A ring drawn round the massif crosses a ridge every time it leaves one
    // catchment for the next, so the count is how many spurs reach that far
    // out. A cone crosses none, a starfish a few; a mountain carries a spur
    // and a valley every few hundred metres all the way round.
    constexpr int fineResolution = 256;
    const auto field = plainMountainField(fineResolution, leaveAsItIs);
    const Cell centre{fineResolution / 2, fineResolution / 2};
    const double cellSizeMeters = worldSize / fineResolution;

    REQUIRE(ridgeCrossingsOnRing(*field, fineResolution, centre, 1600.0 / cellSizeMeters) >= 6);
    REQUIRE(ridgeCrossingsOnRing(*field, fineResolution, centre, 2600.0 / cellSizeMeters) >= 6);
}

TEST_CASE("the relief of a mountain scales with its height", "[nodes][mountain]") {
    // The valleys between the spurs are a share of the mass standing over
    // them, so a mountain twice as tall carries valleys twice as deep, and
    // the relief dies away with the mass into the apron at the foot.
    constexpr int fineResolution = 256;
    const auto lowField = plainMountainField(fineResolution, leaveAsItIs);
    const auto tallField = plainMountainField(fineResolution, [](terrain::GraphNode& mountain) {
        mountain.parameters["height"] = ParameterValue{2000.0, ""};
    });
    const Cell centre{fineResolution / 2, fineResolution / 2};
    const double ring = 2000.0 / (worldSize / fineResolution);
    const float lowSpread = heightSpreadOnRing(*lowField, fineResolution, centre, ring - 1.0, ring + 1.0);
    const float tallSpread =
        heightSpreadOnRing(*tallField, fineResolution, centre, ring - 1.0, ring + 1.0);
    REQUIRE(tallSpread == Approx(lowSpread * 2.0f).epsilon(0.02));
}

TEST_CASE("the flank relief eases into an apron at the foot", "[nodes][mountain]") {
    // Real mountains do not end in a cliff line of spurs: the mass goes
    // tangent to the plane, the valleys shallow out with it, and the last
    // stretch to the foot is gentle ground.
    constexpr int fineResolution = 256;
    const auto field = plainMountainField(fineResolution, [](terrain::GraphNode& mountain) {
        mountain.parameters["peakSharpness"] = ParameterValue{2.0, ""};
    });
    const Cell centre{fineResolution / 2, fineResolution / 2};
    const double cellSizeMeters = worldSize / fineResolution;
    const float midSlope = meanStepInBand(*field, fineResolution, centre,
                                          1500.0 / cellSizeMeters, 2500.0 / cellSizeMeters);
    const float footSlope = meanStepInBand(*field, fineResolution, centre,
                                           3400.0 / cellSizeMeters, 3900.0 / cellSizeMeters);
    CAPTURE(midSlope, footSlope);
    REQUIRE(footSlope < midSlope * 0.5f);
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
    const auto smallField = plainMountainField(resolution, leaveAsItIs);
    const auto largeField = plainMountainField(resolution, [](terrain::GraphNode& mountain) {
        mountain.parameters["scale"] = ParameterValue{1.6, ""};
    });

    // Eight cells past the small mountain's radius of 4000 m there is nothing
    // left of it, while the scaled one still stands.
    const int beyondFoot = middle + 4000 / static_cast<int>(worldSize / resolution) + 8;
    REQUIRE(smallField->at(middle, beyondFoot) == Approx(0.0));
    REQUIRE(largeField->at(middle, beyondFoot) > 50.0f);
}

TEST_CASE("a bulkier mountain stands further out from its summit",
          "[nodes][mountain]") {
    const auto slenderField = plainMountainField(resolution, [](terrain::GraphNode& mountain) {
        mountain.parameters["bulk"] = ParameterValue{0.0, ""};
    });
    const auto broadField = plainMountainField(resolution, [](terrain::GraphNode& mountain) {
        mountain.parameters["bulk"] = ParameterValue{2.0, ""};
    });

    const Cell summit = highestCell(*broadField, resolution);
    const double mid = 4000.0 * 0.5 / (worldSize / resolution);
    REQUIRE(meanHeightOnRing(*broadField, resolution, summit, mid - 1.0, mid + 1.0)
            > meanHeightOnRing(*slenderField, resolution, summit, mid - 1.0, mid + 1.0));
    REQUIRE(broadField->maximum() == Approx(slenderField->maximum()).margin(15.0));
}

TEST_CASE("reducing detail drops the finest ridges and keeps the mass",
          "[nodes][mountain]") {
    constexpr int fineResolution = 512;
    const auto detailedField = mountainField(fineResolution, [](terrain::GraphNode& mountain) {
        mountain.parameters["detailStrength"] = ParameterValue{1.0, ""};
    });
    const auto reducedField = mountainField(fineResolution, [](terrain::GraphNode& mountain) {
        mountain.parameters["detailStrength"] = ParameterValue{1.0, ""};
        mountain.parameters["reduceDetails"] = ParameterValue{1.0, ""};
    });
    const float reducedBend = meanNeighbourBend(*reducedField, fineResolution);
    const float detailedBend = meanNeighbourBend(*detailedField, fineResolution);
    // The ridge tree carries most of the crease; holding back the finer flank
    // cells takes a clear share of what is left and no more.
    CAPTURE(reducedBend, detailedBend);
    REQUIRE(reducedBend < detailedBend * 0.9f);
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
