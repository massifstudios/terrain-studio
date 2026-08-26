#pragma once

#include <mutex>
#include <string>
#include <vector>

#include "terrain/evaluator.h"
#include "terrain/plugin_host.h"
#include "terrain/registry.h"

namespace bridge {

/**
 * Process-wide engine state. One registry, one result cache, one evaluation
 * at a time — the addon lives in a dedicated utilityProcess, so there is
 * nothing to share it with.
 */
class EngineHost {
public:
    static EngineHost& instance();

    const terrain::NodeRegistry& registry() const { return registry_; }

    terrain::EvaluationOutcome evaluate(const terrain::Graph& graph, const std::string& nodeId,
                                        int outputPort, terrain::EvaluationOptions options,
                                        terrain::CancellationToken& token);

    /// Cancels whatever is running, so a new preview can take its place.
    void cancelCurrent();

    /// Scans each directory in turn and returns what became of every library
    /// seen so far, not just the ones this call found — a rescan is additive,
    /// and the UI wants the whole picture. Nodes may only arrive between
    /// evaluations, so this waits for one to finish rather than registering
    /// underneath it.
    std::vector<terrain::PluginReport> loadPlugins(const std::vector<std::string>& directories);

private:
    EngineHost();

    terrain::NodeRegistry registry_;
    terrain::PluginHost plugins_;
    terrain::Evaluator evaluator_;
    std::mutex evaluationMutex_;
    std::mutex tokenMutex_;
    terrain::CancellationToken* currentToken_ = nullptr;
};

}  // namespace bridge
