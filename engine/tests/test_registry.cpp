#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>

#include "terrain/registry.h"

using terrain::NodeRegistry;
using terrain::NodeType;
using terrain::ParameterType;
using terrain::PortType;

namespace {

void noopEvaluate(const TerrainContextApi*, TerrainEvaluationContext*) {}

NodeRegistry builtinRegistry() {
    NodeRegistry registry;
    terrain::registerBuiltinNodes(registry);
    return registry;
}

}  // namespace

TEST_CASE("built-ins register through the plugin entry point", "[registry]") {
    const NodeRegistry registry = builtinRegistry();

    REQUIRE(registry.find("ImportDEM") != nullptr);
    REQUIRE(registry.find("Constant") != nullptr);
    REQUIRE(registry.find("Blur") != nullptr);
    REQUIRE(registry.find("Add") != nullptr);
    REQUIRE(registry.find("Export") != nullptr);
    REQUIRE(registry.find("NoSuchNode") == nullptr);
}

TEST_CASE("the ABI version the registry advertises is the SDK's", "[registry]") {
    NodeRegistry registry;
    REQUIRE(registry.api().abiVersion == TERRAIN_SDK_ABI_VERSION);
}

TEST_CASE("listing node types is order-stable", "[registry]") {
    const NodeRegistry registry = builtinRegistry();
    const std::vector<const NodeType*> types = registry.all();

    REQUIRE(types.size() == registry.size());
    REQUIRE(std::is_sorted(types.begin(), types.end(),
                           [](const NodeType* left, const NodeType* right) {
                               return left->typeName < right->typeName;
                           }));
}

TEST_CASE("ports and parameters survive the C descriptor round trip", "[registry]") {
    const NodeRegistry registry = builtinRegistry();
    const NodeType* blur = registry.find("Blur");
    REQUIRE(blur != nullptr);

    REQUIRE(blur->category == "Filter");
    REQUIRE(blur->inputs.size() == 2);
    REQUIRE(blur->inputs[0].name == "In");
    REQUIRE_FALSE(blur->inputs[0].isOptional);
    REQUIRE(blur->inputs[1].name == "Mask");
    REQUIRE(blur->inputs[1].isOptional);
    REQUIRE(blur->inputs[1].fieldSemantics == terrain::FieldSemantics::Mask);

    const terrain::ParameterDescriptor* radius = blur->parameter("radius");
    REQUIRE(radius != nullptr);
    REQUIRE(radius->unit == "m");
    REQUIRE(radius->type == ParameterType::Float);
    REQUIRE(radius->defaultValue == 50.0);
}

TEST_CASE("registering the same type name twice is refused", "[registry]") {
    NodeRegistry registry;
    const TerrainNodeDescriptor descriptor = {
        "Duplicate", "Test", "", 1, nullptr, 0, nullptr, 0, nullptr, 0, &noopEvaluate};

    REQUIRE(registry.add(descriptor));
    REQUIRE_FALSE(registry.add(descriptor));
    REQUIRE(registry.size() == 1);
}

TEST_CASE("a descriptor without an evaluate function is refused", "[registry]") {
    NodeRegistry registry;
    const TerrainNodeDescriptor descriptor = {
        "Broken", "Test", "", 1, nullptr, 0, nullptr, 0, nullptr, 0, nullptr};

    REQUIRE_FALSE(registry.add(descriptor));
}

TEST_CASE("the registry serialises to JSON the UI can read", "[registry]") {
    const NodeRegistry registry = builtinRegistry();
    const std::string json = registry.toJson();

    REQUIRE(json.starts_with("{\"abiVersion\":1,\"nodes\":["));
    REQUIRE(json.find("\"typeName\":\"Blur\"") != std::string::npos);
    REQUIRE(json.find("\"category\":\"Filter\"") != std::string::npos);
    REQUIRE(json.find("\"unit\":\"m\"") != std::string::npos);
    REQUIRE(json.find("\"fieldSemantics\":\"mask\"") != std::string::npos);
    REQUIRE(json.ends_with("]}"));
}
