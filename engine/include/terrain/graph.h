#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "terrain/heightfield.h"
#include "terrain/registry.h"

namespace terrain {

/// A parameter value as edited. Which half is meaningful comes from the
/// descriptor: numeric types read `number`, path and text types read `text`.
struct ParameterValue {
    double number = 0.0;
    std::string text;
};

struct GraphNode {
    std::string id;
    std::string typeName;
    std::map<std::string, ParameterValue, std::less<>> parameters;
};

struct GraphEdge {
    std::string fromNodeId;
    int fromPort = 0;
    std::string toNodeId;
    int toPort = 0;
};

/// Project-level settings every node evaluates against.
struct GraphSettings {
    int resolution = 1024;
    double worldSizeMeters = 10000.0;
    std::uint64_t seed = 0;
    GeoReference georeference;

    double cellSizeMeters() const;
};

/**
 * A directed acyclic graph of node instances. Cycles are a validation error;
 * there are no relaxation groups here.
 */
class Graph {
public:
    GraphSettings settings;

    /// Stored in a deque so the reference stays valid as more nodes arrive.
    GraphNode& addNode(std::string id, std::string typeName);
    void connect(std::string fromNodeId, int fromPort, std::string toNodeId, int toPort);

    const GraphNode* find(std::string_view id) const;
    const std::deque<GraphNode>& nodes() const { return nodes_; }
    const std::vector<GraphEdge>& edges() const { return edges_; }

    /// The edge feeding one input port, or nullptr when it is unconnected.
    const GraphEdge* incoming(std::string_view nodeId, int port) const;

private:
    std::deque<GraphNode> nodes_;
    std::vector<GraphEdge> edges_;
};

struct GraphValidation {
    bool isValid = true;
    std::vector<std::string> errors;
};

GraphValidation validateGraph(const Graph& graph, const NodeRegistry& registry);

/// Every node the target depends on, in dependency order, target last.
/// Empty when the target is missing or the graph has a cycle through it.
std::vector<const GraphNode*> dependencyOrder(const Graph& graph, std::string_view targetNodeId);

}  // namespace terrain
