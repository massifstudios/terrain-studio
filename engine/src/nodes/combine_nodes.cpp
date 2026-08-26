#include "node_support.h"

namespace terrain::nodes {
namespace {

const TerrainPortDescriptor addInputs[] = {
    {"A", "First field", TerrainPortType_Field, TerrainFieldSemantics_Heightfield, 0},
    {"B", "Second field", TerrainPortType_Field, TerrainFieldSemantics_Heightfield, 0}};

const TerrainPortDescriptor addOutputs[] = {
    {"Out", "A plus B, cell by cell", TerrainPortType_Field, TerrainFieldSemantics_Heightfield,
     0}};

void evaluateAdd(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const float* first = api->inputField(context, 0);
    const float* second = api->inputField(context, 1);
    float* output = api->outputField(context, 0);
    const std::size_t count = cellCount(api, context);

    for (std::size_t index = 0; index < count; ++index) {
        output[index] = first[index] + second[index];
    }
}

const TerrainNodeDescriptor addDescriptor = {"Add",
                                             "Combine",
                                             "Adds two fields together",
                                             1,
                                             addInputs,
                                             2,
                                             addOutputs,
                                             1,
                                             nullptr,
                                             0,
                                             &evaluateAdd};

}  // namespace

void registerCombineNodes(const TerrainRegistryApi& api, TerrainRegistry* registry) {
    api.registerNode(registry, &addDescriptor);
}

}  // namespace terrain::nodes
