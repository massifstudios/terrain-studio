#include <algorithm>
#include <cmath>
#include <exception>
#include <numbers>

#include "node_support.h"
#include "shape_support.h"
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

const TerrainPortDescriptor gradientOutputs[] = {
    {"Out", "A field that rises evenly across the world", TerrainPortType_Field,
     TerrainFieldSemantics_Heightfield, 0}};

const TerrainParameterDescriptor gradientParameters[] = {
    {"direction", "Direction", "Compass bearing the field rises towards; 0 is north", "deg",
     TerrainParameterType_Float, 0.0, 0.0, 360.0, "", nullptr, 0},
    {"startValue", "Start", "The value at the low end", "m", TerrainParameterType_Float, 0.0,
     -12000.0, 12000.0, "", nullptr, 0},
    {"endValue", "End", "The value at the high end", "m", TerrainParameterType_Float, 1000.0,
     -12000.0, 12000.0, "", nullptr, 0}};

/// Distance along the ramp direction, measured from the corner where the ramp
/// is lowest, as [0, 1] across the world square.
struct RampAxis {
    double eastward = 0.0;
    double northward = 1.0;
    double lowest = 0.0;
    double span = 1.0;

    double positionAt(double eastMeters, double northMeters) const {
        return (eastMeters * eastward + northMeters * northward - lowest) / span;
    }
};

RampAxis rampAxisFor(double directionDegrees, double worldSizeMeters) {
    const double radians = directionDegrees * std::numbers::pi / 180.0;
    RampAxis axis;
    axis.eastward = std::sin(radians);
    axis.northward = std::cos(radians);

    double lowest = 0.0;
    double highest = 0.0;
    for (const double east : {0.0, worldSizeMeters}) {
        for (const double north : {0.0, worldSizeMeters}) {
            const double projection = east * axis.eastward + north * axis.northward;
            lowest = std::min(lowest, projection);
            highest = std::max(highest, projection);
        }
    }
    axis.lowest = lowest;
    axis.span = std::max(highest - lowest, 1.0e-9);
    return axis;
}

void evaluateGradient(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const int size = api->width(context);
    const double cellSize = api->cellSizeMeters(context);
    const RampAxis axis = rampAxisFor(api->parameterNumber(context, 0), size * cellSize);
    const double startValue = api->parameterNumber(context, 1);
    const double endValue = api->parameterNumber(context, 2);
    float* output = api->outputField(context, 0);

    for (int row = 0; row < size; ++row) {
        // Row 0 is north, so northing counts down from the top edge.
        const double northMeters = (size - row - 0.5) * cellSize;
        float* line = output + static_cast<std::size_t>(row) * size;
        for (int column = 0; column < size; ++column) {
            const double unit = axis.positionAt((column + 0.5) * cellSize, northMeters);
            line[column] = static_cast<float>(startValue + (endValue - startValue) * unit);
        }
    }
}

const TerrainNodeDescriptor gradientDescriptor = {
    "Gradient",
    "Input",
    "A straight ramp across the world, for tilting or masking by direction",
    1,
    nullptr,
    0,
    gradientOutputs,
    1,
    gradientParameters,
    3,
    &evaluateGradient};

/* ------------------------------------------------------- RadialGradient -- */

const TerrainPortDescriptor radialOutputs[] = {
    {"Out", "A field that falls away from a centre", TerrainPortType_Field,
     TerrainFieldSemantics_Heightfield, 0}};

const TerrainEnumOption radialProfiles[] = {
    {"linear", "Linear", 0}, {"dome", "Dome", 1}, {"smooth", "Smooth", 2}};

const TerrainParameterDescriptor radialParameters[] = {
    {"centreEast", "Centre east", "Where the centre sits across the world, 0 at the west edge", "",
     TerrainParameterType_Float, 0.5, 0.0, 1.0, "", nullptr, 0},
    {"centreNorth", "Centre north", "Where the centre sits up the world, 0 at the south edge", "",
     TerrainParameterType_Float, 0.5, 0.0, 1.0, "", nullptr, 0},
    {"radius", "Radius", "Ground distance from the centre out to the offset value", "m",
     TerrainParameterType_Float, 2000.0, 1.0, 100000.0, "", nullptr, 0},
    {"profile", "Profile", "Linear falls off as a cone, dome as a hemisphere, smooth as a bell",
     "", TerrainParameterType_Enum, 2.0, 0.0, 2.0, "", radialProfiles, 3},
    {"amplitude", "Amplitude", "How far above the offset the centre stands", "m",
     TerrainParameterType_Float, 500.0, -12000.0, 12000.0, "", nullptr, 0},
    {"offset", "Offset", "The value at the radius and everywhere beyond it", "m",
     TerrainParameterType_Float, 0.0, -12000.0, 12000.0, "", nullptr, 0}};

void evaluateRadialGradient(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const WorldFrame frame = worldFrameFor(api, context);
    const ShapePlacement placement = placementFrom(frame, api->parameterNumber(context, 0),
                                                   api->parameterNumber(context, 1), 0.0);
    const double radiusMeters = api->parameterNumber(context, 2);
    const auto profile =
        static_cast<RadialProfile>(static_cast<int>(api->parameterNumber(context, 3)));
    const float amplitude = static_cast<float>(api->parameterNumber(context, 4));
    const float offset = static_cast<float>(api->parameterNumber(context, 5));
    float* output = api->outputField(context, 0);

    forEachRowRange(api, context, frame.size, [&](int firstRow, int lastRow) {
        for (int row = firstRow; row < lastRow; ++row) {
            float* line = output + static_cast<std::size_t>(row) * frame.size;
            for (int column = 0; column < frame.size; ++column) {
                const LocalPoint point =
                    placement.localPoint(frame.eastingAt(column), frame.northingAt(row));
                const float value =
                    radialProfileValue(profile, point.distanceFromCentre(), radiusMeters);
                line[column] = value * amplitude + offset;
            }
        }
    });
}

const TerrainNodeDescriptor radialGradientDescriptor = {
    "RadialGradient",
    "Input",
    "A field falling away from a point: a cone, a dome, or a soft bell",
    1,
    nullptr,
    0,
    radialOutputs,
    1,
    radialParameters,
    6,
    &evaluateRadialGradient};

/* ------------------------------------------------------------------ Shape -- */

const TerrainPortDescriptor shapeOutputs[] = {
    {"Out", "The shape, raised out of the offset", TerrainPortType_Field,
     TerrainFieldSemantics_Heightfield, 0}};

const TerrainEnumOption shapeKinds[] = {
    {"disc", "Disc", 0}, {"square", "Square", 1}, {"ring", "Ring", 2}, {"polygon", "Polygon", 3}};

const TerrainParameterDescriptor shapeParameters[] = {
    {"shape", "Shape", "Which outline is drawn", "", TerrainParameterType_Enum, 0.0, 0.0, 3.0, "",
     shapeKinds, 4},
    {"centreEast", "Centre east", "Where the centre sits across the world, 0 at the west edge", "",
     TerrainParameterType_Float, 0.5, 0.0, 1.0, "", nullptr, 0},
    {"centreNorth", "Centre north", "Where the centre sits up the world, 0 at the south edge", "",
     TerrainParameterType_Float, 0.5, 0.0, 1.0, "", nullptr, 0},
    {"radius", "Radius", "Half the width of a square, the radius of anything else", "m",
     TerrainParameterType_Float, 2000.0, 1.0, 100000.0, "", nullptr, 0},
    {"rotation", "Rotation", "How far the shape is turned clockwise", "deg",
     TerrainParameterType_Float, 0.0, 0.0, 360.0, "", nullptr, 0},
    {"sideCount", "Sides", "Polygon only: how many sides it has", "", TerrainParameterType_Int,
     6.0, 3.0, 64.0, "", nullptr, 0},
    {"ringWidth", "Ring width", "Ring only: how thick the band is", "m",
     TerrainParameterType_Float, 200.0, 1.0, 100000.0, "", nullptr, 0},
    {"edgeFalloff", "Edge falloff", "How far outside the edge the shape fades to nothing", "m",
     TerrainParameterType_Float, 100.0, 0.0, 100000.0, "", nullptr, 0},
    {"amplitude", "Amplitude", "How far above the offset the inside of the shape stands", "m",
     TerrainParameterType_Float, 500.0, -12000.0, 12000.0, "", nullptr, 0},
    {"offset", "Offset", "The value outside the shape", "m", TerrainParameterType_Float, 0.0,
     -12000.0, 12000.0, "", nullptr, 0}};

struct ShapeOutline {
    int kind = 0;
    double radiusMeters = 0.0;
    double ringWidthMeters = 0.0;
    int sideCount = 3;

    double distanceAt(const LocalPoint& point) const {
        if (kind == 1) {
            return squareDistance(point, radiusMeters);
        }
        if (kind == 2) {
            return ringDistance(point, radiusMeters, ringWidthMeters);
        }
        if (kind == 3) {
            return polygonDistance(point, radiusMeters, sideCount);
        }
        return discDistance(point, radiusMeters);
    }
};

void evaluateShape(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const WorldFrame frame = worldFrameFor(api, context);
    const ShapePlacement placement =
        placementFrom(frame, api->parameterNumber(context, 1), api->parameterNumber(context, 2),
                      api->parameterNumber(context, 4));
    const ShapeOutline outline{static_cast<int>(api->parameterNumber(context, 0)),
                               api->parameterNumber(context, 3), api->parameterNumber(context, 6),
                               static_cast<int>(api->parameterNumber(context, 5))};
    const double falloffMeters = api->parameterNumber(context, 7);
    const float amplitude = static_cast<float>(api->parameterNumber(context, 8));
    const float offset = static_cast<float>(api->parameterNumber(context, 9));
    float* output = api->outputField(context, 0);

    forEachRowRange(api, context, frame.size, [&](int firstRow, int lastRow) {
        for (int row = firstRow; row < lastRow; ++row) {
            float* line = output + static_cast<std::size_t>(row) * frame.size;
            for (int column = 0; column < frame.size; ++column) {
                const LocalPoint point =
                    placement.localPoint(frame.eastingAt(column), frame.northingAt(row));
                const float weight = insideWeight(outline.distanceAt(point), falloffMeters);
                line[column] = weight * amplitude + offset;
            }
        }
    });
}

const TerrainNodeDescriptor shapeDescriptor = {
    "Shape",
    "Input",
    "Draws a disc, square, ring or regular polygon as a field",
    1,
    nullptr,
    0,
    shapeOutputs,
    1,
    shapeParameters,
    10,
    &evaluateShape};

}  // namespace

void registerInputNodes(const TerrainRegistryApi& api, TerrainRegistry* registry) {
    api.registerNode(registry, &importDemDescriptor);
    api.registerNode(registry, &constantDescriptor);
    api.registerNode(registry, &gradientDescriptor);
    api.registerNode(registry, &radialGradientDescriptor);
    api.registerNode(registry, &shapeDescriptor);
}

}  // namespace terrain::nodes
