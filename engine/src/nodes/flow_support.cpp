#include "flow_support.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <vector>

namespace terrain::nodes {
namespace {

constexpr int flowNeighbourRow[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
constexpr int flowNeighbourColumn[8] = {-1, 0, 1, -1, 1, -1, 0, 1};

/// Cells from the highest down. Ties break on index so the order is a total
/// order and not something the sort implementation gets to decide.
std::vector<std::uint32_t> cellsByDescendingHeight(const float* field, std::size_t count) {
    std::vector<std::uint32_t> order(count);
    for (std::size_t index = 0; index < count; ++index) {
        order[index] = static_cast<std::uint32_t>(index);
    }
    std::sort(order.begin(), order.end(), [field](std::uint32_t left, std::uint32_t right) {
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

DownhillShare downhillFrom(const float* field, int size, int row, int column,
                           double cellSizeMeters, float concentration) {
    const float centre = field[static_cast<std::size_t>(row) * size + column];
    DownhillShare share;

    for (int direction = 0; direction < 8; ++direction) {
        const int targetRow = row + flowNeighbourRow[direction];
        const int targetColumn = column + flowNeighbourColumn[direction];
        if (targetRow < 0 || targetRow >= size || targetColumn < 0 || targetColumn >= size) {
            continue;
        }
        const std::size_t index = static_cast<std::size_t>(targetRow) * size + targetColumn;
        const float drop = centre - field[index];
        if (drop <= 0.0f) {
            continue;
        }
        float run = static_cast<float>(cellSizeMeters);
        if (flowNeighbourRow[direction] != 0 && flowNeighbourColumn[direction] != 0) {
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

}  // namespace

void accumulateFlow(const float* height, int size, double cellSizeMeters, float concentration,
                    float* accumulation) {
    const std::size_t count = static_cast<std::size_t>(size) * size;
    std::fill(accumulation, accumulation + count, 1.0f);
    for (const std::uint32_t index : cellsByDescendingHeight(height, count)) {
        const DownhillShare share =
            downhillFrom(height, size, static_cast<int>(index / size),
                         static_cast<int>(index % size), cellSizeMeters, concentration);
        if (share.total <= 0.0f) {
            continue;
        }
        for (int outlet = 0; outlet < share.count; ++outlet) {
            accumulation[share.target[outlet]] +=
                accumulation[index] * share.weight[outlet] / share.total;
        }
    }
}

void normaliseFlow(const float* accumulation, std::size_t count, float contrast,
                   float* destination) {
    const float highest = *std::max_element(accumulation, accumulation + count);
    // Flow spans orders of magnitude between a hilltop and a valley floor, so
    // it is shown on a log scale; a linear one would be black everywhere but
    // the main channel.
    const float scale = 1.0f / std::log1p(highest);
    for (std::size_t index = 0; index < count; ++index) {
        const float unit = std::clamp(std::log1p(accumulation[index]) * scale, 0.0f, 1.0f);
        destination[index] = std::pow(unit, 1.0f / contrast);
    }
}

}  // namespace terrain::nodes
