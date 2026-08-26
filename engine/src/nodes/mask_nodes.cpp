#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

#include "node_support.h"

namespace terrain::nodes {
namespace {

const TerrainPortDescriptor maskInputs[] = {
    {"In", "The surface to measure", TerrainPortType_Field, TerrainFieldSemantics_Heightfield, 0}};

const TerrainPortDescriptor maskOutputs[] = {
    {"Out", "Weights in 0 to 1", TerrainPortType_Field, TerrainFieldSemantics_Mask, 0}};

/* ------------------------------------------------------------ SlopeMask -- */

const TerrainParameterDescriptor slopeParameters[] = {
    {"minimumSlope", "From", "Ground at least this steep is selected", "deg",
     TerrainParameterType_Float, 30.0, 0.0, 90.0, "", nullptr, 0},
    {"maximumSlope", "To", "Ground steeper than this is dropped again", "deg",
     TerrainParameterType_Float, 90.0, 0.0, 90.0, "", nullptr, 0},
    {"falloff", "Falloff", "How wide the soft edge on either side is", "deg",
     TerrainParameterType_Float, 5.0, 0.0, 90.0, "", nullptr, 0}};

void evaluateSlopeMask(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const int size = api->width(context);
    const double cellSize = api->cellSizeMeters(context);
    const float* input = api->inputField(context, 0);
    float* output = api->outputField(context, 0);
    const float low = static_cast<float>(api->parameterNumber(context, 0));
    const float high = static_cast<float>(api->parameterNumber(context, 1));
    const float falloff = static_cast<float>(api->parameterNumber(context, 2));

    forEachRowRange(api, context, size, [&](int firstRow, int lastRow) {
        for (int row = firstRow; row < lastRow; ++row) {
            float* line = output + static_cast<std::size_t>(row) * size;
            for (int column = 0; column < size; ++column) {
                const float degrees =
                    slopeDegrees(sampleGradient(input, size, row, column, cellSize));
                line[column] = bandWeight(degrees, std::min(low, high), std::max(low, high),
                                          falloff);
            }
        }
    });
}

const TerrainNodeDescriptor slopeMaskDescriptor = {
    "SlopeMask",
    "Mask",
    "Selects ground by how steep it is",
    1,
    maskInputs,
    1,
    maskOutputs,
    1,
    slopeParameters,
    3,
    &evaluateSlopeMask};

/* ----------------------------------------------------------- HeightMask -- */

const TerrainParameterDescriptor heightParameters[] = {
    {"minimumAltitude", "From", "The bottom of the selected band", "m",
     TerrainParameterType_Float, 1500.0, -12000.0, 12000.0, "", nullptr, 0},
    {"maximumAltitude", "To", "The top of the selected band", "m", TerrainParameterType_Float,
     3000.0, -12000.0, 12000.0, "", nullptr, 0},
    {"falloff", "Falloff", "How far the band fades out above and below", "m",
     TerrainParameterType_Float, 150.0, 0.0, 5000.0, "", nullptr, 0}};

void evaluateHeightMask(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const std::size_t count = cellCount(api, context);
    const float* input = api->inputField(context, 0);
    float* output = api->outputField(context, 0);
    const float low = static_cast<float>(api->parameterNumber(context, 0));
    const float high = static_cast<float>(api->parameterNumber(context, 1));
    const float falloff = static_cast<float>(api->parameterNumber(context, 2));

    for (std::size_t index = 0; index < count; ++index) {
        output[index] =
            bandWeight(input[index], std::min(low, high), std::max(low, high), falloff);
    }
}

const TerrainNodeDescriptor heightMaskDescriptor = {
    "HeightMask",
    "Mask",
    "Selects an altitude band, such as a treeline or a snowline",
    1,
    maskInputs,
    1,
    maskOutputs,
    1,
    heightParameters,
    3,
    &evaluateHeightMask};

/* -------------------------------------------------------- CurvatureMask -- */

const TerrainEnumOption curvatureModes[] = {
    {"concave", "Hollows", 0}, {"convex", "Rises", 1}, {"either", "Either", 2}};

const TerrainParameterDescriptor curvatureParameters[] = {
    {"mode", "Selects", "Hollows collect, rises shed; either picks both", "",
     TerrainParameterType_Enum, 0.0, 0.0, 2.0, "", curvatureModes, 3},
    {"strength", "Strength", "Raising this widens the selection into gentler ground", "",
     TerrainParameterType_Float, 4.0, 0.1, 50.0, "", nullptr, 0}};

/// Selected sign of the curvature, before it is scaled and clipped.
float curvatureSide(float curvature, int mode) {
    if (mode == 1) {
        return curvature;
    }
    if (mode == 2) {
        return std::abs(curvature);
    }
    return -curvature;
}

void evaluateCurvatureMask(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const int size = api->width(context);
    const std::size_t count = cellCount(api, context);
    const double cellSize = api->cellSizeMeters(context);
    const float* input = api->inputField(context, 0);
    float* output = api->outputField(context, 0);
    const int mode = static_cast<int>(api->parameterNumber(context, 0));
    const float strength = static_cast<float>(api->parameterNumber(context, 1));

    std::vector<float> curvature(count);
    forEachRowRange(api, context, size, [&](int firstRow, int lastRow) {
        for (int row = firstRow; row < lastRow; ++row) {
            for (int column = 0; column < size; ++column) {
                curvature[static_cast<std::size_t>(row) * size + column] =
                    sampleCurvature(input, size, row, column, cellSize);
            }
        }
    });

    // Curvature is in 1/m and its useful magnitude changes with the terrain,
    // so the mask is scaled against this field's own strongest bend rather
    // than against a number the artist would have to guess.
    const FieldRange range = fieldRange(curvature.data(), count);
    const float strongest =
        std::max(std::max(std::abs(range.minimum), std::abs(range.maximum)), 1.0e-12f);
    for (std::size_t index = 0; index < count; ++index) {
        output[index] = std::clamp(curvatureSide(curvature[index], mode) / strongest * strength,
                                   0.0f, 1.0f);
    }
}

const TerrainNodeDescriptor curvatureMaskDescriptor = {
    "CurvatureMask",
    "Mask",
    "Selects hollows or rises by how sharply the ground bends",
    1,
    maskInputs,
    1,
    maskOutputs,
    1,
    curvatureParameters,
    2,
    &evaluateCurvatureMask};

/* ----------------------------------------------------------- AspectMask -- */

const TerrainParameterDescriptor aspectParameters[] = {
    {"direction", "Faces", "Compass bearing the selected slopes look towards", "deg",
     TerrainParameterType_Float, 180.0, 0.0, 360.0, "", nullptr, 0},
    {"spread", "Spread", "How far either side of that bearing counts fully", "deg",
     TerrainParameterType_Float, 45.0, 0.0, 180.0, "", nullptr, 0},
    {"falloff", "Falloff", "How wide the soft edge on either side is", "deg",
     TerrainParameterType_Float, 30.0, 0.0, 180.0, "", nullptr, 0},
    {"minimumSlope", "Ignore below", "Flat ground faces nowhere, so it is left out", "deg",
     TerrainParameterType_Float, 3.0, 0.0, 90.0, "", nullptr, 0}};

/// Compass bearing the ground looks towards: downhill, so the opposite of the
/// uphill gradient. 0 is north, 90 east.
float aspectDegrees(const SurfaceGradient& gradient) {
    const float radians = std::atan2(-gradient.eastward, -gradient.northward);
    const float degrees = radians * (180.0f / std::numbers::pi_v<float>);
    if (degrees < 0.0f) {
        return degrees + 360.0f;
    }
    return degrees;
}

float bearingDifference(float left, float right) {
    const float raw = std::abs(left - right);
    return std::min(raw, 360.0f - raw);
}

void evaluateAspectMask(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const int size = api->width(context);
    const double cellSize = api->cellSizeMeters(context);
    const float* input = api->inputField(context, 0);
    float* output = api->outputField(context, 0);
    const float target = static_cast<float>(api->parameterNumber(context, 0));
    const float spread = static_cast<float>(api->parameterNumber(context, 1));
    const float falloff = static_cast<float>(api->parameterNumber(context, 2));
    const float minimumSlope = static_cast<float>(api->parameterNumber(context, 3));

    forEachRowRange(api, context, size, [&](int firstRow, int lastRow) {
        for (int row = firstRow; row < lastRow; ++row) {
            float* line = output + static_cast<std::size_t>(row) * size;
            for (int column = 0; column < size; ++column) {
                const SurfaceGradient gradient =
                    sampleGradient(input, size, row, column, cellSize);
                const float facing =
                    bandWeight(bearingDifference(aspectDegrees(gradient), target), 0.0f, spread,
                               falloff);
                const float steepEnough =
                    smoothStep(slopeDegrees(gradient) / std::max(minimumSlope, 1.0e-3f));
                line[column] = facing * steepEnough;
            }
        }
    });
}

const TerrainNodeDescriptor aspectMaskDescriptor = {
    "AspectMask",
    "Mask",
    "Selects slopes by the direction they face, for sun, wind or snow",
    1,
    maskInputs,
    1,
    maskOutputs,
    1,
    aspectParameters,
    4,
    &evaluateAspectMask};

}  // namespace

void registerMaskNodes(const TerrainRegistryApi& api, TerrainRegistry* registry) {
    api.registerNode(registry, &slopeMaskDescriptor);
    api.registerNode(registry, &heightMaskDescriptor);
    api.registerNode(registry, &curvatureMaskDescriptor);
    api.registerNode(registry, &aspectMaskDescriptor);
}

}  // namespace terrain::nodes
