#include <napi.h>

#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "engine_host.h"
#include "graph_conversion.h"
#include "terrain/formats/massifdem.h"
#include "terrain/version.h"

namespace {

const char* semanticsName(terrain::FieldSemantics semantics) {
    if (semantics == terrain::FieldSemantics::Mask) {
        return "mask";
    }
    return "heightfield";
}

struct ProgressItem {
    std::string nodeId;
    std::string typeName;
    float fraction = 0.0f;
};

Napi::Value EngineVersion(const Napi::CallbackInfo& info) {
    return Napi::String::New(info.Env(), std::string(terrain::engineVersion()));
}

Napi::Value NodeRegistryJson(const Napi::CallbackInfo& info) {
    return Napi::String::New(info.Env(), bridge::EngineHost::instance().registry().toJson());
}

/// Header-only description of a DEM on disk, for the import dialog.
Napi::Value DescribeElevationModel(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    if (info.Length() < 1 || !info[0].IsString()) {
        throw Napi::TypeError::New(env, "describeElevationModel(path) expects a path");
    }

    const terrain::formats::DemInfo description =
        terrain::formats::readMassifDemInfo(info[0].As<Napi::String>().Utf8Value());

    Napi::Object result = Napi::Object::New(env);
    result.Set("width", Napi::Number::New(env, description.width));
    result.Set("height", Napi::Number::New(env, description.height));
    result.Set("cellSizeMeters", Napi::Number::New(env, description.cellSizeMeters));
    result.Set("minimumAltitudeMeters",
               Napi::Number::New(env, description.minimumAltitudeMeters));
    result.Set("maximumAltitudeMeters",
               Napi::Number::New(env, description.maximumAltitudeMeters));
    result.Set("label", Napi::String::New(env, description.label));

    Napi::Object georeference = Napi::Object::New(env);
    georeference.Set("coordinateSystem",
                     Napi::String::New(env, description.georeference.coordinateSystem));
    georeference.Set("eastingMinMeters",
                     Napi::Number::New(env, description.georeference.eastingMinMeters));
    georeference.Set("northingMaxMeters",
                     Napi::Number::New(env, description.georeference.northingMaxMeters));
    result.Set("georeference", georeference);
    return result;
}

Napi::Value CancelEvaluation(const Napi::CallbackInfo& info) {
    bridge::EngineHost::instance().cancelCurrent();
    return info.Env().Undefined();
}

Napi::Array toStringArray(Napi::Env env, const std::vector<std::string>& values) {
    Napi::Array array = Napi::Array::New(env, values.size());
    for (std::size_t index = 0; index < values.size(); ++index) {
        array.Set(static_cast<std::uint32_t>(index), Napi::String::New(env, values[index]));
    }
    return array;
}

Napi::Array toTimingArray(Napi::Env env, const std::vector<terrain::NodeTiming>& timings) {
    Napi::Array array = Napi::Array::New(env, timings.size());
    for (std::size_t index = 0; index < timings.size(); ++index) {
        Napi::Object entry = Napi::Object::New(env);
        entry.Set("nodeId", Napi::String::New(env, timings[index].nodeId));
        entry.Set("milliseconds", Napi::Number::New(env, timings[index].milliseconds));
        entry.Set("servedFromCache", Napi::Boolean::New(env, timings[index].servedFromCache));
        array.Set(static_cast<std::uint32_t>(index), entry);
    }
    return array;
}

/**
 * Runs one graph evaluation off the JavaScript thread, streaming per-node
 * progress back as it goes.
 *
 * The result is copied into a fresh ArrayBuffer rather than wrapped as an
 * external one: Electron builds Node-API without external buffer support, and
 * the copy is paid once per evaluation before the buffer transfers to the
 * renderer for free.
 */
class EvaluateWorker : public Napi::AsyncProgressQueueWorker<ProgressItem> {
public:
    EvaluateWorker(Napi::Env env, terrain::Graph graph, std::string nodeId, int outputPort,
                   terrain::EvaluationOptions options, Napi::Function onProgress)
        : Napi::AsyncProgressQueueWorker<ProgressItem>(env),
          graph_(std::move(graph)),
          nodeId_(std::move(nodeId)),
          outputPort_(outputPort),
          options_(std::move(options)),
          deferred_(Napi::Promise::Deferred::New(env)),
          progressReference_(Napi::Persistent(onProgress)) {}

    Napi::Promise promise() { return deferred_.Promise(); }

    void Execute(const ExecutionProgress& reporter) override {
        options_.onProgress = [&reporter](const terrain::ProgressUpdate& update) {
            const ProgressItem item{update.nodeId, update.typeName, update.fraction};
            reporter.Send(&item, 1);
        };
        outcome_ = bridge::EngineHost::instance().evaluate(graph_, nodeId_, outputPort_, options_,
                                                          token_);
    }

    void OnProgress(const ProgressItem* items, std::size_t count) override {
        const Napi::Env env = Env();
        Napi::HandleScope scope(env);
        for (std::size_t index = 0; index < count; ++index) {
            Napi::Object update = Napi::Object::New(env);
            update.Set("nodeId", Napi::String::New(env, items[index].nodeId));
            update.Set("typeName", Napi::String::New(env, items[index].typeName));
            update.Set("fraction", Napi::Number::New(env, items[index].fraction));
            progressReference_.Call({update});
        }
    }

    void OnOK() override {
        const Napi::Env env = Env();
        Napi::HandleScope scope(env);
        deferred_.Resolve(buildResult(env));
    }

    void OnError(const Napi::Error& error) override { deferred_.Reject(error.Value()); }

private:
    Napi::Value buildResult(Napi::Env env) {
        Napi::Object result = Napi::Object::New(env);
        result.Set("cancelled", Napi::Boolean::New(env, outcome_.wasCancelled));
        result.Set("errors", toStringArray(env, outcome_.errors));
        result.Set("evaluatedNodeIds", toStringArray(env, outcome_.evaluatedNodeIds));
        result.Set("timings", toTimingArray(env, outcome_.timings));
        if (outcome_.field == nullptr) {
            return result;
        }

        const terrain::Heightfield& field = *outcome_.field;
        const std::size_t byteLength = field.sampleCount() * sizeof(float);
        Napi::ArrayBuffer buffer = Napi::ArrayBuffer::New(env, byteLength);
        std::memcpy(buffer.Data(), field.data(), byteLength);

        result.Set("samples", Napi::Float32Array::New(env, field.sampleCount(), buffer, 0));
        result.Set("width", Napi::Number::New(env, field.width()));
        result.Set("height", Napi::Number::New(env, field.height()));
        result.Set("cellSizeMeters", Napi::Number::New(env, field.cellSizeMeters()));
        result.Set("minimum", Napi::Number::New(env, field.minimum()));
        result.Set("maximum", Napi::Number::New(env, field.maximum()));
        result.Set("semantics", Napi::String::New(env, semanticsName(field.semantics())));
        return result;
    }

    terrain::Graph graph_;
    std::string nodeId_;
    int outputPort_ = 0;
    terrain::EvaluationOptions options_;
    terrain::CancellationToken token_;
    terrain::EvaluationOutcome outcome_;
    Napi::Promise::Deferred deferred_;
    Napi::FunctionReference progressReference_;
};

terrain::EvaluationOptions optionsFromJs(const Napi::Object& source) {
    terrain::EvaluationOptions options;
    if (source.Has("threadCount")) {
        options.threadCount = source.Get("threadCount").ToNumber().Int32Value();
    }
    if (source.Has("isFinalBuild")) {
        options.isFinalBuild = source.Get("isFinalBuild").ToBoolean().Value();
    }
    return options;
}

Napi::Value Evaluate(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    if (info.Length() < 5 || !info[0].IsObject() || !info[4].IsFunction()) {
        throw Napi::TypeError::New(
            env, "evaluate(graph, nodeId, outputPort, options, onProgress) expects five arguments");
    }

    auto* worker = new EvaluateWorker(
        env, bridge::graphFromJs(info[0].As<Napi::Object>()), info[1].ToString().Utf8Value(),
        info[2].ToNumber().Int32Value(), optionsFromJs(info[3].As<Napi::Object>()),
        info[4].As<Napi::Function>());
    Napi::Promise promise = worker->promise();
    worker->Queue();
    return promise;
}

Napi::Object Initialize(Napi::Env env, Napi::Object exports) {
    exports.Set("engineVersion", Napi::Function::New(env, EngineVersion));
    exports.Set("nodeRegistryJson", Napi::Function::New(env, NodeRegistryJson));
    exports.Set("evaluate", Napi::Function::New(env, Evaluate));
    exports.Set("cancelEvaluation", Napi::Function::New(env, CancelEvaluation));
    exports.Set("describeElevationModel", Napi::Function::New(env, DescribeElevationModel));
    return exports;
}

}  // namespace

NODE_API_MODULE(engine, Initialize)
