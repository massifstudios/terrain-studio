#include "terrain/graph.h"

#include <algorithm>
#include <set>
#include <unordered_map>
#include <unordered_set>

namespace terrain {
namespace {

/// Depth-first post-order. Returns false when a cycle is reached.
bool visit(const Graph& graph, const GraphNode& node, std::unordered_set<std::string>& onStack,
           std::unordered_set<std::string>& finished, std::vector<const GraphNode*>& order) {
    if (finished.contains(node.id)) {
        return true;
    }
    if (!onStack.insert(node.id).second) {
        return false;
    }

    for (const GraphEdge& edge : graph.edges()) {
        if (edge.toNodeId != node.id) {
            continue;
        }
        const GraphNode* upstream = graph.find(edge.fromNodeId);
        if (upstream != nullptr && !visit(graph, *upstream, onStack, finished, order)) {
            return false;
        }
    }

    onStack.erase(node.id);
    finished.insert(node.id);
    order.push_back(&node);
    return true;
}

void checkNodeTypes(const Graph& graph, const NodeRegistry& registry, GraphValidation& result) {
    std::set<std::string> seenIds;
    for (const GraphNode& node : graph.nodes()) {
        if (!seenIds.insert(node.id).second) {
            result.errors.push_back("Duplicate node id: " + node.id);
        }
        if (registry.find(node.typeName) == nullptr) {
            result.errors.push_back("Unknown node type '" + node.typeName + "' on node "
                                    + node.id);
        }
    }
}

const PortDescriptor* portAt(const std::vector<PortDescriptor>& ports, int index) {
    if (index < 0 || index >= static_cast<int>(ports.size())) {
        return nullptr;
    }
    return &ports[static_cast<std::size_t>(index)];
}

void checkEdge(const Graph& graph, const NodeRegistry& registry, const GraphEdge& edge,
               GraphValidation& result) {
    const GraphNode* source = graph.find(edge.fromNodeId);
    const GraphNode* target = graph.find(edge.toNodeId);
    if (source == nullptr || target == nullptr) {
        result.errors.push_back("Edge refers to a node that is not in the graph");
        return;
    }

    const NodeType* sourceType = registry.find(source->typeName);
    const NodeType* targetType = registry.find(target->typeName);
    if (sourceType == nullptr || targetType == nullptr) {
        return;
    }

    const PortDescriptor* outputPort = portAt(sourceType->outputs, edge.fromPort);
    const PortDescriptor* inputPort = portAt(targetType->inputs, edge.toPort);
    if (outputPort == nullptr || inputPort == nullptr) {
        result.errors.push_back("Edge " + source->id + " -> " + target->id
                                + " uses a port index that does not exist");
        return;
    }
    if (outputPort->type != inputPort->type) {
        result.errors.push_back("Edge " + source->id + "." + outputPort->name + " -> "
                                + target->id + "." + inputPort->name + " connects "
                                "incompatible port types");
    }
}

void checkInputs(const Graph& graph, const NodeRegistry& registry, GraphValidation& result) {
    for (const GraphNode& node : graph.nodes()) {
        const NodeType* type = registry.find(node.typeName);
        if (type == nullptr) {
            continue;
        }
        for (std::size_t port = 0; port < type->inputs.size(); ++port) {
            const bool connected = graph.incoming(node.id, static_cast<int>(port)) != nullptr;
            if (!connected && !type->inputs[port].isOptional) {
                result.errors.push_back("Node " + node.id + " has no input on required port '"
                                        + type->inputs[port].name + "'");
            }
        }
    }
}

void checkSingleEdgePerInput(const Graph& graph, GraphValidation& result) {
    std::set<std::pair<std::string, int>> occupied;
    for (const GraphEdge& edge : graph.edges()) {
        if (!occupied.insert({edge.toNodeId, edge.toPort}).second) {
            result.errors.push_back("Node " + edge.toNodeId
                                    + " has more than one edge into the same input port");
        }
    }
}

void checkAcyclic(const Graph& graph, GraphValidation& result) {
    std::unordered_set<std::string> onStack;
    std::unordered_set<std::string> finished;
    std::vector<const GraphNode*> order;
    for (const GraphNode& node : graph.nodes()) {
        if (!visit(graph, node, onStack, finished, order)) {
            result.errors.push_back("The graph contains a cycle through node " + node.id);
            return;
        }
    }
}

}  // namespace

double GraphSettings::cellSizeMeters() const {
    if (resolution <= 0) {
        return 1.0;
    }
    return worldSizeMeters / resolution;
}

GraphNode& Graph::addNode(std::string id, std::string typeName) {
    nodes_.push_back(GraphNode{std::move(id), std::move(typeName), {}});
    return nodes_.back();
}

void Graph::connect(std::string fromNodeId, int fromPort, std::string toNodeId, int toPort) {
    edges_.push_back(
        GraphEdge{std::move(fromNodeId), fromPort, std::move(toNodeId), toPort});
}

const GraphNode* Graph::find(std::string_view id) const {
    for (const GraphNode& node : nodes_) {
        if (node.id == id) {
            return &node;
        }
    }
    return nullptr;
}

const GraphEdge* Graph::incoming(std::string_view nodeId, int port) const {
    for (const GraphEdge& edge : edges_) {
        if (edge.toNodeId == nodeId && edge.toPort == port) {
            return &edge;
        }
    }
    return nullptr;
}

GraphValidation validateGraph(const Graph& graph, const NodeRegistry& registry) {
    GraphValidation result;
    checkNodeTypes(graph, registry, result);
    checkSingleEdgePerInput(graph, result);
    for (const GraphEdge& edge : graph.edges()) {
        checkEdge(graph, registry, edge, result);
    }
    checkInputs(graph, registry, result);
    checkAcyclic(graph, result);
    result.isValid = result.errors.empty();
    return result;
}

std::vector<const GraphNode*> dependencyOrder(const Graph& graph, std::string_view targetNodeId) {
    const GraphNode* target = graph.find(targetNodeId);
    if (target == nullptr) {
        return {};
    }

    std::unordered_set<std::string> onStack;
    std::unordered_set<std::string> finished;
    std::vector<const GraphNode*> order;
    if (!visit(graph, *target, onStack, finished, order)) {
        return {};
    }
    return order;
}

}  // namespace terrain
