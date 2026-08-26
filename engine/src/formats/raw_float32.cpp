#include "terrain/formats/raw_float32.h"

#include <fstream>
#include <stdexcept>
#include <vector>

#include "../binary_io.h"
#include "../file_bytes.h"

namespace terrain::formats {

Heightfield readRawFloat32(const std::filesystem::path& path, const RawFloat32Layout& layout) {
    if (layout.width <= 0 || layout.height <= 0) {
        throw std::invalid_argument("Raw float32 import needs a positive width and height");
    }

    const std::vector<std::uint8_t> bytes = io::readAllBytes(path, "raw float32 file");

    const std::size_t expected =
        static_cast<std::size_t>(layout.width) * layout.height * sizeof(float);
    if (bytes.size() < expected) {
        throw std::runtime_error("Raw float32 file is smaller than the stated dimensions");
    }

    Heightfield field(layout.width, layout.height);
    field.setCellSizeMeters(layout.cellSizeMeters);
    std::span<float> destination = field.samples();
    for (std::size_t index = 0; index < destination.size(); ++index) {
        destination[index] = binary::readLittleFloat(bytes.data() + index * sizeof(float));
    }
    return field;
}

}  // namespace terrain::formats
