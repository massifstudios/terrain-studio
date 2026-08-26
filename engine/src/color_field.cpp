#include "terrain/color_field.h"

namespace terrain {

ColorField::ColorField(int width, int height)
    : width_(width),
      height_(height),
      samples_(static_cast<std::size_t>(width) * height * channelCount, 0.0f) {}

float* ColorField::at(int row, int column) {
    return samples_.data() + (static_cast<std::size_t>(row) * width_ + column) * channelCount;
}

const float* ColorField::at(int row, int column) const {
    return samples_.data() + (static_cast<std::size_t>(row) * width_ + column) * channelCount;
}

}  // namespace terrain
