#include "erosion_support.h"

#include <algorithm>
#include <cmath>

namespace terrain::nodes {

FixedPointGrid::FixedPointGrid(std::size_t count) : cells_(count) {
    clear();
}

void FixedPointGrid::add(std::size_t index, float meters) {
    const auto units = static_cast<std::int32_t>(std::lround(meters * unitsPerMeter));
    if (units == 0) {
        return;
    }
    cells_[index].fetch_add(units, std::memory_order_relaxed);
}

float FixedPointGrid::valueAt(std::size_t index) const {
    return static_cast<float>(cells_[index].load(std::memory_order_relaxed)) / unitsPerMeter;
}

void FixedPointGrid::clear() {
    for (std::atomic<std::int32_t>& cell : cells_) {
        cell.store(0, std::memory_order_relaxed);
    }
}

float FixedPointGrid::strongest() const {
    std::int32_t highest = 0;
    for (const std::atomic<std::int32_t>& cell : cells_) {
        highest = std::max(highest, std::abs(cell.load(std::memory_order_relaxed)));
    }
    return static_cast<float>(highest) / unitsPerMeter;
}

ErosionBrush::ErosionBrush(int radiusInCells, int gridSize) {
    const int radius = std::clamp(radiusInCells, smallestRadius, std::max(gridSize / 4, smallestRadius));
    float total = 0.0f;

    for (int offsetRow = -radius; offsetRow <= radius; ++offsetRow) {
        for (int offsetColumn = -radius; offsetColumn <= radius; ++offsetColumn) {
            const float distance = std::sqrt(static_cast<float>(offsetRow * offsetRow
                                                                + offsetColumn * offsetColumn));
            if (distance > radius) {
                continue;
            }
            // Falling to zero one cell beyond the rim, so the outermost ring
            // still carries material instead of being a wasted lap.
            const float weight = 1.0f - distance / (radius + 1);
            samples_.push_back(Sample{offsetRow, offsetColumn, weight});
            total += weight;
        }
    }

    for (Sample& sample : samples_) {
        sample.weight /= total;
    }
}

}  // namespace terrain::nodes
