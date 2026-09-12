#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <vector>

#include "node_support.h"

namespace terrain::nodes {
namespace {

const TerrainPortDescriptor maskInputs[] = {
    {"In", "The surface to measure", TerrainPortType_Field,
     TerrainFieldSemantics_Heightfield, 0}};

const TerrainPortDescriptor maskOutputs[] = {{"Out", "Weights in 0 to 1",
                                              TerrainPortType_Field,
                                              TerrainFieldSemantics_Mask, 0}};

/* ------------------------------------------------------------ SlopeMask -- */

const TerrainParameterDescriptor slopeParameters[] = {
    {"minimumSlope", "From", "Ground at least this steep is selected", "deg",
     TerrainParameterType_Float, 30.0, 0.0, 90.0, "", nullptr, 0},
    {"maximumSlope", "To", "Ground steeper than this is dropped again", "deg",
     TerrainParameterType_Float, 90.0, 0.0, 90.0, "", nullptr, 0},
    {"falloff", "Falloff", "How wide the soft edge on either side is", "deg",
     TerrainParameterType_Float, 5.0, 0.0, 90.0, "", nullptr, 0}};

void evaluateSlopeMask(const TerrainContextApi *api,
                       TerrainEvaluationContext *context) {
  const int size = api->width(context);
  const double cellSize = api->cellSizeMeters(context);
  const float *input = api->inputField(context, 0);
  float *output = api->outputField(context, 0);
  const float low = static_cast<float>(api->parameterNumber(context, 0));
  const float high = static_cast<float>(api->parameterNumber(context, 1));
  const float falloff = static_cast<float>(api->parameterNumber(context, 2));

  forEachRowRange(api, context, size, [&](int firstRow, int lastRow) {
    for (int row = firstRow; row < lastRow; ++row) {
      float *line = output + static_cast<std::size_t>(row) * size;
      for (int column = 0; column < size; ++column) {
        const float degrees =
            slopeDegrees(sampleGradient(input, size, row, column, cellSize));
        line[column] = bandWeight(degrees, std::min(low, high),
                                  std::max(low, high), falloff);
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
    {"maximumAltitude", "To", "The top of the selected band", "m",
     TerrainParameterType_Float, 3000.0, -12000.0, 12000.0, "", nullptr, 0},
    {"falloff", "Falloff", "How far the band fades out above and below", "m",
     TerrainParameterType_Float, 150.0, 0.0, 5000.0, "", nullptr, 0}};

void evaluateHeightMask(const TerrainContextApi *api,
                        TerrainEvaluationContext *context) {
  const std::size_t count = cellCount(api, context);
  const float *input = api->inputField(context, 0);
  float *output = api->outputField(context, 0);
  const float low = static_cast<float>(api->parameterNumber(context, 0));
  const float high = static_cast<float>(api->parameterNumber(context, 1));
  const float falloff = static_cast<float>(api->parameterNumber(context, 2));

  for (std::size_t index = 0; index < count; ++index) {
    output[index] = bandWeight(input[index], std::min(low, high),
                               std::max(low, high), falloff);
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
    {"strength", "Strength",
     "Raising this widens the selection into gentler ground", "",
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

void evaluateCurvatureMask(const TerrainContextApi *api,
                           TerrainEvaluationContext *context) {
  const int size = api->width(context);
  const std::size_t count = cellCount(api, context);
  const double cellSize = api->cellSizeMeters(context);
  const float *input = api->inputField(context, 0);
  float *output = api->outputField(context, 0);
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
  const float strongest = std::max(
      std::max(std::abs(range.minimum), std::abs(range.maximum)), 1.0e-12f);
  for (std::size_t index = 0; index < count; ++index) {
    output[index] =
        std::clamp(curvatureSide(curvature[index], mode) / strongest * strength,
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
    {"direction", "Faces", "Compass bearing the selected slopes look towards",
     "deg", TerrainParameterType_Float, 180.0, 0.0, 360.0, "", nullptr, 0},
    {"spread", "Spread", "How far either side of that bearing counts fully",
     "deg", TerrainParameterType_Float, 45.0, 0.0, 180.0, "", nullptr, 0},
    {"falloff", "Falloff", "How wide the soft edge on either side is", "deg",
     TerrainParameterType_Float, 30.0, 0.0, 180.0, "", nullptr, 0},
    {"minimumSlope", "Ignore below",
     "Flat ground faces nowhere, so it is left out", "deg",
     TerrainParameterType_Float, 3.0, 0.0, 90.0, "", nullptr, 0}};

/// Compass bearing the ground looks towards: downhill, so the opposite of the
/// uphill gradient. 0 is north, 90 east.
float aspectDegrees(const SurfaceGradient &gradient) {
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

void evaluateAspectMask(const TerrainContextApi *api,
                        TerrainEvaluationContext *context) {
  const int size = api->width(context);
  const double cellSize = api->cellSizeMeters(context);
  const float *input = api->inputField(context, 0);
  float *output = api->outputField(context, 0);
  const float target = static_cast<float>(api->parameterNumber(context, 0));
  const float spread = static_cast<float>(api->parameterNumber(context, 1));
  const float falloff = static_cast<float>(api->parameterNumber(context, 2));
  const float minimumSlope =
      static_cast<float>(api->parameterNumber(context, 3));

  forEachRowRange(api, context, size, [&](int firstRow, int lastRow) {
    for (int row = firstRow; row < lastRow; ++row) {
      float *line = output + static_cast<std::size_t>(row) * size;
      for (int column = 0; column < size; ++column) {
        const SurfaceGradient gradient =
            sampleGradient(input, size, row, column, cellSize);
        const float facing =
            bandWeight(bearingDifference(aspectDegrees(gradient), target), 0.0f,
                       spread, falloff);
        const float steepEnough = smoothStep(slopeDegrees(gradient) /
                                             std::max(minimumSlope, 1.0e-3f));
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

/* ------------------------------------------------------------- FlowMask -- */

const TerrainParameterDescriptor flowParameters[] = {
    {"concentration", "Concentration",
     "Low values spread water over the hillside, high values pull it into "
     "channels",
     "", TerrainParameterType_Float, 1.4, 0.5, 8.0, "", nullptr, 0},
    {"contrast", "Contrast",
     "Raises the faint headwaters towards the main channels", "",
     TerrainParameterType_Float, 1.0, 0.2, 5.0, "", nullptr, 0}};

constexpr int flowNeighbourRow[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
constexpr int flowNeighbourColumn[8] = {-1, 0, 1, -1, 1, -1, 0, 1};

/// Cells from the highest down. Ties break on index so the order is a total
/// order and not something the sort implementation gets to decide.
std::vector<std::uint32_t> cellsByDescendingHeight(const float *field,
                                                   std::size_t count) {
  std::vector<std::uint32_t> order(count);
  for (std::size_t index = 0; index < count; ++index) {
    order[index] = static_cast<std::uint32_t>(index);
  }
  std::sort(order.begin(), order.end(),
            [field](std::uint32_t left, std::uint32_t right) {
              if (field[left] != field[right]) {
                return field[left] > field[right];
              }
              return left < right;
            });
  return order;
}

struct DownhillShare {
  std::size_t target[8] = {};
  float weight[8] = {};
  int count = 0;
  float total = 0.0f;
};

/// Water leaves towards every lower neighbour at once, in proportion to how
/// steeply each one falls away. A single-direction rule would draw channels
/// one cell wide along the grid axes; this draws the branching a hillside
/// actually has.
DownhillShare downhillFrom(const float *field, int size, int row, int column,
                           double cellSizeMeters, float concentration) {
  const float centre = field[static_cast<std::size_t>(row) * size + column];
  DownhillShare share;

  for (int direction = 0; direction < 8; ++direction) {
    const int targetRow = row + flowNeighbourRow[direction];
    const int targetColumn = column + flowNeighbourColumn[direction];
    if (targetRow < 0 || targetRow >= size || targetColumn < 0 ||
        targetColumn >= size) {
      continue;
    }
    const std::size_t index =
        static_cast<std::size_t>(targetRow) * size + targetColumn;
    const float drop = centre - field[index];
    if (drop <= 0.0f) {
      continue;
    }
    float run = static_cast<float>(cellSizeMeters);
    if (flowNeighbourRow[direction] != 0 &&
        flowNeighbourColumn[direction] != 0) {
      run *= std::numbers::sqrt2_v<float>;
    }
    const float weight = std::pow(drop / run, concentration);
    share.target[share.count] = index;
    share.weight[share.count] = weight;
    share.total += weight;
    share.count += 1;
  }
  return share;
}

void writeNormalisedFlow(const std::vector<float> &accumulation,
                         float *destination, float contrast) {
  const float highest =
      *std::max_element(accumulation.begin(), accumulation.end());
  // Flow spans orders of magnitude between a hilltop and a valley floor, so
  // it is shown on a log scale; a linear one would be black everywhere but
  // the main channel.
  const float scale = 1.0f / std::log1p(highest);
  for (std::size_t index = 0; index < accumulation.size(); ++index) {
    const float unit =
        std::clamp(std::log1p(accumulation[index]) * scale, 0.0f, 1.0f);
    destination[index] = std::pow(unit, 1.0f / contrast);
  }
}

void evaluateFlowMask(const TerrainContextApi *api,
                      TerrainEvaluationContext *context) {
  const int size = api->width(context);
  const std::size_t count = cellCount(api, context);
  const double cellSize = api->cellSizeMeters(context);
  const float *input = api->inputField(context, 0);
  const float concentration =
      static_cast<float>(api->parameterNumber(context, 0));

  // Every cell starts with its own rainfall and passes on whatever reached
  // it, so working downhill visits a cell only once its inflow is complete.
  std::vector<float> accumulation(count, 1.0f);
  for (const std::uint32_t index : cellsByDescendingHeight(input, count)) {
    const DownhillShare share =
        downhillFrom(input, size, static_cast<int>(index / size),
                     static_cast<int>(index % size), cellSize, concentration);
    if (share.total <= 0.0f) {
      continue;
    }
    for (int outlet = 0; outlet < share.count; ++outlet) {
      accumulation[share.target[outlet]] +=
          accumulation[index] * share.weight[outlet] / share.total;
    }
  }

  writeNormalisedFlow(accumulation, api->outputField(context, 0),
                      static_cast<float>(api->parameterNumber(context, 1)));
}

const TerrainNodeDescriptor flowMaskDescriptor = {
    "FlowMask",
    "Mask",
    "Selects where water gathers, following it downhill across the whole "
    "surface",
    1,
    maskInputs,
    1,
    maskOutputs,
    1,
    flowParameters,
    2,
    &evaluateFlowMask};

} // namespace

void registerMaskNodes(const TerrainRegistryApi &api,
                       TerrainRegistry *registry) {
  api.registerNode(registry, &slopeMaskDescriptor);
  api.registerNode(registry, &heightMaskDescriptor);
  api.registerNode(registry, &curvatureMaskDescriptor);
  api.registerNode(registry, &aspectMaskDescriptor);
  api.registerNode(registry, &flowMaskDescriptor);
}

} // namespace terrain::nodes
