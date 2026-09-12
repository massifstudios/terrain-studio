#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <numbers>
#include <set>
#include <string>
#include <vector>

#include "node_test_support.h"

using Catch::Approx;
using terrain::Graph;
using terrain::Heightfield;
using terrain::ParameterValue;
using terrain::PointSet;
using terrain::testing::allSamplesWithin;
using terrain::testing::builtinRegistry;
using terrain::testing::evaluateNode;
using terrain::testing::evaluateOutput;
using terrain::testing::hashField;
using terrain::testing::squareGraph;

namespace {

constexpr int resolution = 96;
constexpr double worldSize = 9600.0;
constexpr double cellSize = worldSize / resolution;

/// Rough ground for rocks to roll over, made without touching the disk.
Graph ridgedGround(double amplitudeMeters) {
    Graph graph = squareGraph(resolution, worldSize);
    terrain::GraphNode& shape = graph.addNode("shape", "RidgedNoise");
    shape.parameters["featureSize"] = ParameterValue{3000.0, ""};
    shape.parameters["octaves"] = ParameterValue{5.0, ""};
    shape.parameters["amplitude"] = ParameterValue{amplitudeMeters, ""};
    return graph;
}

/// A cone standing on a plain: one steep face all the way round, and a hard
/// break in slope at its foot where anything rolling off it must stop.
Graph coneOnAPlain(double amplitudeMeters, double radiusMeters) {
    Graph graph = squareGraph(64, 6400.0);
    terrain::GraphNode& cone = graph.addNode("shape", "RadialGradient");
    cone.parameters["centreEast"] = ParameterValue{0.5, ""};
    cone.parameters["centreNorth"] = ParameterValue{0.5, ""};
    cone.parameters["radius"] = ParameterValue{radiusMeters, ""};
    cone.parameters["profile"] = ParameterValue{0.0, ""};
    cone.parameters["amplitude"] = ParameterValue{amplitudeMeters, ""};
    cone.parameters["offset"] = ParameterValue{0.0, ""};
    return graph;
}

/// The points on a named output port. `evaluatePoints` only reaches port 0,
/// and Debris reports its rocks on port 2.
std::shared_ptr<const PointSet> pointsOnPort(const Graph& graph, const std::string& nodeId,
                                             int outputPort, int threadCount = 1) {
    const terrain::NodeRegistry registry = builtinRegistry();
    terrain::Evaluator evaluator(registry);
    terrain::EvaluationOptions options;
    options.threadCount = threadCount;
    return evaluator.evaluate(graph, nodeId, outputPort, options).points;
}

double totalHeight(const Heightfield& field) {
    double total = 0.0;
    for (const float sample : field.samples()) {
        total += sample;
    }
    return total;
}

/// The ground slope at one cell, by the same central differences the engine
/// reads, so a test about a rest angle asks about the angle the node saw.
double slopeDegreesAt(const Heightfield& field, int row, int column) {
    const int size = field.width();
    const int north = std::max(row - 1, 0);
    const int south = std::min(row + 1, size - 1);
    const int west = std::max(column - 1, 0);
    const int east = std::min(column + 1, size - 1);
    const double eastward = (field.at(row, east) - field.at(row, west))
                            / ((east - west) * field.cellSizeMeters());
    const double northward = (field.at(north, column) - field.at(south, column))
                             / ((south - north) * field.cellSizeMeters());
    return std::atan(std::hypot(eastward, northward)) * 180.0 / std::numbers::pi;
}

int rowOfNorthing(double northingMeters, double worldSizeMeters, double cellSizeMeters) {
    return static_cast<int>((worldSizeMeters - northingMeters) / cellSizeMeters);
}

int columnOfEasting(double eastingMeters, double cellSizeMeters) {
    return static_cast<int>(eastingMeters / cellSizeMeters);
}

Graph debrisOverGround(double friction, int rockCount, double seed) {
    Graph graph = ridgedGround(1400.0);
    terrain::GraphNode& rocks = graph.addNode("rocks", "Debris");
    rocks.parameters["debrisAmount"] = ParameterValue{static_cast<double>(rockCount), ""};
    rocks.parameters["friction"] = ParameterValue{friction, ""};
    rocks.parameters["sizeMin"] = ParameterValue{40.0, ""};
    rocks.parameters["sizeMax"] = ParameterValue{160.0, ""};
    rocks.parameters["seed"] = ParameterValue{seed, ""};
    graph.connect("shape", 0, "rocks", 0);
    return graph;
}

Graph screeOnACone(double edge, double sourceAngle) {
    Graph graph = coneOnAPlain(2400.0, 2500.0);
    terrain::GraphNode& scree = graph.addNode("scree", "Scree");
    scree.parameters["scale"] = ParameterValue{120.0, ""};
    scree.parameters["height"] = ParameterValue{20.0, ""};
    scree.parameters["density"] = ParameterValue{2000.0, ""};
    scree.parameters["spread"] = ParameterValue{3000.0, ""};
    scree.parameters["edge"] = ParameterValue{edge, ""};
    scree.parameters["sourceAngle"] = ParameterValue{sourceAngle, ""};
    graph.connect("shape", 0, "scree", 0);
    return graph;
}

/// The mean of a mask over the ring of ground between two radii of the cone.
double meanInRing(const Heightfield& mask, double innerMeters, double outerMeters) {
    const int size = mask.width();
    const double centre = size * mask.cellSizeMeters() * 0.5;
    double total = 0.0;
    int cells = 0;
    for (int row = 0; row < size; ++row) {
        for (int column = 0; column < size; ++column) {
            const double east = (column + 0.5) * mask.cellSizeMeters() - centre;
            const double north = (row + 0.5) * mask.cellSizeMeters() - centre;
            const double distance = std::hypot(east, north);
            if (distance < innerMeters || distance > outerMeters) {
                continue;
            }
            total += mask.at(row, column);
            cells += 1;
        }
    }
    REQUIRE(cells > 0);
    return total / cells;
}

Graph sedimentsOverGround(double depth, int passes, double grainy) {
    Graph graph = ridgedGround(1200.0);
    terrain::GraphNode& layer = graph.addNode("layer", "Sediments");
    layer.parameters["depth"] = ParameterValue{depth, ""};
    layer.parameters["passes"] = ParameterValue{static_cast<double>(passes), ""};
    layer.parameters["travel"] = ParameterValue{630.0, ""};
    layer.parameters["grainyDeposits"] = ParameterValue{grainy, ""};
    graph.connect("shape", 0, "layer", 0);
    return graph;
}

}  // namespace

/* ================================================================ Debris == */

TEST_CASE("rocks come to rest on ground gentle enough to hold them", "[debris]") {
    const double friction = 0.7;
    const double restAngle = friction * 45.0;
    const Graph graph = debrisOverGround(friction, 3000, 0.0);
    const auto ground = evaluateNode(graph, "shape");
    const auto rocks = pointsOnPort(graph, "rocks", 2);
    REQUIRE(rocks != nullptr);
    REQUIRE(rocks->size() > 500);

    int steepRests = 0;
    for (const TerrainPoint& rock : rocks->points()) {
        const int row = rowOfNorthing(rock.northingMeters, worldSize, cellSize);
        const int column = columnOfEasting(rock.eastingMeters, cellSize);
        if (slopeDegreesAt(*ground, row, column) > restAngle + 1.0) {
            steepRests += 1;
        }
    }

    // The handful left over are rocks that ran into a pit or off the edge of
    // the world, which is the only thing a fixed step budget may do with one.
    REQUIRE(steepRests * 20 < static_cast<int>(rocks->size()));
}

TEST_CASE("a rock never ends up higher than the ground it came off", "[debris]") {
    const Graph graph = debrisOverGround(0.3, 2000, 0.0);
    const auto ground = evaluateNode(graph, "shape");
    const auto rocks = pointsOnPort(graph, "rocks", 2);

    for (const TerrainPoint& rock : rocks->points()) {
        const int row = rowOfNorthing(rock.northingMeters, worldSize, cellSize);
        const int column = columnOfEasting(rock.eastingMeters, cellSize);
        REQUIRE(rock.altitudeMeters == Approx(ground->at(row, column)));
        REQUIRE(rock.altitudeMeters <= ground->maximum());
    }
}

TEST_CASE("every rock has an id of its own and stands inside the world", "[debris]") {
    const Graph graph = debrisOverGround(0.5, 2000, 0.0);
    const auto rocks = pointsOnPort(graph, "rocks", 2);
    REQUIRE(rocks->size() > 500);

    std::set<std::uint64_t> ids;
    for (const TerrainPoint& rock : rocks->points()) {
        ids.insert(rock.id);
        REQUIRE(rock.eastingMeters >= 0.0f);
        REQUIRE(rock.eastingMeters <= worldSize);
        REQUIRE(rock.northingMeters >= 0.0f);
        REQUIRE(rock.northingMeters <= worldSize);
        REQUIRE(rock.rotationDegrees >= 0.0f);
        REQUIRE(rock.rotationDegrees <= 360.0f);
        REQUIRE(rock.scale >= 40.0f);
        REQUIRE(rock.scale <= 160.0f);
    }
    REQUIRE(ids.size() == rocks->size());
}

TEST_CASE("resting rocks are stamped into the surface and reported as a mask", "[debris]") {
    const Graph graph = debrisOverGround(0.5, 2000, 0.0);
    const auto ground = evaluateNode(graph, "shape");
    const auto stamped = evaluateNode(graph, "rocks");
    const auto mask = evaluateOutput(graph, "rocks", 1);

    REQUIRE(totalHeight(*stamped) > totalHeight(*ground));
    REQUIRE(mask->semantics() == terrain::FieldSemantics::Mask);
    REQUIRE(allSamplesWithin(*mask, 0.0f, 1.0f));
    REQUIRE(mask->maximum() == Approx(1.0f));
}

TEST_CASE("turning the still rocks off leaves the surface untouched", "[debris]") {
    Graph graph = debrisOverGround(0.5, 2000, 0.0);
    graph.find("rocks")->parameters["renderStillRocks"] = ParameterValue{0.0, ""};

    const auto ground = evaluateNode(graph, "shape");
    const auto out = evaluateNode(graph, "rocks");
    const auto mask = evaluateOutput(graph, "rocks", 1);

    REQUIRE(hashField(*out) == hashField(*ground));
    REQUIRE(mask->maximum() > 0.0f);
}

TEST_CASE("debris gives the same surface and the same rocks at one thread and at eight",
          "[debris][determinism]") {
    const Graph graph = debrisOverGround(0.5, 3000, 0.0);
    const auto single = evaluateNode(graph, "rocks", 1);
    const auto many = evaluateNode(graph, "rocks", 8);
    REQUIRE(hashField(*single) == hashField(*many));

    const auto singleRocks = pointsOnPort(graph, "rocks", 2, 1);
    const auto manyRocks = pointsOnPort(graph, "rocks", 2, 8);
    REQUIRE(singleRocks->size() == manyRocks->size());
    for (std::size_t index = 0; index < singleRocks->size(); ++index) {
        REQUIRE(singleRocks->points()[index].id == manyRocks->points()[index].id);
        REQUIRE(singleRocks->points()[index].eastingMeters
                == manyRocks->points()[index].eastingMeters);
        REQUIRE(singleRocks->points()[index].scale == manyRocks->points()[index].scale);
    }
}

TEST_CASE("a different debris seed sheds different rocks", "[debris][determinism]") {
    const auto first = pointsOnPort(debrisOverGround(0.5, 2000, 0.0), "rocks", 2);
    const auto second = pointsOnPort(debrisOverGround(0.5, 2000, 77.0), "rocks", 2);

    REQUIRE(first->size() > 0);
    REQUIRE(second->size() > 0);
    REQUIRE(first->points()[0].id != second->points()[0].id);
}

TEST_CASE("the natural distribution sheds rock off the steep ground", "[debris]") {
    Graph graph = debrisOverGround(0.9, 3000, 0.0);
    graph.find("rocks")->parameters["distribution"] = ParameterValue{1.0, ""};
    graph.find("rocks")->parameters["friction"] = ParameterValue{1.0, ""};

    const auto ground = evaluateNode(graph, "shape");
    const auto rocks = pointsOnPort(graph, "rocks", 2);
    REQUIRE(rocks->size() > 100);

    double restingAltitude = 0.0;
    for (const TerrainPoint& rock : rocks->points()) {
        restingAltitude += rock.altitudeMeters;
    }
    restingAltitude /= rocks->size();

    // Friction of 1 holds a rock wherever it was shed, so where the rocks lie
    // is where the distribution put them: up on the steep high ground.
    const double middle = (ground->minimum() + ground->maximum()) * 0.5;
    REQUIRE(restingAltitude > middle);
}

/* ================================================================= Scree == */

TEST_CASE("scree gathers at the foot of a steep face rather than on it", "[debris][scree]") {
    const auto mask = evaluateOutput(screeOnACone(1.0, 30.0), "scree", 1);
    REQUIRE(mask->semantics() == terrain::FieldSemantics::Mask);
    REQUIRE(allSamplesWithin(*mask, 0.0f, 1.0f));

    // The cone is one unbroken face out to 2500 m, so the break in slope is
    // the only place a stone that keeps going can stop.
    const double onTheFace = meanInRing(*mask, 700.0, 2200.0);
    const double atTheFoot = meanInRing(*mask, 2300.0, 2700.0);
    REQUIRE(atTheFoot > onTheFace * 3.0);
}

TEST_CASE("no edge gathering leaves the stones where they broke off", "[debris][scree]") {
    const auto mask = evaluateOutput(screeOnACone(0.0, 30.0), "scree", 1);

    const double onTheFace = meanInRing(*mask, 700.0, 2200.0);
    const double atTheFoot = meanInRing(*mask, 2300.0, 2700.0);
    REQUIRE(onTheFace > atTheFoot);
}

TEST_CASE("ground gentler than the source angle sheds nothing", "[debris][scree]") {
    const Graph graph = screeOnACone(1.0, 80.0);
    const auto ground = evaluateNode(graph, "shape");
    const auto out = evaluateNode(graph, "scree");

    REQUIRE(hashField(*out) == hashField(*ground));
}

TEST_CASE("scree gives the same surface at one thread and at eight",
          "[debris][scree][determinism]") {
    const Graph graph = screeOnACone(0.8, 30.0);
    const auto single = evaluateNode(graph, "scree", 1);
    const auto many = evaluateNode(graph, "scree", 8);

    REQUIRE(hashField(*single) == hashField(*many));
}

TEST_CASE("a different scree seed lays the stones differently", "[debris][scree][determinism]") {
    Graph graph = screeOnACone(0.8, 30.0);
    const auto first = evaluateNode(graph, "scree");
    graph.find("scree")->parameters["seed"] = ParameterValue{42.0, ""};
    const auto second = evaluateNode(graph, "scree");

    REQUIRE(hashField(*first) != hashField(*second));
}

/* ============================================================= Sediments == */

TEST_CASE("sediments lays down exactly the depth it was given", "[debris][sediments]") {
    const Graph graph = sedimentsOverGround(40.0, 20, 0.0);
    const auto before = evaluateNode(graph, "shape");
    const auto after = evaluateNode(graph, "layer");

    REQUIRE(totalHeight(*after) - totalHeight(*before)
            == Approx(40.0 * before->sampleCount()).epsilon(1.0e-3));
}

TEST_CASE("a grainy layer weighs the same as a flat one", "[debris][sediments]") {
    const Graph graph = sedimentsOverGround(40.0, 20, 1.0);
    const auto before = evaluateNode(graph, "shape");
    const auto after = evaluateNode(graph, "layer");

    REQUIRE(totalHeight(*after) - totalHeight(*before)
            == Approx(40.0 * before->sampleCount()).epsilon(1.0e-3));
    REQUIRE(hashField(*after) != hashField(*evaluateNode(sedimentsOverGround(40.0, 20, 0.0),
                                                         "layer")));
}

TEST_CASE("dropping the layer all at once lays down the same depth", "[debris][sediments]") {
    Graph graph = sedimentsOverGround(40.0, 20, 0.0);
    graph.find("layer")->parameters["style"] = ParameterValue{1.0, ""};

    const auto before = evaluateNode(graph, "shape");
    const auto after = evaluateNode(graph, "layer");
    REQUIRE(totalHeight(*after) - totalHeight(*before)
            == Approx(40.0 * before->sampleCount()).epsilon(1.0e-3));
}

TEST_CASE("sediment settles into the low ground", "[debris][sediments]") {
    const Graph graph = sedimentsOverGround(60.0, 20, 0.0);
    const auto before = evaluateNode(graph, "shape");
    const auto after = evaluateNode(graph, "layer");
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

    REQUIRE(lowGround / lowCells > highGround / highCells);
}

TEST_CASE("fine sediment travels further than bulky", "[debris][sediments]") {
    Graph fine = sedimentsOverGround(60.0, 20, 0.0);
    fine.find("layer")->parameters["type"] = ParameterValue{0.0, ""};
    Graph bulky = sedimentsOverGround(60.0, 20, 0.0);
    bulky.find("layer")->parameters["type"] = ParameterValue{1.0, ""};

    const auto before = evaluateNode(fine, "shape");
    const auto fineLayer = evaluateNode(fine, "layer");
    const auto bulkyLayer = evaluateNode(bulky, "layer");
    const float middle = (before->minimum() + before->maximum()) * 0.5f;

    double fineInValleys = 0.0;
    double bulkyInValleys = 0.0;
    for (std::size_t index = 0; index < before->sampleCount(); ++index) {
        if (before->data()[index] >= middle) {
            continue;
        }
        fineInValleys += fineLayer->data()[index] - before->data()[index];
        bulkyInValleys += bulkyLayer->data()[index] - before->data()[index];
    }

    // Fine material rests at a gentler angle, so more of it ends up off the
    // slopes and down in the valleys.
    REQUIRE(fineInValleys > bulkyInValleys);
}

TEST_CASE("the sediment depth comes out as a mask", "[debris][sediments]") {
    const Graph graph = sedimentsOverGround(40.0, 20, 0.0);
    const auto depth = evaluateOutput(graph, "layer", 1);

    REQUIRE(depth->semantics() == terrain::FieldSemantics::Mask);
    REQUIRE(allSamplesWithin(*depth, 0.0f, 1.0f));
    REQUIRE(depth->maximum() == Approx(1.0f));
}

TEST_CASE("sediments gives the same surface at one thread and at eight",
          "[debris][sediments][determinism]") {
    const Graph graph = sedimentsOverGround(40.0, 20, 1.0);
    const auto single = evaluateNode(graph, "layer", 1);
    const auto many = evaluateNode(graph, "layer", 8);

    REQUIRE(hashField(*single) == hashField(*many));
}

TEST_CASE("a different sediment seed grains the layer differently",
          "[debris][sediments][determinism]") {
    Graph graph = sedimentsOverGround(40.0, 20, 1.0);
    const auto first = evaluateNode(graph, "layer");
    graph.find("layer")->parameters["seed"] = ParameterValue{9.0, ""};
    const auto second = evaluateNode(graph, "layer");

    REQUIRE(hashField(*first) != hashField(*second));
}
