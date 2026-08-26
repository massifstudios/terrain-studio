#include "terrain/evaluator.h"

#include <algorithm>
#include <chrono>
#include <map>
#include <mutex>
#include <thread>

#include "evaluation_context.h"
#include "terrain/hash.h"
#include "terrain/resample.h"
#include "terrain/thumbnail.h"

namespace terrain {
namespace {

struct NodePlan {
    const GraphNode* node = nullptr;
    const NodeType* type = nullptr;
    std::uint64_t cacheKey = 0;
    int depth = 0;
};

ParameterValue resolveParameter(const GraphNode& node, const ParameterDescriptor& descriptor) {
    const auto found = node.parameters.find(descriptor.name);
    if (found != node.parameters.end()) {
        return found->second;
    }
    return ParameterValue{descriptor.defaultValue, descriptor.defaultText};
}

void hashParameters(Hasher& hasher, const GraphNode& node, const NodeType& type) {
    for (const ParameterDescriptor& descriptor : type.parameters) {
        const ParameterValue value = resolveParameter(node, descriptor);
        hasher.combine(descriptor.name);
        hasher.combine(value.number);
        hasher.combine(value.text);
    }
}

void hashSettings(Hasher& hasher, const GraphSettings& settings) {
    hasher.combine(static_cast<std::uint64_t>(settings.resolution));
    hasher.combine(settings.worldSizeMeters);
    hasher.combine(settings.seed);
    hasher.combine(settings.georeference.coordinateSystem);
    hasher.combine(settings.georeference.eastingMinMeters);
    hasher.combine(settings.georeference.northingMaxMeters);
}

/// Everything a node's result depends on, in one number: its type and version,
/// the settings, its own parameters, and the key of whatever feeds each input.
/// A build hashes differently from a preview producing identical numbers,
/// because a build is the evaluation allowed to write files — without that,
/// building straight after a preview would serve the exporters from cache and
/// put nothing on disk.
std::uint64_t nodeCacheKey(const Graph& graph, const GraphNode& node, const NodeType& type,
                           bool isFinalBuild,
                           const std::map<std::string, std::uint64_t>& upstreamKeys) {
    Hasher hasher;
    hasher.combine(type.typeName);
    hasher.combine(static_cast<std::uint64_t>(type.version));
    hasher.combine(static_cast<std::uint64_t>(isFinalBuild));
    hashSettings(hasher, graph.settings);
    hashParameters(hasher, node, type);
    for (std::size_t port = 0; port < type.inputs.size(); ++port) {
        const GraphEdge* edge = graph.incoming(node.id, static_cast<int>(port));
        if (edge == nullptr) {
            hasher.combine(std::uint64_t{0});
            continue;
        }
        hasher.combine(upstreamKeys.at(edge->fromNodeId));
        hasher.combine(static_cast<std::uint64_t>(edge->fromPort));
    }
    return hasher.digest();
}

/// 0 means "as many as the machine has". Nothing about the result may depend
/// on this number; it only decides how fast the same work happens.
int resolvedThreadCount(int requested) {
    if (requested > 0) {
        return requested;
    }
    const int detected = static_cast<int>(std::thread::hardware_concurrency());
    return std::max(detected, 1);
}

int workerCount(int requested, std::size_t taskCount) {
    return std::min<int>(resolvedThreadCount(requested),
                         static_cast<int>(std::max<std::size_t>(taskCount, 1)));
}

/// Threads left for one node's own lanes once the wave has been spread across
/// the pool, so a wave of eight nodes does not each spawn eight more threads.
int laneThreadBudget(int requested, std::size_t waveSize) {
    const int total = resolvedThreadCount(requested);
    return std::max<int>(total / static_cast<int>(std::max<std::size_t>(waveSize, 1)), 1);
}

/// Runs a batch of independent tasks and waits for all of them. Every task
/// writes only into its own slot, so the outcome does not depend on how the
/// batch was divided.
void runBatch(std::vector<std::function<void()>>& tasks, int threadCount) {
    if (tasks.empty()) {
        return;
    }
    const int workers = workerCount(threadCount, tasks.size());
    if (workers == 1) {
        for (auto& task : tasks) {
            task();
        }
        return;
    }

    std::atomic<std::size_t> nextTask{0};
    std::vector<std::jthread> threads;
    threads.reserve(static_cast<std::size_t>(workers));
    for (int worker = 0; worker < workers; ++worker) {
        threads.emplace_back([&tasks, &nextTask] {
            for (std::size_t index = nextTask.fetch_add(1); index < tasks.size();
                 index = nextTask.fetch_add(1)) {
                tasks[index]();
            }
        });
    }
}

/// Storage for one output port, of whichever kind the port declares.
NodeOutput makeOutput(const GraphSettings& settings, const PortDescriptor& port) {
    NodeOutput output;
    if (port.type == PortType::Color) {
        output.color = std::make_shared<ColorField>(settings.resolution, settings.resolution);
        return output;
    }
    if (port.type == PortType::Points) {
        output.points = std::make_shared<PointSet>();
        return output;
    }

    output.field = std::make_shared<Heightfield>(settings.resolution, settings.resolution);
    output.field->setCellSizeMeters(settings.cellSizeMeters());
    output.field->setGeoreference(settings.georeference);
    output.field->setSemantics(port.fieldSemantics);
    return output;
}

ThumbnailImage thumbnailOf(const NodeOutput& output, int size, double worldSizeMeters) {
    if (output.color != nullptr) {
        return renderThumbnail(*output.color, size);
    }
    if (output.points != nullptr) {
        return renderThumbnail(*output.points, size, worldSizeMeters);
    }
    return renderThumbnail(*output.field, size);
}

}  // namespace

/// One thumbnail per node in the evaluated subgraph, so the editor can show
/// the whole graph at a glance without a second evaluation.
void Evaluator::collectThumbnails(
    const std::map<std::string, std::shared_ptr<NodeResult>>& results, int thumbnailResolution,
    double worldSizeMeters, EvaluationOutcome& outcome) {
    if (thumbnailResolution <= 0) {
        return;
    }
    for (const auto& [nodeId, result] : results) {
        if (result == nullptr || result->outputs.empty()) {
            continue;
        }
        outcome.thumbnails.push_back(NodeThumbnail{
            nodeId,
            thumbnailOf(result->outputs.front(), thumbnailResolution, worldSizeMeters)});
    }
}

Evaluator::Evaluator(const NodeRegistry& registry) : registry_(&registry) {}

/// A key for every node in the graph, including the branches this evaluation
/// has no reason to visit: the editor draws all of them, and needs to know
/// which of its pictures a change has just invalidated.
std::vector<NodeSignature> Evaluator::signaturesOf(const Graph& graph, bool isFinalBuild) const {
    std::map<std::string, std::uint64_t> keys;
    std::vector<NodeSignature> signatures;
    for (const GraphNode* node : topologicalOrder(graph)) {
        const NodeType* type = registry_->find(node->typeName);
        if (type == nullptr) {
            continue;
        }
        const std::uint64_t key = nodeCacheKey(graph, *node, *type, isFinalBuild, keys);
        keys.emplace(node->id, key);
        signatures.push_back(NodeSignature{node->id, key});
    }
    return signatures;
}

void Evaluator::clearCache() {
    cache_.clear();
}

EvaluationOutcome Evaluator::evaluate(const Graph& graph, std::string_view nodeId, int outputPort,
                                      const EvaluationOptions& options) {
    EvaluationOutcome outcome;

    const GraphValidation validation = validateGraph(graph, *registry_);
    if (!validation.isValid) {
        outcome.errors = validation.errors;
        return outcome;
    }

    const std::vector<const GraphNode*> order = dependencyOrder(graph, nodeId);
    if (order.empty()) {
        outcome.errors.push_back({"", "", "Node '" + std::string(nodeId) + "' is not in the graph"});
        return outcome;
    }

    outcome.signatures = signaturesOf(graph, options.isFinalBuild);

    // Cache keys and wave depths, both of which only need the dependency order.
    std::map<std::string, NodePlan> plans;
    std::map<std::string, std::uint64_t> keys;
    std::vector<NodePlan*> planOrder;
    int deepest = 0;
    for (const GraphNode* node : order) {
        NodePlan plan;
        plan.node = node;
        plan.type = registry_->find(node->typeName);
        plan.cacheKey = nodeCacheKey(graph, *node, *plan.type, options.isFinalBuild, keys);
        keys.emplace(node->id, plan.cacheKey);

        for (std::size_t port = 0; port < plan.type->inputs.size(); ++port) {
            const GraphEdge* edge = graph.incoming(node->id, static_cast<int>(port));
            if (edge != nullptr) {
                plan.depth = std::max(plan.depth, plans.at(edge->fromNodeId).depth + 1);
            }
        }
        deepest = std::max(deepest, plan.depth);
        planOrder.push_back(&plans.emplace(node->id, plan).first->second);
    }

    std::map<std::string, std::shared_ptr<NodeResult>> results;
    for (int depth = 0; depth <= deepest; ++depth) {
        std::vector<NodePlan*> wave;
        for (NodePlan* plan : planOrder) {
            if (plan->depth != depth) {
                continue;
            }
            const auto cached = cache_.find(plan->cacheKey);
            if (cached != cache_.end()) {
                results[plan->node->id] = cached->second;
                outcome.timings.push_back(NodeTiming{plan->node->id, 0.0, true});
                continue;
            }
            wave.push_back(plan);
        }
        if (wave.empty()) {
            continue;
        }
        if (options.cancellation != nullptr && options.cancellation->isCancelled()) {
            outcome.wasCancelled = true;
            return outcome;
        }

        // Nodes in a wave run concurrently, so the caller's progress callback
        // is serialised here rather than being made thread-safe everywhere.
        std::mutex progressMutex;
        std::vector<std::shared_ptr<NodeResult>> waveResults(wave.size());
        std::vector<NodeTiming> waveTimings(wave.size());
        std::vector<GraphError> waveErrors(wave.size());
        std::vector<std::function<void()>> tasks;
        tasks.reserve(wave.size());

        for (std::size_t index = 0; index < wave.size(); ++index) {
            NodePlan* plan = wave[index];
            tasks.push_back([&, index, plan] {
                const auto started = std::chrono::steady_clock::now();

                auto result = std::make_shared<NodeResult>();
                for (const PortDescriptor& port : plan->type->outputs) {
                    result->outputs.push_back(makeOutput(graph.settings, port));
                }

                ContextState state;
                state.resolution = graph.settings.resolution;
                state.cellSizeMeters = graph.settings.cellSizeMeters();
                state.georeference = &graph.settings.georeference;
                state.nodeSeed = deriveNodeSeed(graph.settings.seed, plan->node->id);
                state.outputs = &result->outputs;
                state.isFinalBuild = options.isFinalBuild;
                state.threadBudget = laneThreadBudget(options.threadCount, wave.size());
                state.cancellation = options.cancellation;

                for (std::size_t port = 0; port < plan->type->inputs.size(); ++port) {
                    const GraphEdge* edge = graph.incoming(plan->node->id, static_cast<int>(port));
                    if (edge == nullptr) {
                        state.inputs.push_back(nullptr);
                        continue;
                    }
                    const NodeResult& upstream = *results.at(edge->fromNodeId);
                    state.inputs.push_back(
                        &upstream.outputs[static_cast<std::size_t>(edge->fromPort)]);
                }

                for (const ParameterDescriptor& descriptor : plan->type->parameters) {
                    const ParameterValue value = resolveParameter(*plan->node, descriptor);
                    state.parameterNumbers.push_back(value.number);
                    state.parameterTexts.push_back(value.text);
                }

                const std::string nodeIdCopy = plan->node->id;
                const std::string typeNameCopy = plan->type->typeName;
                if (options.onProgress) {
                    state.onProgress = [&options, &progressMutex, nodeIdCopy,
                                        typeNameCopy](float fraction) {
                        const std::lock_guard<std::mutex> lock(progressMutex);
                        options.onProgress(ProgressUpdate{nodeIdCopy, typeNameCopy, fraction});
                    };
                }

                plan->type->evaluate(&terrainContextApi(), asEvaluationContext(state));

                const auto finished = std::chrono::steady_clock::now();
                const std::chrono::duration<double, std::milli> elapsed = finished - started;
                waveTimings[index] = NodeTiming{plan->node->id, elapsed.count(), false};
                if (state.hasError) {
                    waveErrors[index] = GraphError{plan->node->id, "", state.error};
                    return;
                }
                // Emission order is whatever the node's lanes happened to be;
                // sorting by id is what makes a point set reproducible.
                for (NodeOutput& output : result->outputs) {
                    if (output.points != nullptr) {
                        output.points->canonicalise();
                    }
                }
                waveResults[index] = std::move(result);
            });
        }

        runBatch(tasks, options.threadCount);

        for (std::size_t index = 0; index < wave.size(); ++index) {
            outcome.timings.push_back(waveTimings[index]);
            if (!waveErrors[index].message.empty()) {
                outcome.errors.push_back(waveErrors[index]);
                continue;
            }
            outcome.evaluatedNodeIds.push_back(wave[index]->node->id);
            cache_[wave[index]->cacheKey] = waveResults[index];
            results[wave[index]->node->id] = waveResults[index];
        }
        if (!outcome.errors.empty()) {
            return outcome;
        }
    }

    collectThumbnails(results, options.thumbnailResolution, graph.settings.worldSizeMeters,
                      outcome);

    const std::shared_ptr<NodeResult>& target = results.at(std::string(nodeId));
    if (outputPort < 0 || outputPort >= static_cast<int>(target->outputs.size())) {
        outcome.errors.push_back({std::string(nodeId), "", "Output port is out of range"});
        return outcome;
    }
    const NodeOutput& requested = target->outputs[static_cast<std::size_t>(outputPort)];
    outcome.field = requested.field;
    outcome.color = requested.color;
    outcome.points = requested.points;
    return outcome;
}

}  // namespace terrain
