#include "terrain/formats/texture_maps.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "terrain/formats/png_image.h"

namespace terrain::formats {
namespace {

std::uint8_t toByte(float unit) {
    return static_cast<std::uint8_t>(std::lround(std::clamp(unit, 0.0f, 1.0f) * 255.0f));
}

const Heightfield* firstPresent(const std::array<const Heightfield*, 4>& layers) {
    for (const Heightfield* layer : layers) {
        if (layer != nullptr) {
            return layer;
        }
    }
    return nullptr;
}

}  // namespace

void writeSplatPng(const std::filesystem::path& path,
                   const std::array<const Heightfield*, 4>& layers) {
    const Heightfield* reference = firstPresent(layers);
    if (reference == nullptr) {
        throw std::runtime_error("A splat map needs at least one layer");
    }

    const std::size_t count = reference->sampleCount();
    std::vector<std::uint8_t> pixels(count * layers.size(), 0);
    for (std::size_t channel = 0; channel < layers.size(); ++channel) {
        const Heightfield* layer = layers[channel];
        if (layer == nullptr) {
            continue;
        }
        for (std::size_t index = 0; index < count; ++index) {
            pixels[index * layers.size() + channel] = toByte(layer->data()[index]);
        }
    }
    writePngRgba8(path, reference->width(), reference->height(), pixels);
}

void writeColorPng(const std::filesystem::path& path, const ColorField& colors) {
    std::vector<std::uint8_t> pixels(colors.samples().size());
    for (std::size_t index = 0; index < pixels.size(); ++index) {
        pixels[index] = toByte(colors.data()[index]);
    }
    writePngRgb8(path, colors.width(), colors.height(), pixels);
}

}  // namespace terrain::formats
