#include "node_support.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace terrain::nodes {
namespace {

/// A fixed lane count, deliberately unrelated to the core count: it decides
/// which rows go together, and that must not vary between machines.
constexpr int rowLaneCount = 64;

struct RowLaneWork {
    int rowCount = 0;
    const std::function<void(int, int)>* body = nullptr;
};

void runRowLane(void* userData, std::int32_t laneIndex, std::int32_t laneCount) {
    const RowLaneWork& work = *static_cast<const RowLaneWork*>(userData);
    const int firstRow = static_cast<int>(static_cast<long long>(work.rowCount) * laneIndex
                                          / laneCount);
    const int lastRow = static_cast<int>(static_cast<long long>(work.rowCount) * (laneIndex + 1)
                                         / laneCount);
    if (firstRow < lastRow) {
        (*work.body)(firstRow, lastRow);
    }
}

int clampIndex(int value, int size) {
    return std::clamp(value, 0, size - 1);
}

float sampleClamped(const float* field, int size, int row, int column) {
    return field[static_cast<std::size_t>(clampIndex(row, size)) * size + clampIndex(column, size)];
}

}  // namespace

void blendThroughMask(const float* original, const float* filtered, const float* mask,
                      float* destination, std::size_t count) {
    if (mask == nullptr) {
        std::copy(filtered, filtered + count, destination);
        return;
    }
    for (std::size_t index = 0; index < count; ++index) {
        const float weight = std::clamp(mask[index], 0.0f, 1.0f);
        destination[index] = original[index] + (filtered[index] - original[index]) * weight;
    }
}

void forEachRowRange(const TerrainContextApi* api, TerrainEvaluationContext* context, int rowCount,
                     const std::function<void(int firstRow, int lastRow)>& body) {
    RowLaneWork work{rowCount, &body};
    api->runLanes(context, std::min(rowLaneCount, std::max(rowCount, 1)), &runRowLane, &work);
}

float FieldRange::span() const {
    return maximum - minimum;
}

float FieldRange::normalise(float value) const {
    const float width = span();
    if (width <= 0.0f) {
        return 0.0f;
    }
    return std::clamp((value - minimum) / width, 0.0f, 1.0f);
}

float FieldRange::denormalise(float unit) const {
    return minimum + unit * span();
}

FieldRange fieldRange(const float* values, std::size_t count) {
    if (count == 0) {
        return FieldRange{};
    }
    const auto bounds = std::minmax_element(values, values + count);
    return FieldRange{*bounds.first, *bounds.second};
}

SurfaceGradient sampleGradient(const float* field, int size, int row, int column,
                               double cellSizeMeters) {
    const float step = static_cast<float>(cellSizeMeters);
    const float east = sampleClamped(field, size, row, column + 1);
    const float west = sampleClamped(field, size, row, column - 1);
    const float south = sampleClamped(field, size, row + 1, column);
    const float north = sampleClamped(field, size, row - 1, column);

    // The divisor is the distance between the two samples, which is two cells
    // except where clamping collapsed one side against the edge.
    const float eastSpan = static_cast<float>(clampIndex(column + 1, size)
                                              - clampIndex(column - 1, size));
    const float northSpan = static_cast<float>(clampIndex(row + 1, size)
                                               - clampIndex(row - 1, size));
    return SurfaceGradient{(east - west) / std::max(eastSpan, 1.0f) / step,
                           (north - south) / std::max(northSpan, 1.0f) / step};
}

float slopeDegrees(const SurfaceGradient& gradient) {
    const float steepness =
        std::sqrt(gradient.eastward * gradient.eastward + gradient.northward * gradient.northward);
    return std::atan(steepness) * (180.0f / std::numbers::pi_v<float>);
}

float sampleCurvature(const float* field, int size, int row, int column, double cellSizeMeters) {
    const float centre = sampleClamped(field, size, row, column);
    const float sum = sampleClamped(field, size, row, column + 1)
                      + sampleClamped(field, size, row, column - 1)
                      + sampleClamped(field, size, row + 1, column)
                      + sampleClamped(field, size, row - 1, column);
    const float step = static_cast<float>(cellSizeMeters);
    // The Laplacian is positive where the cell sits below its neighbours; the
    // sign is turned so a bend reads positive over a rise, as documented.
    return (4.0f * centre - sum) / (step * step);
}

float smoothStep(float edge) {
    const float t = std::clamp(edge, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float bandWeight(float value, float low, float high, float falloff) {
    if (value >= low && value <= high) {
        return 1.0f;
    }
    if (falloff <= 0.0f) {
        return 0.0f;
    }
    if (value < low) {
        return smoothStep(1.0f - (low - value) / falloff);
    }
    return smoothStep(1.0f - (value - high) / falloff);
}

}  // namespace terrain::nodes
