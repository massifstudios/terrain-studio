#include "terrain/formats/massifdem.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <vector>

#include "../binary_io.h"
#include "../file_bytes.h"

namespace terrain::formats {
namespace {

constexpr char massifDemMagic[8] = {'M', 'A', 'S', 'S', 'I', 'F', 'D', 'M'};
constexpr std::size_t coordinateSystemOffset = 64;
constexpr std::size_t coordinateSystemBytes = 32;
constexpr std::size_t labelOffset = 96;
constexpr std::size_t labelBytes = 96;
constexpr double smallestAltitudeStep = 1.0e-6;
constexpr double quantisationLevels = 65535.0;

std::string readPaddedString(const std::uint8_t* bytes, std::size_t capacity) {
    const auto* terminator = static_cast<const std::uint8_t*>(std::memchr(bytes, 0, capacity));
    if (terminator == nullptr) {
        return std::string(reinterpret_cast<const char*>(bytes), capacity);
    }
    return std::string(reinterpret_cast<const char*>(bytes),
                       static_cast<std::size_t>(terminator - bytes));
}

void writePaddedString(std::uint8_t* bytes, std::size_t capacity, std::string_view text) {
    // The reference writer truncates the label to capacity - 1 bytes.
    const std::size_t copied = std::min(text.size(), capacity - 1);
    std::memcpy(bytes, text.data(), copied);
}

void validateHeader(const std::vector<std::uint8_t>& bytes) {
    if (bytes.size() < massifDemHeaderBytes) {
        throw std::runtime_error("DEM is shorter than its header");
    }
    if (std::memcmp(bytes.data(), massifDemMagic, sizeof(massifDemMagic)) != 0) {
        throw std::runtime_error("Not a .massifdem file: bad magic");
    }
    const std::uint32_t version = binary::readLittle32(bytes.data() + 8);
    if (version != massifDemVersion) {
        throw std::runtime_error("Unsupported .massifdem version " + std::to_string(version));
    }
}

double altitudeStepFor(double minimum, double maximum) {
    return std::max((maximum - minimum) / quantisationLevels, smallestAltitudeStep);
}

}  // namespace

DemDocument readMassifDem(const std::filesystem::path& path) {
    const std::vector<std::uint8_t> bytes = io::readAllBytes(path, "DEM");
    validateHeader(bytes);

    const int width = static_cast<int>(binary::readLittle32(bytes.data() + 12));
    const int height = static_cast<int>(binary::readLittle32(bytes.data() + 16));
    const double cellSizeMeters = binary::readLittleDouble(bytes.data() + 24);
    const double minimumAltitude = binary::readLittleDouble(bytes.data() + 32);
    const double altitudeStep = binary::readLittleDouble(bytes.data() + 40);

    const std::size_t expectedBytes =
        massifDemHeaderBytes + static_cast<std::size_t>(width) * height * sizeof(std::uint16_t);
    if (bytes.size() < expectedBytes) {
        throw std::runtime_error("DEM is truncated: expected " + std::to_string(expectedBytes)
                                 + " bytes");
    }

    Heightfield field(width, height);
    field.setCellSizeMeters(cellSizeMeters);
    field.setGeoreference(GeoReference{
        readPaddedString(bytes.data() + coordinateSystemOffset, coordinateSystemBytes),
        binary::readLittleDouble(bytes.data() + 48), binary::readLittleDouble(bytes.data() + 56)});

    const std::uint8_t* samples = bytes.data() + massifDemHeaderBytes;
    std::span<float> destination = field.samples();
    for (std::size_t index = 0; index < destination.size(); ++index) {
        const std::uint16_t quantised = binary::readLittle16(samples + index * 2);
        destination[index] = static_cast<float>(minimumAltitude + quantised * altitudeStep);
    }

    return DemDocument{std::move(field),
                       readPaddedString(bytes.data() + labelOffset, labelBytes)};
}

void writeMassifDem(const std::filesystem::path& path, const Heightfield& field,
                    std::string_view label) {
    if (field.isEmpty()) {
        throw std::runtime_error("Cannot write an empty DEM");
    }

    const double minimum = field.minimum();
    const double altitudeStep = altitudeStepFor(minimum, field.maximum());

    std::vector<std::uint8_t> header(massifDemHeaderBytes, 0);
    std::memcpy(header.data(), massifDemMagic, sizeof(massifDemMagic));
    binary::writeLittle32(header.data() + 8, massifDemVersion);
    binary::writeLittle32(header.data() + 12, static_cast<std::uint32_t>(field.width()));
    binary::writeLittle32(header.data() + 16, static_cast<std::uint32_t>(field.height()));
    binary::writeLittle32(header.data() + 20, 0);
    binary::writeLittleDouble(header.data() + 24, field.cellSizeMeters());
    binary::writeLittleDouble(header.data() + 32, minimum);
    binary::writeLittleDouble(header.data() + 40, altitudeStep);
    binary::writeLittleDouble(header.data() + 48, field.georeference().eastingMinMeters);
    binary::writeLittleDouble(header.data() + 56, field.georeference().northingMaxMeters);
    writePaddedString(header.data() + coordinateSystemOffset, coordinateSystemBytes,
                      field.georeference().coordinateSystem);
    writePaddedString(header.data() + labelOffset, labelBytes, label);

    std::vector<std::uint8_t> payload(field.sampleCount() * sizeof(std::uint16_t));
    const std::span<const float> samples = field.samples();
    for (std::size_t index = 0; index < samples.size(); ++index) {
        // std::nearbyint under the default rounding mode matches numpy's rint.
        const double level = std::nearbyint((samples[index] - minimum) / altitudeStep);
        const double clamped = std::clamp(level, 0.0, quantisationLevels);
        binary::writeLittle16(payload.data() + index * 2, static_cast<std::uint16_t>(clamped));
    }

    std::ofstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Cannot write DEM: " + path.string());
    }
    file.write(reinterpret_cast<const char*>(header.data()),
               static_cast<std::streamsize>(header.size()));
    file.write(reinterpret_cast<const char*>(payload.data()),
               static_cast<std::streamsize>(payload.size()));
}

}  // namespace terrain::formats
