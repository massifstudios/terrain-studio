#pragma once

#include <mutex>

#include "terrain/evaluator.h"
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

private:
    EngineHost();

    terrain::NodeRegistry registry_;
    terrain::Evaluator evaluator_;
    std::mutex evaluationMutex_;
    std::mutex tokenMutex_;
    terrain::CancellationToken* currentToken_ = nullptr;
};

}  // namespace bridge
