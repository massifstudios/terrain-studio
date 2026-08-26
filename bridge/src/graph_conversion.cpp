#include "graph_conversion.h"

#include <string>

namespace bridge {
namespace {

bool hasObject(const Napi::Object& source, const char* name) {
    return source.Has(name) && source.Get(name).IsObject();
}

terrain::GeoReference georeferenceFrom(const Napi::Object& settings) {
    terrain::GeoReference reference;
    if (!hasObject(settings, "georeference")) {
        return reference;
    }
    const Napi::Object source = settings.Get("georeference").As<Napi::Object>();
    if (source.Has("coordinateSystem")) {
        reference.coordinateSystem = source.Get("coordinateSystem").ToString().Utf8Value();
    }
    if (source.Has("eastingMinMeters")) {
        reference.eastingMinMeters = source.Get("eastingMinMeters").ToNumber().DoubleValue();
    }
    if (source.Has("northingMaxMeters")) {
        reference.northingMaxMeters = source.Get("northingMaxMeters").ToNumber().DoubleValue();
    }
    return reference;
}

terrain::GraphSettings settingsFrom(const Napi::Object& source) {
    terrain::GraphSettings settings;
    if (!hasObject(source, "settings")) {
        return settings;
    }
    const Napi::Object values = source.Get("settings").As<Napi::Object>();
    if (values.Has("resolution")) {
        settings.resolution = values.Get("resolution").ToNumber().Int32Value();
    }
    if (values.Has("worldSizeMeters")) {
        settings.worldSizeMeters = values.Get("worldSizeMeters").ToNumber().DoubleValue();
    }
    if (values.Has("seed")) {
        settings.seed = static_cast<std::uint64_t>(values.Get("seed").ToNumber().Int64Value());
    }
    settings.georeference = georeferenceFrom(values);
    return settings;
}

terrain::ParameterValue parameterFrom(const Napi::Value& value) {
    if (value.IsString()) {
        return terrain::ParameterValue{0.0, value.As<Napi::String>().Utf8Value()};
    }
    if (value.IsBoolean()) {
        return terrain::ParameterValue{value.As<Napi::Boolean>().Value() ? 1.0 : 0.0, ""};
    }
    return terrain::ParameterValue{value.ToNumber().DoubleValue(), ""};
}

void readParameters(const Napi::Object& source, terrain::GraphNode& node) {
    if (!hasObject(source, "parameters")) {
        return;
    }
    const Napi::Object parameters = source.Get("parameters").As<Napi::Object>();
    const Napi::Array names = parameters.GetPropertyNames();
    for (std::uint32_t index = 0; index < names.Length(); ++index) {
        const std::string name = names.Get(index).ToString().Utf8Value();
        node.parameters[name] = parameterFrom(parameters.Get(name));
    }
}

void readNodes(const Napi::Object& source, terrain::Graph& graph) {
    if (!source.Has("nodes") || !source.Get("nodes").IsArray()) {
        return;
    }
    const Napi::Array nodes = source.Get("nodes").As<Napi::Array>();
    for (std::uint32_t index = 0; index < nodes.Length(); ++index) {
        const Napi::Object entry = nodes.Get(index).As<Napi::Object>();
        terrain::GraphNode& node = graph.addNode(entry.Get("id").ToString().Utf8Value(),
                                                 entry.Get("typeName").ToString().Utf8Value());
        readParameters(entry, node);
    }
}

void readEdges(const Napi::Object& source, terrain::Graph& graph) {
    if (!source.Has("edges") || !source.Get("edges").IsArray()) {
        return;
    }
    const Napi::Array edges = source.Get("edges").As<Napi::Array>();
    for (std::uint32_t index = 0; index < edges.Length(); ++index) {
        const Napi::Object entry = edges.Get(index).As<Napi::Object>();
        graph.connect(entry.Get("fromNodeId").ToString().Utf8Value(),
                      entry.Get("fromPort").ToNumber().Int32Value(),
                      entry.Get("toNodeId").ToString().Utf8Value(),
                      entry.Get("toPort").ToNumber().Int32Value());
    }
}

}  // namespace

terrain::Graph graphFromJs(const Napi::Object& source) {
    terrain::Graph graph;
    graph.settings = settingsFrom(source);
    readNodes(source, graph);
    readEdges(source, graph);
    return graph;
}

}  // namespace bridge
