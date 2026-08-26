#include "engine_host.h"

namespace bridge {

EngineHost::EngineHost() : evaluator_(registry_) {
    terrain::registerBuiltinNodes(registry_);
}

EngineHost& EngineHost::instance() {
    static EngineHost host;
    return host;
}

std::vector<terrain::PluginReport> EngineHost::loadPlugins(
    const std::vector<std::string>& directories) {
    const std::lock_guard<std::mutex> evaluationLock(evaluationMutex_);

    for (const std::string& directory : directories) {
        plugins_.loadDirectory(directory, registry_);
    }
    return plugins_.reports();
}

void EngineHost::cancelCurrent() {
    const std::lock_guard<std::mutex> lock(tokenMutex_);
    if (currentToken_ != nullptr) {
        currentToken_->cancel();
    }
}

terrain::EvaluationOutcome EngineHost::evaluate(const terrain::Graph& graph,
                                                const std::string& nodeId, int outputPort,
                                                terrain::EvaluationOptions options,
                                                terrain::CancellationToken& token) {
    const std::lock_guard<std::mutex> evaluationLock(evaluationMutex_);
    {
        const std::lock_guard<std::mutex> tokenLock(tokenMutex_);
        currentToken_ = &token;
    }

    options.cancellation = &token;
    terrain::EvaluationOutcome outcome = evaluator_.evaluate(graph, nodeId, outputPort, options);

    {
        const std::lock_guard<std::mutex> tokenLock(tokenMutex_);
        currentToken_ = nullptr;
    }
    return outcome;
}

}  // namespace bridge
