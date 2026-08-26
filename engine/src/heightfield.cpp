#include "terrain/heightfield.h"

#include <algorithm>
#include <stdexcept>

namespace terrain {

Heightfield::Heightfield(int width, int height) : width_(width), height_(height) {
    if (width <= 0 || height <= 0) {
        throw std::invalid_argument("Heightfield dimensions must be positive");
    }
    samples_.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), 0.0f);
}

float& Heightfield::at(int row, int column) {
    return samples_[static_cast<std::size_t>(row) * static_cast<std::size_t>(width_) + column];
}

float Heightfield::at(int row, int column) const {
    return samples_[static_cast<std::size_t>(row) * static_cast<std::size_t>(width_) + column];
}

void Heightfield::fill(float value) {
    std::fill(samples_.begin(), samples_.end(), value);
}

float Heightfield::minimum() const {
    if (samples_.empty()) {
        return 0.0f;
    }
    return *std::min_element(samples_.begin(), samples_.end());
}

float Heightfield::maximum() const {
    if (samples_.empty()) {
        return 0.0f;
    }
    return *std::max_element(samples_.begin(), samples_.end());
}

}  // namespace terrain
