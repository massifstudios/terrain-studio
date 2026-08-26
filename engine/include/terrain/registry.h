#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "terrain/heightfield.h"
#include "terrain_node_sdk.h"

namespace terrain {

enum class PortType {
    Field = TerrainPortType_Field,
    Color = TerrainPortType_Color,
    Points = TerrainPortType_Points
};

enum class ParameterType {
    Float = TerrainParameterType_Float,
    Int = TerrainParameterType_Int,
    Bool = TerrainParameterType_Bool,
    Enum = TerrainParameterType_Enum,
    Seed = TerrainParameterType_Seed,
    ColorRamp = TerrainParameterType_ColorRamp,
    Path = TerrainParameterType_Path,
    Text = TerrainParameterType_Text,
    Curve = TerrainParameterType_Curve
};

struct EnumOption {
    std::string name;
    std::string label;
    int value = 0;
};

struct PortDescriptor {
    std::string name;
    std::string description;
    PortType type = PortType::Field;
    FieldSemantics fieldSemantics = FieldSemantics::Heightfield;
    bool isOptional = false;
};

struct ParameterDescriptor {
    std::string name;
    std::string label;
    std::string description;
    std::string unit;
    ParameterType type = ParameterType::Float;
    double defaultValue = 0.0;
    double minimumValue = 0.0;
    double maximumValue = 0.0;
    std::string defaultText;
    std::vector<EnumOption> enumOptions;
};

/// A registered node type. Owns copies of everything its descriptor pointed at.
struct NodeType {
    std::string typeName;
    std::string category;
    std::string description;
    std::uint32_t version = 1;
    std::vector<PortDescriptor> inputs;
    std::vector<PortDescriptor> outputs;
    std::vector<ParameterDescriptor> parameters;
    TerrainNodeEvaluate evaluate = nullptr;

    std::optional<int> parameterIndex(std::string_view name) const;
    const ParameterDescriptor* parameter(std::string_view name) const;
};

/**
 * The one place node types come from. Palette, property panel, DSL and
 * serializer all read this and nothing else, which is what makes adding a node
 * a single registration. It is also the plugin boundary: built-ins go through
 * exactly the C entry point plugins use.
 */
class NodeRegistry {
public:
    /// The C-ABI view handed to plugins, and used by built-ins.
    TerrainRegistryApi api();
    TerrainRegistry* handle();

    bool add(const TerrainNodeDescriptor& descriptor);

    const NodeType* find(std::string_view typeName) const;
    /// Sorted by type name, so any listing is order-stable.
    std::vector<const NodeType*> all() const;
    std::size_t size() const { return types_.size(); }

    /// The registry as JSON, for the property panel and the node palette.
    std::string toJson() const;

private:
    std::map<std::string, NodeType, std::less<>> types_;
};

/// Registers every node the engine ships with.
void registerBuiltinNodes(NodeRegistry& registry);

}  // namespace terrain
