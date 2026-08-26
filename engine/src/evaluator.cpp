#include "terrain/evaluator.h"

#include <algorithm>
#include <chrono>
#include <map>
#include <mutex>
#include <thread>

#include "evaluation_context.h"
#include "terrain/hash.h"

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

int workerCount(int requested, std::size_t taskCount) {
    int available = requested;
    if (available <= 0) {
        available = static_cast<int>(std::thread::hardware_concurrency());
    }
    if (available <= 0) {
        available = 1;
    }
    return std::min<int>(available, static_cast<int>(std::max<std::size_t>(taskCount, 1)));
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

std::shared_ptr<Heightfield> makeOutputField(const GraphSettings& settings,
                                             const PortDescriptor& port) {
    auto field = std::make_shared<Heightfield>(settings.resolution, settings.resolution);
    field->setCellSizeMeters(settings.cellSizeMeters());
    field->setGeoreference(settings.georeference);
    field->setSemantics(port.fieldSemantics);
    return field;
}

}  // namespace

Evaluator::Evaluator(const NodeRegistry& registry) : registry_(&registry) {}

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
        outcome.errors.push_back("Node '" + std::string(nodeId) + "' is not in the graph");
        return outcome;
    }

    // Cache keys and wave depths, both of which only need the dependency order.
    std::map<std::string, NodePlan> plans;
    std::vector<NodePlan*> planOrder;
    int deepest = 0;
    for (const GraphNode* node : order) {
        NodePlan plan;
        plan.node = node;
        plan.type = registry_->find(node->typeName);

        Hasher hasher;
        hasher.combine(plan.type->typeName);
        hasher.combine(static_cast<std::uint64_t>(plan.type->version));
        hashSettings(hasher, graph.settings);
        hashParameters(hasher, *node, *plan.type);
        for (std::size_t port = 0; port < plan.type->inputs.size(); ++port) {
            const GraphEdge* edge = graph.incoming(node->id, static_cast<int>(port));
            if (edge == nullptr) {
                hasher.combine(std::uint64_t{0});
                continue;
            }
            const NodePlan& upstream = plans.at(edge->fromNodeId);
            hasher.combine(upstream.cacheKey);
            hasher.combine(static_cast<std::uint64_t>(edge->fromPort));
            plan.depth = std::max(plan.depth, upstream.depth + 1);
        }
        plan.cacheKey = hasher.digest();
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
        std::vector<std::string> waveErrors(wave.size());
        std::vector<std::function<void()>> tasks;
        tasks.reserve(wave.size());

        for (std::size_t index = 0; index < wave.size(); ++index) {
            NodePlan* plan = wave[index];
            tasks.push_back([&, index, plan] {
                const auto started = std::chrono::steady_clock::now();

                auto result = std::make_shared<NodeResult>();
                for (const PortDescriptor& port : plan->type->outputs) {
                    result->outputs.push_back(makeOutputField(graph.settings, port));
                }

                ContextState state;
                state.resolution = graph.settings.resolution;
                state.cellSizeMeters = graph.settings.cellSizeMeters();
                state.georeference = &graph.settings.georeference;
                state.nodeSeed = deriveNodeSeed(graph.settings.seed, plan->node->id);
                state.outputs = &result->outputs;
                state.isFinalBuild = options.isFinalBuild;
                state.cancellation = options.cancellation;

                for (std::size_t port = 0; port < plan->type->inputs.size(); ++port) {
                    const GraphEdge* edge = graph.incoming(plan->node->id, static_cast<int>(port));
                    if (edge == nullptr) {
                        state.inputs.push_back(nullptr);
                        continue;
                    }
                    const NodeResult& upstream = *results.at(edge->fromNodeId);
                    state.inputs.push_back(
                        upstream.outputs[static_cast<std::size_t>(edge->fromPort)]->data());
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
                    waveErrors[index] = plan->node->id + ": " + state.error;
                    return;
                }
                waveResults[index] = std::move(result);
            });
        }

        runBatch(tasks, options.threadCount);

        for (std::size_t index = 0; index < wave.size(); ++index) {
            outcome.timings.push_back(waveTimings[index]);
            if (!waveErrors[index].empty()) {
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

    const std::shared_ptr<NodeResult>& target = results.at(std::string(nodeId));
    if (outputPort < 0 || outputPort >= static_cast<int>(target->outputs.size())) {
        outcome.errors.push_back("Output port is out of range");
        return outcome;
    }
    outcome.field = target->outputs[static_cast<std::size_t>(outputPort)];
    return outcome;
}

}  // namespace terrain
