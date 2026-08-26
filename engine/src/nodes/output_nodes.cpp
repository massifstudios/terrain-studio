#include <algorithm>
#include <array>
#include <exception>
#include <filesystem>
#include <vector>

#include "node_support.h"
#include "terrain/color_field.h"
#include "terrain/formats/heightfield_png.h"
#include "terrain/formats/hillshade.h"
#include "terrain/formats/massifdem.h"
#include "terrain/formats/point_files.h"
#include "terrain/formats/texture_maps.h"
#include "terrain/point_set.h"

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

/* --------------------------------------------------------- ExportPNG16 -- */

/// Whether this evaluation is the one allowed to touch the disk, and has
/// somewhere to write to. Every exporter asks the same two questions.
bool shouldWrite(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    // Interactive preview evaluates the same graph constantly; only a build
    // writes.
    if (api->isFinalBuild(context) == 0) {
        return false;
    }
    const char* path = api->parameterText(context, 0);
    if (path == nullptr || path[0] == '\0') {
        api->setError(context, "No export path set");
        return false;
    }
    return true;
}

const TerrainParameterDescriptor pathOnlyParameters[] = {
    {"path", "File", "Where the file is written during a build", "",
     TerrainParameterType_Path, 0.0, 0.0, 0.0, "", nullptr, 0}};

const TerrainPortDescriptor pngInputs[] = {
    {"In", "The field to write", TerrainPortType_Field, TerrainFieldSemantics_Heightfield, 0}};

const TerrainPortDescriptor pngOutputs[] = {
    {"Out", "The same field, so the export can sit mid-graph", TerrainPortType_Field,
     TerrainFieldSemantics_Heightfield, 0}};

void evaluateExportPng16(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const float* input = api->inputField(context, 0);
    float* output = api->outputField(context, 0);
    std::copy(input, input + cellCount(api, context), output);
    if (!shouldWrite(api, context)) {
        return;
    }

    try {
        formats::writeHeightfieldPng16(api->parameterText(context, 0),
                                       fieldFromContext(api, context));
    } catch (const std::exception& error) {
        api->setError(context, error.what());
    }
}

const TerrainNodeDescriptor exportPng16Descriptor = {
    "ExportPNG16",
    "Output",
    "Writes a 16-bit PNG heightmap, for other terrain tools",
    1,
    pngInputs,
    1,
    pngOutputs,
    1,
    pathOnlyParameters,
    1,
    &evaluateExportPng16};

/* --------------------------------------------------------- ExportSplat -- */

constexpr int splatChannelCount = 4;

const TerrainPortDescriptor splatExportInputs[] = {
    {"Red", "The mask packed into the red channel", TerrainPortType_Field,
     TerrainFieldSemantics_Mask, 1},
    {"Green", "The mask packed into the green channel", TerrainPortType_Field,
     TerrainFieldSemantics_Mask, 1},
    {"Blue", "The mask packed into the blue channel", TerrainPortType_Field,
     TerrainFieldSemantics_Mask, 1},
    {"Alpha", "The mask packed into the alpha channel", TerrainPortType_Field,
     TerrainFieldSemantics_Mask, 1}};

const TerrainPortDescriptor splatExportOutputs[] = {
    {"Out", "The red channel's mask, so the export can sit mid-graph",
     TerrainPortType_Field, TerrainFieldSemantics_Mask, 0}};

/// A copy per channel, because the writer wants heightfields and the context
/// only hands out raw pointers into somebody else's storage.
Heightfield layerFromPort(const TerrainContextApi* api, TerrainEvaluationContext* context,
                          int port) {
    const int size = api->width(context);
    Heightfield layer(size, size);
    layer.setSemantics(FieldSemantics::Mask);
    const float* input = api->inputField(context, port);
    std::copy(input, input + layer.sampleCount(), layer.data());
    return layer;
}

void evaluateExportSplat(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const std::size_t count = cellCount(api, context);
    float* output = api->outputField(context, 0);
    const float* red = api->inputField(context, 0);
    if (red == nullptr) {
        std::fill(output, output + count, 0.0f);
    } else {
        std::copy(red, red + count, output);
    }
    if (!shouldWrite(api, context)) {
        return;
    }

    std::vector<Heightfield> layers;
    std::array<const Heightfield*, splatChannelCount> pointers{};
    for (int channel = 0; channel < splatChannelCount; ++channel) {
        if (api->isInputConnected(context, channel) == 0) {
            continue;
        }
        layers.push_back(layerFromPort(api, context, channel));
    }

    // The vector is filled first so no pointer is taken into storage that a
    // later push_back would move.
    std::size_t next = 0;
    for (int channel = 0; channel < splatChannelCount; ++channel) {
        if (api->isInputConnected(context, channel) == 0) {
            continue;
        }
        pointers[channel] = &layers[next];
        next += 1;
    }

    try {
        formats::writeSplatPng(api->parameterText(context, 0), pointers);
    } catch (const std::exception& error) {
        api->setError(context, error.what());
    }
}

const TerrainNodeDescriptor exportSplatDescriptor = {
    "ExportSplat",
    "Output",
    "Packs up to four masks into the channels of one 8-bit PNG",
    1,
    splatExportInputs,
    splatChannelCount,
    splatExportOutputs,
    1,
    pathOnlyParameters,
    1,
    &evaluateExportSplat};

/* --------------------------------------------------------- ExportColor -- */

const TerrainPortDescriptor colorExportInputs[] = {
    {"In", "The colour map to write", TerrainPortType_Color, TerrainFieldSemantics_Heightfield,
     0}};

const TerrainPortDescriptor colorExportOutputs[] = {
    {"Out", "The same colour map, so the export can sit mid-graph", TerrainPortType_Color,
     TerrainFieldSemantics_Heightfield, 0}};

void evaluateExportColor(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const int size = api->width(context);
    const std::size_t sampleCount = cellCount(api, context) * ColorField::channelCount;
    const float* input = api->inputColor(context, 0);
    std::copy(input, input + sampleCount, api->outputColor(context, 0));
    if (!shouldWrite(api, context)) {
        return;
    }

    ColorField colors(size, size);
    std::copy(input, input + sampleCount, colors.data());
    try {
        formats::writeColorPng(api->parameterText(context, 0), colors);
    } catch (const std::exception& error) {
        api->setError(context, error.what());
    }
}

const TerrainNodeDescriptor exportColorDescriptor = {
    "ExportColor",
    "Output",
    "Writes a colour map as an 8-bit RGB PNG",
    1,
    colorExportInputs,
    1,
    colorExportOutputs,
    1,
    pathOnlyParameters,
    1,
    &evaluateExportColor};

/* -------------------------------------------------------- ExportPoints -- */

const TerrainPortDescriptor pointsExportInputs[] = {
    {"In", "The instances to write", TerrainPortType_Points, TerrainFieldSemantics_Heightfield,
     0}};

const TerrainPortDescriptor pointsExportOutputs[] = {
    {"Out", "The same instances, so the export can sit mid-graph", TerrainPortType_Points,
     TerrainFieldSemantics_Heightfield, 0}};

const TerrainEnumOption pointFormats[] = {{"csv", "CSV", 0}, {"json", "JSON", 1}};

const TerrainParameterDescriptor pointsParameters[] = {
    {"path", "File", "Where the file is written during a build", "",
     TerrainParameterType_Path, 0.0, 0.0, 0.0, "", nullptr, 0},
    {"format", "Format", "A plain table or a JSON document", "", TerrainParameterType_Enum, 0.0,
     0.0, 1.0, "", pointFormats, 2}};

void writePointSet(const TerrainContextApi* api, TerrainEvaluationContext* context,
                   const PointSet& points) {
    const GeoReference georeference{api->coordinateSystem(context),
                                    api->eastingMinMeters(context),
                                    api->northingMaxMeters(context)};
    const std::filesystem::path path(api->parameterText(context, 0));
    if (static_cast<int>(api->parameterNumber(context, 1)) == 1) {
        formats::writePointsJson(path, points, georeference);
        return;
    }
    formats::writePointsCsv(path, points, georeference);
}

void evaluateExportPoints(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const int count = api->inputPointCount(context, 0);
    const TerrainPoint* input = api->inputPoints(context, 0);

    PointSet passed;
    for (int index = 0; index < count; ++index) {
        api->emitPoint(context, 0, &input[index]);
        passed.add(input[index]);
    }
    if (!shouldWrite(api, context)) {
        return;
    }

    try {
        writePointSet(api, context, passed);
    } catch (const std::exception& error) {
        api->setError(context, error.what());
    }
}

const TerrainNodeDescriptor exportPointsDescriptor = {
    "ExportPoints",
    "Output",
    "Writes scattered instances as a table a consumer can instance from",
    1,
    pointsExportInputs,
    1,
    pointsExportOutputs,
    1,
    pointsParameters,
    2,
    &evaluateExportPoints};

}  // namespace

void registerOutputNodes(const TerrainRegistryApi& api, TerrainRegistry* registry) {
    api.registerNode(registry, &exportDescriptor);
    api.registerNode(registry, &exportPng16Descriptor);
    api.registerNode(registry, &exportSplatDescriptor);
    api.registerNode(registry, &exportColorDescriptor);
    api.registerNode(registry, &exportPointsDescriptor);
}

}  // namespace terrain::nodes
