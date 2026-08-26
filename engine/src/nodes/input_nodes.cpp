#include <algorithm>
#include <exception>

#include "node_support.h"
#include "terrain/formats/elevation_import.h"
#include "terrain/resample.h"

namespace terrain::nodes {
namespace {

const TerrainPortDescriptor importOutputs[] = {
    {"Out", "The imported elevation model, resampled to the graph resolution",
     TerrainPortType_Field, TerrainFieldSemantics_Heightfield, 0}};

const TerrainParameterDescriptor importParameters[] = {
    {"path", "File", "A .massifdem, a 16-bit PNG heightmap, or a raw float32 grid", "",
     TerrainParameterType_Path, 0.0, 0.0, 0.0, "", nullptr, 0},
    {"minimumAltitude", "Minimum altitude", "PNG only: the altitude sample value 0 stands for",
     "m", TerrainParameterType_Float, 0.0, -12000.0, 12000.0, "", nullptr, 0},
    {"maximumAltitude", "Maximum altitude",
     "PNG only: the altitude the highest sample value stands for", "m",
     TerrainParameterType_Float, 1000.0, -12000.0, 12000.0, "", nullptr, 0},
    {"rawWidth", "Raw width", "Raw float32 only: samples per row", "px",
     TerrainParameterType_Int, 0.0, 0.0, 65536.0, "", nullptr, 0},
    {"rawHeight", "Raw height", "Raw float32 only: number of rows", "px",
     TerrainParameterType_Int, 0.0, 0.0, 65536.0, "", nullptr, 0}};

void evaluateImportDem(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const char* path = api->parameterText(context, 0);
    if (path == nullptr || path[0] == '\0') {
        api->setError(context, "No elevation model selected");
        return;
    }

    formats::ElevationImportOptions options;
    options.minimumAltitudeMeters = api->parameterNumber(context, 1);
    options.maximumAltitudeMeters = api->parameterNumber(context, 2);
    options.rawWidth = static_cast<int>(api->parameterNumber(context, 3));
    options.rawHeight = static_cast<int>(api->parameterNumber(context, 4));

    try {
        const Heightfield source = formats::readElevationModel(path, options);
        const int resolution = api->width(context);
        const double worldSize = resolution * api->cellSizeMeters(context);
        const Heightfield resampled = resampleToSquare(source, resolution, worldSize);
        std::copy(resampled.data(), resampled.data() + resampled.sampleCount(),
                  api->outputField(context, 0));
    } catch (const std::exception& error) {
        api->setError(context, error.what());
    }
}

const TerrainNodeDescriptor importDemDescriptor = {
    "ImportDEM",
    "Input",
    "Reads an elevation model from disk and fits it to the graph resolution",
    1,
    nullptr,
    0,
    importOutputs,
    1,
    importParameters,
    5,
    &evaluateImportDem};

const TerrainPortDescriptor constantOutputs[] = {
    {"Out", "A field with the same value in every cell", TerrainPortType_Field,
     TerrainFieldSemantics_Heightfield, 0}};

const TerrainParameterDescriptor constantParameters[] = {
    {"value", "Value", "The value written into every cell", "m", TerrainParameterType_Float, 0.0,
     -12000.0, 12000.0, "", nullptr, 0}};

void evaluateConstant(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const float value = static_cast<float>(api->parameterNumber(context, 0));
    float* output = api->outputField(context, 0);
    std::fill(output, output + cellCount(api, context), value);
}

const TerrainNodeDescriptor constantDescriptor = {"Constant",
                                                  "Input",
                                                  "A flat field, useful as an offset or a base",
                                                  1,
                                                  nullptr,
                                                  0,
                                                  constantOutputs,
                                                  1,
                                                  constantParameters,
                                                  1,
                                                  &evaluateConstant};

}  // namespace

void registerInputNodes(const TerrainRegistryApi& api, TerrainRegistry* registry) {
    api.registerNode(registry, &importDemDescriptor);
    api.registerNode(registry, &constantDescriptor);
}

}  // namespace terrain::nodes
