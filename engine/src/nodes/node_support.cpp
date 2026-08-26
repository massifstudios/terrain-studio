#include "node_support.h"

#include <algorithm>

namespace terrain::nodes {

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

}  // namespace terrain::nodes
