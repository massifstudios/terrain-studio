#include <algorithm>
#include <exception>
#include <filesystem>

#include "node_support.h"
#include "terrain/formats/hillshade.h"
#include "terrain/formats/massifdem.h"

namespace terrain::nodes {
namespace {

const TerrainPortDescriptor exportInputs[] = {
    {"In", "The field to write", TerrainPortType_Field, TerrainFieldSemantics_Heightfield, 0}};

const TerrainPortDescriptor exportOutputs[] = {
    {"Out", "The same field, so the export can sit mid-graph", TerrainPortType_Field,
     TerrainFieldSemantics_Heightfield, 0}};

const TerrainParameterDescriptor exportParameters[] = {
    {"path", "File", "Where the .massifdem is written; the preview goes beside it", "",
     TerrainParameterType_Path, 0.0, 0.0, 0.0, "", nullptr, 0},
    {"label", "Label", "Provenance text stored in the file header", "",
     TerrainParameterType_Text, 0.0, 0.0, 0.0, "Terrain Studio export", nullptr, 0}};

Heightfield fieldFromContext(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const int size = api->width(context);
    Heightfield field(size, size);
    field.setCellSizeMeters(api->cellSizeMeters(context));
    field.setGeoreference(GeoReference{api->coordinateSystem(context),
                                       api->eastingMinMeters(context),
                                       api->northingMaxMeters(context)});

    const float* input = api->inputField(context, 0);
    std::copy(input, input + field.sampleCount(), field.data());
    return field;
}

void evaluateExport(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const float* input = api->inputField(context, 0);
    float* output = api->outputField(context, 0);
    const std::size_t count = cellCount(api, context);
    std::copy(input, input + count, output);

    // Interactive preview evaluates the same graph constantly; only a build writes.
    if (api->isFinalBuild(context) == 0) {
        return;
    }

    const char* path = api->parameterText(context, 0);
    if (path == nullptr || path[0] == '\0') {
        api->setError(context, "No export path set");
        return;
    }

    try {
        const Heightfield field = fieldFromContext(api, context);
        const std::filesystem::path demPath(path);
        formats::writeMassifDem(demPath, field, api->parameterText(context, 1));
        formats::writeHillshadePreview(formats::previewPathFor(demPath), field);
    } catch (const std::exception& error) {
        api->setError(context, error.what());
    }
}

const TerrainNodeDescriptor exportDescriptor = {
    "Export",
    "Output",
    "Writes a .massifdem and its hillshade preview during a build",
    1,
    exportInputs,
    1,
    exportOutputs,
    1,
    exportParameters,
    2,
    &evaluateExport};

}  // namespace

void registerOutputNodes(const TerrainRegistryApi& api, TerrainRegistry* registry) {
    api.registerNode(registry, &exportDescriptor);
}

}  // namespace terrain::nodes
