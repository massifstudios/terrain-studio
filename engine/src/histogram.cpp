#include "terrain/histogram.h"

#include <algorithm>
#include <cstdint>

namespace terrain {
namespace {

int binOf(float value, float minimum, float span) {
    const float position = (value - minimum) / span * Histogram::binCount;
    return std::clamp(static_cast<int>(position), 0, Histogram::binCount - 1);
}

}  // namespace

Histogram::Histogram(const float* values, std::size_t count) {
    cumulative_.assign(binCount + 1, 0.0f);
    if (count == 0) {
        return;
    }

    const auto bounds = std::minmax_element(values, values + count);
    minimum_ = *bounds.first;
    maximum_ = *bounds.second;
    if (isFlat()) {
        std::fill(cumulative_.begin() + 1, cumulative_.end(), 1.0f);
        return;
    }

    std::vector<std::uint64_t> counts(binCount, 0);
    const float span = maximum_ - minimum_;
    for (std::size_t index = 0; index < count; ++index) {
        counts[static_cast<std::size_t>(binOf(values[index], minimum_, span))] += 1;
    }

    std::uint64_t running = 0;
    for (int bin = 0; bin < binCount; ++bin) {
        running += counts[static_cast<std::size_t>(bin)];
        cumulative_[static_cast<std::size_t>(bin) + 1] =
            static_cast<float>(static_cast<double>(running) / static_cast<double>(count));
    }
}

bool Histogram::isFlat() const {
    return maximum_ - minimum_ <= 0.0f;
}

float Histogram::valueAtBinEdge(std::size_t edge) const {
    const float position =
        static_cast<float>(std::min<std::size_t>(edge, binCount)) / static_cast<float>(binCount);
    return minimum_ + position * (maximum_ - minimum_);
}

float Histogram::valueAtQuantile(float fraction) const {
    const float target = std::clamp(fraction, 0.0f, 1.0f);
    const auto reached = std::lower_bound(cumulative_.begin(), cumulative_.end(), target);
    return valueAtBinEdge(static_cast<std::size_t>(reached - cumulative_.begin()));
}

float Histogram::quantileOfValue(float value) const {
    if (isFlat()) {
        return 0.0f;
    }
    const float position =
        std::clamp((value - minimum_) / (maximum_ - minimum_), 0.0f, 1.0f) * binCount;
    const int lower = std::min(static_cast<int>(position), binCount - 1);
    const float within = position - static_cast<float>(lower);
    const float low = cumulative_[static_cast<std::size_t>(lower)];
    const float high = cumulative_[static_cast<std::size_t>(lower) + 1];
    return low + (high - low) * within;
}

}  // namespace terrain
