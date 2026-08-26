#pragma once

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "terrain/color_field.h"
#include "terrain/graph.h"
#include "terrain/heightfield.h"
#include "terrain/point_set.h"
#include "terrain/registry.h"
#include "terrain/thumbnail.h"

namespace terrain {

/// What one output port produced. Exactly one member is set, and which one is
/// decided by the port's declared type, never by inspection.
struct NodeOutput {
    std::shared_ptr<Heightfield> field;
    std::shared_ptr<ColorField> color;
    std::shared_ptr<PointSet> points;
};

class CancellationToken {
public:
    void cancel() { cancelled_.store(true, std::memory_order_relaxed); }
    bool isCancelled() const { return cancelled_.load(std::memory_order_relaxed); }

private:
    std::atomic<bool> cancelled_{false};
};

struct ProgressUpdate {
    std::string nodeId;
    std::string typeName;
    float fraction = 0.0f;
};

using ProgressCallback = std::function<void(const ProgressUpdate&)>;

struct EvaluationOptions {
    /// 0 means "as many as the machine has". Explicit counts exist so the
    /// determinism suite can compare one thread against many.
    int threadCount = 0;
    /// Only a full-resolution build may write files.
    bool isFinalBuild = false;
    /// When positive, every node in the evaluated subgraph also returns a
    /// square thumbnail of its first output, for the node editor.
    int thumbnailResolution = 0;
    ProgressCallback onProgress;
    CancellationToken* cancellation = nullptr;
};

/// Reported to the UI, never fed back into evaluation.
struct NodeTiming {
    std::string nodeId;
    double milliseconds = 0.0;
    bool servedFromCache = false;
};

/// A node's first output, shaded down to a square for the graph editor.
struct NodeThumbnail {
    std::string nodeId;
    ThumbnailImage image;
};

struct EvaluationOutcome {
    /// The requested output. Which of the three is set follows the port type.
    std::shared_ptr<const Heightfield> field;
    std::shared_ptr<const ColorField> color;
    std::shared_ptr<const PointSet> points;
    /// Nodes that actually ran. A clean cache leaves this empty.
    std::vector<std::string> evaluatedNodeIds;
    std::vector<NodeTiming> timings;
    std::vector<NodeThumbnail> thumbnails;
    std::vector<std::string> errors;
    bool wasCancelled = false;

    bool hasResult() const { return field != nullptr || color != nullptr || points != nullptr; }
    bool succeeded() const { return hasResult() && errors.empty() && !wasCancelled; }
};

/**
 * Evaluates a graph node and everything it depends on.
 *
 * Results are cached by a hash of the node type, its version, its parameters,
 * the hashes of its inputs, the resolution and the graph seed — so editing one
 * parameter re-runs that node and its transitive dependents, and nothing else.
 */
class Evaluator {
public:
    explicit Evaluator(const NodeRegistry& registry);

    EvaluationOutcome evaluate(const Graph& graph, std::string_view nodeId, int outputPort,
                               const EvaluationOptions& options);

    void clearCache();
    std::size_t cachedResultCount() const { return cache_.size(); }

private:
    struct NodeResult {
        std::vector<NodeOutput> outputs;
    };

    static void collectThumbnails(
        const std::map<std::string, std::shared_ptr<NodeResult>>& results,
        int thumbnailResolution, double worldSizeMeters, EvaluationOutcome& outcome);

    const NodeRegistry* registry_;
    std::unordered_map<std::uint64_t, std::shared_ptr<NodeResult>> cache_;
};

}  // namespace terrain
