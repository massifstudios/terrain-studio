#pragma once

#include <algorithm>
#include <memory>
#include <string>

#include "terrain/evaluator.h"
#include "terrain/graph.h"
#include "terrain/hash.h"
#include "terrain/registry.h"

namespace terrain::testing {

/// A small square graph with the world size and cell size a node test wants to
/// reason about analytically.
inline Graph squareGraph(int resolution, double worldSizeMeters) {
    Graph graph;
    graph.settings.resolution = resolution;
    graph.settings.worldSizeMeters = worldSizeMeters;
    graph.settings.seed = 20260826;
    return graph;
}

inline NodeRegistry builtinRegistry() {
    NodeRegistry registry;
    registerBuiltinNodes(registry);
    return registry;
}

/// Evaluates one output of one node and returns its field, or nullptr on
/// failure.
inline std::shared_ptr<const Heightfield> evaluateOutput(const Graph& graph,
                                                         const std::string& nodeId, int outputPort,
                                                         int threadCount = 1) {
    const NodeRegistry registry = builtinRegistry();
    Evaluator evaluator(registry);
    EvaluationOptions options;
    options.threadCount = threadCount;
    return evaluator.evaluate(graph, nodeId, outputPort, options).field;
}

inline std::shared_ptr<const Heightfield> evaluateNode(const Graph& graph,
                                                       const std::string& nodeId,
                                                       int threadCount = 1) {
    return evaluateOutput(graph, nodeId, 0, threadCount);
}

inline std::shared_ptr<const ColorField> evaluateColor(const Graph& graph,
                                                       const std::string& nodeId,
                                                       int outputPort = 0,
                                                       int threadCount = 1) {
    const NodeRegistry registry = builtinRegistry();
    Evaluator evaluator(registry);
    EvaluationOptions options;
    options.threadCount = threadCount;
    return evaluator.evaluate(graph, nodeId, outputPort, options).color;
}

inline std::shared_ptr<const PointSet> evaluatePoints(const Graph& graph,
                                                      const std::string& nodeId,
                                                      int threadCount = 1) {
    const NodeRegistry registry = builtinRegistry();
    Evaluator evaluator(registry);
    EvaluationOptions options;
    options.threadCount = threadCount;
    return evaluator.evaluate(graph, nodeId, 0, options).points;
}

inline std::uint64_t hashField(const Heightfield& field) {
    Hasher hasher;
    for (const float sample : field.samples()) {
        hasher.combine(static_cast<double>(sample));
    }
    return hasher.digest();
}

inline bool allSamplesWithin(const Heightfield& field, float low, float high) {
    return std::all_of(field.samples().begin(), field.samples().end(),
                       [low, high](float sample) { return sample >= low && sample <= high; });
}

inline bool isConstant(const Heightfield& field) {
    return field.maximum() - field.minimum() < 1.0e-6f;
}

}  // namespace terrain::testing
