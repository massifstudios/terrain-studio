#include "erosion_support.h"

#include <algorithm>
#include <cmath>

namespace terrain::nodes {

FixedPointGrid::FixedPointGrid(std::size_t count) : cells_(count, 0) {}

void FixedPointGrid::add(std::size_t index, float meters) {
    const auto units = static_cast<std::int32_t>(std::lround(meters * unitsPerMeter));
    if (units == 0) {
        return;
    }
    std::atomic_ref<std::int32_t>(cells_[index]).fetch_add(units, std::memory_order_relaxed);
}

float FixedPointGrid::valueAt(std::size_t index) const {
    return metersFor(cells_[index]);
}

float FixedPointGrid::metersFor(std::int32_t units) {
    return static_cast<float>(units) / unitsPerMeter;
}

void FixedPointGrid::clear() {
    std::fill(cells_.begin(), cells_.end(), 0);
}

float FixedPointGrid::strongest() const {
    std::int32_t highest = 0;
    for (std::int32_t cell : cells_) {
        highest = std::max(highest, std::abs(cell));
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
