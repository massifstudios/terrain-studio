#include "terrain/registry.h"

#include <algorithm>

#include "terrain/json_writer.h"

namespace terrain {
namespace {

std::string textOrEmpty(const char* text) {
    if (text == nullptr) {
        return {};
    }
    return text;
}

PortDescriptor convertPort(const TerrainPortDescriptor& source) {
    PortDescriptor port;
    port.name = textOrEmpty(source.name);
    port.description = textOrEmpty(source.description);
    port.type = static_cast<PortType>(source.type);
    port.fieldSemantics = static_cast<FieldSemantics>(source.fieldSemantics);
    port.isOptional = source.isOptional != 0;
    return port;
}

std::vector<EnumOption> convertEnumOptions(const TerrainParameterDescriptor& source) {
    std::vector<EnumOption> options;
    for (int index = 0; index < source.enumOptionCount; ++index) {
        const TerrainEnumOption& option = source.enumOptions[index];
        options.push_back(
            EnumOption{textOrEmpty(option.name), textOrEmpty(option.label), option.value});
    }
    return options;
}

ParameterDescriptor convertParameter(const TerrainParameterDescriptor& source) {
    ParameterDescriptor parameter;
    parameter.name = textOrEmpty(source.name);
    parameter.label = textOrEmpty(source.label);
    parameter.description = textOrEmpty(source.description);
    parameter.unit = textOrEmpty(source.unit);
    parameter.type = static_cast<ParameterType>(source.type);
    parameter.defaultValue = source.defaultValue;
    parameter.minimumValue = source.minimumValue;
    parameter.maximumValue = source.maximumValue;
    parameter.defaultText = textOrEmpty(source.defaultText);
    parameter.enumOptions = convertEnumOptions(source);
    return parameter;
}

template <typename Source, typename Converted>
std::vector<Converted> convertAll(const Source* items, int count,
                                  Converted (*convert)(const Source&)) {
    std::vector<Converted> converted;
    converted.reserve(static_cast<std::size_t>(std::max(count, 0)));
    for (int index = 0; index < count; ++index) {
        converted.push_back(convert(items[index]));
    }
    return converted;
}

const char* portTypeName(PortType type) {
    if (type == PortType::Color) {
        return "color";
    }
    if (type == PortType::Points) {
        return "points";
    }
    return "field";
}

const char* parameterTypeName(ParameterType type) {
    switch (type) {
        case ParameterType::Int: return "int";
        case ParameterType::Bool: return "bool";
        case ParameterType::Enum: return "enum";
        case ParameterType::Seed: return "seed";
        case ParameterType::ColorRamp: return "colorRamp";
        case ParameterType::Path: return "path";
        case ParameterType::Text: return "text";
        case ParameterType::Float: break;
    }
    return "float";
}

const char* semanticsName(FieldSemantics semantics) {
    if (semantics == FieldSemantics::Mask) {
        return "mask";
    }
    return "heightfield";
}

void writePort(JsonWriter& writer, const PortDescriptor& port) {
    writer.beginObject();
    writer.member("name", port.name);
    writer.member("description", port.description);
    writer.member("type", portTypeName(port.type));
    writer.member("fieldSemantics", semanticsName(port.fieldSemantics));
    writer.member("optional", port.isOptional);
    writer.endObject();
}

void writeParameter(JsonWriter& writer, const ParameterDescriptor& parameter) {
    writer.beginObject();
    writer.member("name", parameter.name);
    writer.member("label", parameter.label);
    writer.member("description", parameter.description);
    writer.member("unit", parameter.unit);
    writer.member("type", parameterTypeName(parameter.type));
    writer.member("defaultValue", parameter.defaultValue);
    writer.member("minimum", parameter.minimumValue);
    writer.member("maximum", parameter.maximumValue);
    writer.member("defaultText", parameter.defaultText);
    writer.key("options");
    writer.beginArray();
    for (const EnumOption& option : parameter.enumOptions) {
        writer.beginObject();
        writer.member("name", option.name);
        writer.member("label", option.label);
        writer.member("value", static_cast<long long>(option.value));
        writer.endObject();
    }
    writer.endArray();
    writer.endObject();
}

void writePortArray(JsonWriter& writer, std::string_view name,
                    const std::vector<PortDescriptor>& ports) {
    writer.key(name);
    writer.beginArray();
    for (const PortDescriptor& port : ports) {
        writePort(writer, port);
    }
    writer.endArray();
}

void writeNodeType(JsonWriter& writer, const NodeType& type) {
    writer.beginObject();
    writer.member("typeName", type.typeName);
    writer.member("category", type.category);
    writer.member("description", type.description);
    writer.member("version", static_cast<long long>(type.version));
    writePortArray(writer, "inputs", type.inputs);
    writePortArray(writer, "outputs", type.outputs);
    writer.key("parameters");
    writer.beginArray();
    for (const ParameterDescriptor& parameter : type.parameters) {
        writeParameter(writer, parameter);
    }
    writer.endArray();
    writer.endObject();
}

int32_t registerNodeThunk(TerrainRegistry* registry, const TerrainNodeDescriptor* descriptor) {
    if (registry == nullptr || descriptor == nullptr) {
        return 0;
    }
    NodeRegistry* target = reinterpret_cast<NodeRegistry*>(registry);
    if (target->add(*descriptor)) {
        return 1;
    }
    return 0;
}

}  // namespace

std::optional<int> NodeType::parameterIndex(std::string_view name) const {
    for (std::size_t index = 0; index < parameters.size(); ++index) {
        if (parameters[index].name == name) {
            return static_cast<int>(index);
        }
    }
    return std::nullopt;
}

const ParameterDescriptor* NodeType::parameter(std::string_view name) const {
    const std::optional<int> index = parameterIndex(name);
    if (!index.has_value()) {
        return nullptr;
    }
    return &parameters[static_cast<std::size_t>(*index)];
}

TerrainRegistry* NodeRegistry::handle() {
    return reinterpret_cast<TerrainRegistry*>(this);
}

TerrainRegistryApi NodeRegistry::api() {
    return TerrainRegistryApi{TERRAIN_SDK_ABI_VERSION, &registerNodeThunk};
}

bool NodeRegistry::add(const TerrainNodeDescriptor& descriptor) {
    if (descriptor.typeName == nullptr || descriptor.evaluate == nullptr) {
        return false;
    }

    NodeType type;
    type.typeName = descriptor.typeName;
    type.category = textOrEmpty(descriptor.category);
    type.description = textOrEmpty(descriptor.description);
    type.version = descriptor.version;
    type.inputs = convertAll(descriptor.inputs, descriptor.inputCount, convertPort);
    type.outputs = convertAll(descriptor.outputs, descriptor.outputCount, convertPort);
    type.parameters =
        convertAll(descriptor.parameters, descriptor.parameterCount, convertParameter);
    type.evaluate = descriptor.evaluate;

    return types_.emplace(type.typeName, std::move(type)).second;
}

const NodeType* NodeRegistry::find(std::string_view typeName) const {
    const auto found = types_.find(typeName);
    if (found == types_.end()) {
        return nullptr;
    }
    return &found->second;
}

std::vector<const NodeType*> NodeRegistry::all() const {
    std::vector<const NodeType*> types;
    types.reserve(types_.size());
    for (const auto& [name, type] : types_) {
        types.push_back(&type);
    }
    return types;
}

std::string NodeRegistry::toJson() const {
    JsonWriter writer;
    writer.beginObject();
    writer.member("abiVersion", static_cast<long long>(TERRAIN_SDK_ABI_VERSION));
    writer.key("nodes");
    writer.beginArray();
    for (const NodeType* type : all()) {
        writeNodeType(writer, *type);
    }
    writer.endArray();
    writer.endObject();
    return writer.text();
}

}  // namespace terrain
