#include "terrain/color_ramp.h"

#include <algorithm>
#include <charconv>

namespace terrain {
namespace {

bool readHexDigit(char character, int& value) {
    if (character >= '0' && character <= '9') {
        value = character - '0';
        return true;
    }
    const char lowered = static_cast<char>(character | 0x20);
    if (lowered >= 'a' && lowered <= 'f') {
        value = lowered - 'a' + 10;
        return true;
    }
    return false;
}

bool readHexChannel(std::string_view text, std::size_t offset, float& channel) {
    int high = 0;
    int low = 0;
    if (!readHexDigit(text[offset], high) || !readHexDigit(text[offset + 1], low)) {
        return false;
    }
    channel = static_cast<float>(high * 16 + low) / 255.0f;
    return true;
}

bool readColor(std::string_view text, RgbColor& color) {
    if (text.size() != 7 || text.front() != '#') {
        return false;
    }
    return readHexChannel(text, 1, color.red) && readHexChannel(text, 3, color.green)
           && readHexChannel(text, 5, color.blue);
}

bool readStop(std::string_view text, ColorRamp::Stop& stop) {
    const std::size_t colon = text.find(':');
    if (colon == std::string_view::npos) {
        return false;
    }
    const std::string_view position = text.substr(0, colon);
    const auto read = std::from_chars(position.data(), position.data() + position.size(),
                                      stop.position);
    if (read.ec != std::errc{}) {
        return false;
    }
    stop.position = std::clamp(stop.position, 0.0f, 1.0f);
    return readColor(text.substr(colon + 1), stop.color);
}

float mixChannel(float low, float high, float weight) {
    return low + (high - low) * weight;
}

}  // namespace

ColorRamp ColorRamp::parse(std::string_view text) {
    ColorRamp ramp;
    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t separator = std::min(text.find(';', start), text.size());
        Stop stop;
        if (readStop(text.substr(start, separator - start), stop)) {
            ramp.stops_.push_back(stop);
        }
        start = separator + 1;
    }

    std::stable_sort(ramp.stops_.begin(), ramp.stops_.end(),
                     [](const Stop& left, const Stop& right) {
                         return left.position < right.position;
                     });
    if (ramp.stops_.empty()) {
        ramp.stops_ = {Stop{0.0f, RgbColor{0.0f, 0.0f, 0.0f}},
                       Stop{1.0f, RgbColor{1.0f, 1.0f, 1.0f}}};
    }
    return ramp;
}

RgbColor ColorRamp::sample(float position) const {
    if (position <= stops_.front().position) {
        return stops_.front().color;
    }
    if (position >= stops_.back().position) {
        return stops_.back().color;
    }

    const auto above =
        std::upper_bound(stops_.begin(), stops_.end(), position,
                         [](float value, const Stop& stop) { return value < stop.position; });
    const Stop& high = *above;
    const Stop& low = *(above - 1);
    const float width = high.position - low.position;
    if (width <= 0.0f) {
        return high.color;
    }

    const float weight = (position - low.position) / width;
    return RgbColor{mixChannel(low.color.red, high.color.red, weight),
                    mixChannel(low.color.green, high.color.green, weight),
                    mixChannel(low.color.blue, high.color.blue, weight)};
}

}  // namespace terrain
