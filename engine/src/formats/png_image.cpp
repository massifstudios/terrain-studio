#include "terrain/formats/png_image.h"

#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>

#include <zlib.h>

#include "../binary_io.h"
#include "../file_bytes.h"

namespace terrain::formats {
namespace {

constexpr std::uint8_t pngSignature[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
constexpr int deflateLevel = 6;

struct ImageHeader {
    int width = 0;
    int height = 0;
    int bitDepth = 0;
    int colorType = 0;
    int channels = 0;
};

int channelsForColorType(int colorType) {
    if (colorType == 0) {
        return 1;
    }
    if (colorType == 2) {
        return 3;
    }
    if (colorType == 4) {
        return 2;
    }
    if (colorType == 6) {
        return 4;
    }
    throw std::runtime_error("Unsupported PNG colour type " + std::to_string(colorType));
}

ImageHeader parseImageHeader(std::span<const std::uint8_t> chunk) {
    if (chunk.size() < 13) {
        throw std::runtime_error("PNG header chunk is truncated");
    }
    ImageHeader header;
    header.width = static_cast<int>(binary::readBig32(chunk.data()));
    header.height = static_cast<int>(binary::readBig32(chunk.data() + 4));
    header.bitDepth = chunk[8];
    header.colorType = chunk[9];
    if (chunk[12] != 0) {
        throw std::runtime_error("Interlaced PNGs are not supported");
    }
    if (header.bitDepth != 8 && header.bitDepth != 16) {
        throw std::runtime_error("Unsupported PNG bit depth " + std::to_string(header.bitDepth));
    }
    header.channels = channelsForColorType(header.colorType);
    return header;
}

std::uint8_t paethPredictor(int left, int above, int upperLeft) {
    const int estimate = left + above - upperLeft;
    const int distanceLeft = std::abs(estimate - left);
    const int distanceAbove = std::abs(estimate - above);
    const int distanceUpperLeft = std::abs(estimate - upperLeft);
    if (distanceLeft <= distanceAbove && distanceLeft <= distanceUpperLeft) {
        return static_cast<std::uint8_t>(left);
    }
    if (distanceAbove <= distanceUpperLeft) {
        return static_cast<std::uint8_t>(above);
    }
    return static_cast<std::uint8_t>(upperLeft);
}

void undoSub(std::span<std::uint8_t> row, int bytesPerPixel) {
    for (std::size_t index = bytesPerPixel; index < row.size(); ++index) {
        row[index] = static_cast<std::uint8_t>(row[index] + row[index - bytesPerPixel]);
    }
}

void undoUp(std::span<std::uint8_t> row, std::span<const std::uint8_t> previous) {
    for (std::size_t index = 0; index < row.size(); ++index) {
        row[index] = static_cast<std::uint8_t>(row[index] + previous[index]);
    }
}

/// The byte one pixel to the left, which is zero before the row starts.
int priorByte(std::span<const std::uint8_t> bytes, std::size_t index, int bytesPerPixel) {
    if (index < static_cast<std::size_t>(bytesPerPixel)) {
        return 0;
    }
    return bytes[index - bytesPerPixel];
}

void undoAverage(std::span<std::uint8_t> row, std::span<const std::uint8_t> previous,
                 int bytesPerPixel) {
    for (std::size_t index = 0; index < row.size(); ++index) {
        const int left = priorByte(row, index, bytesPerPixel);
        row[index] = static_cast<std::uint8_t>(row[index] + ((left + previous[index]) / 2));
    }
}

void undoPaeth(std::span<std::uint8_t> row, std::span<const std::uint8_t> previous,
               int bytesPerPixel) {
    for (std::size_t index = 0; index < row.size(); ++index) {
        const int left = priorByte(row, index, bytesPerPixel);
        const int upperLeft = priorByte(previous, index, bytesPerPixel);
        row[index] =
            static_cast<std::uint8_t>(row[index] + paethPredictor(left, previous[index], upperLeft));
    }
}

void unfilterRow(std::uint8_t filterType, std::span<std::uint8_t> row,
                 std::span<const std::uint8_t> previous, int bytesPerPixel) {
    switch (filterType) {
        case 0: return;
        case 1: undoSub(row, bytesPerPixel); return;
        case 2: undoUp(row, previous); return;
        case 3: undoAverage(row, previous, bytesPerPixel); return;
        case 4: undoPaeth(row, previous, bytesPerPixel); return;
        default: throw std::runtime_error("Unknown PNG filter type");
    }
}

std::vector<std::uint8_t> inflateAll(std::span<const std::uint8_t> compressed,
                                     std::size_t expectedSize) {
    std::vector<std::uint8_t> raw(expectedSize);
    uLongf destinationSize = static_cast<uLongf>(expectedSize);
    const int status = uncompress(raw.data(), &destinationSize, compressed.data(),
                                  static_cast<uLong>(compressed.size()));
    if (status != Z_OK || destinationSize != expectedSize) {
        throw std::runtime_error("PNG image data could not be decompressed");
    }
    return raw;
}

std::vector<std::uint8_t> collectImageData(std::span<const std::uint8_t> bytes,
                                           ImageHeader& header) {
    std::vector<std::uint8_t> compressed;
    std::size_t cursor = sizeof(pngSignature);
    bool sawHeader = false;

    while (cursor + 8 <= bytes.size()) {
        const std::uint32_t length = binary::readBig32(bytes.data() + cursor);
        const std::span<const std::uint8_t> chunk = bytes.subspan(cursor + 8, length);
        const char* kind = reinterpret_cast<const char*>(bytes.data() + cursor + 4);
        if (std::memcmp(kind, "IHDR", 4) == 0) {
            header = parseImageHeader(chunk);
            sawHeader = true;
        } else if (std::memcmp(kind, "IDAT", 4) == 0) {
            compressed.insert(compressed.end(), chunk.begin(), chunk.end());
        } else if (std::memcmp(kind, "IEND", 4) == 0) {
            break;
        }
        cursor += 12 + length;
    }

    if (!sawHeader) {
        throw std::runtime_error("PNG has no IHDR chunk");
    }
    return compressed;
}

void appendChunk(std::vector<std::uint8_t>& file, const char kind[4],
                 std::span<const std::uint8_t> payload) {
    std::vector<std::uint8_t> body(4 + payload.size());
    std::memcpy(body.data(), kind, 4);
    if (!payload.empty()) {
        std::memcpy(body.data() + 4, payload.data(), payload.size());
    }

    std::uint8_t lengthBytes[4];
    binary::writeBig32(lengthBytes, static_cast<std::uint32_t>(payload.size()));
    file.insert(file.end(), lengthBytes, lengthBytes + 4);
    file.insert(file.end(), body.begin(), body.end());

    std::uint8_t crcBytes[4];
    const uLong checksum = crc32(crc32(0L, nullptr, 0), body.data(),
                                 static_cast<uInt>(body.size()));
    binary::writeBig32(crcBytes, static_cast<std::uint32_t>(checksum));
    file.insert(file.end(), crcBytes, crcBytes + 4);
}

std::vector<std::uint8_t> buildImageHeaderChunk(int width, int height, int bitDepth,
                                                int colorType) {
    std::vector<std::uint8_t> payload(13, 0);
    binary::writeBig32(payload.data(), static_cast<std::uint32_t>(width));
    binary::writeBig32(payload.data() + 4, static_cast<std::uint32_t>(height));
    payload[8] = static_cast<std::uint8_t>(bitDepth);
    payload[9] = static_cast<std::uint8_t>(colorType);
    return payload;
}

std::vector<std::uint8_t> deflateAll(std::span<const std::uint8_t> raw) {
    uLongf bound = compressBound(static_cast<uLong>(raw.size()));
    std::vector<std::uint8_t> compressed(bound);
    const int status = compress2(compressed.data(), &bound, raw.data(),
                                 static_cast<uLong>(raw.size()), deflateLevel);
    if (status != Z_OK) {
        throw std::runtime_error("PNG image data could not be compressed");
    }
    compressed.resize(bound);
    return compressed;
}

void writePngFile(const std::filesystem::path& path, int width, int height, int bitDepth,
                  int colorType, std::span<const std::uint8_t> rawWithFilters) {
    std::vector<std::uint8_t> file(pngSignature, pngSignature + sizeof(pngSignature));
    const std::vector<std::uint8_t> headerChunk =
        buildImageHeaderChunk(width, height, bitDepth, colorType);
    appendChunk(file, "IHDR", headerChunk);
    appendChunk(file, "IDAT", deflateAll(rawWithFilters));
    appendChunk(file, "IEND", {});

    std::ofstream output(path, std::ios::binary);
    if (!output) {
        throw std::runtime_error("Cannot write PNG: " + path.string());
    }
    output.write(reinterpret_cast<const char*>(file.data()),
                 static_cast<std::streamsize>(file.size()));
}

/// Prefixes every row with filter type 0, which is what the reference writer does.
std::vector<std::uint8_t> prefixRowFilters(std::span<const std::uint8_t> pixels, int height,
                                           std::size_t rowBytes) {
    std::vector<std::uint8_t> raw(static_cast<std::size_t>(height) * (rowBytes + 1));
    for (int row = 0; row < height; ++row) {
        std::uint8_t* destination = raw.data() + static_cast<std::size_t>(row) * (rowBytes + 1);
        destination[0] = 0;
        std::memcpy(destination + 1, pixels.data() + static_cast<std::size_t>(row) * rowBytes,
                    rowBytes);
    }
    return raw;
}

std::vector<std::uint16_t> expandSamples(std::span<const std::uint8_t> raw,
                                         const ImageHeader& header) {
    const int bytesPerSample = header.bitDepth / 8;
    const std::size_t sampleCount =
        static_cast<std::size_t>(header.width) * header.height * header.channels;
    std::vector<std::uint16_t> samples(sampleCount);
    for (std::size_t index = 0; index < sampleCount; ++index) {
        const std::uint8_t* source = raw.data() + index * bytesPerSample;
        if (bytesPerSample == 1) {
            samples[index] = *source;
        } else {
            samples[index] = binary::readBig16(source);
        }
    }
    return samples;
}

}  // namespace

PngImage readPng(const std::filesystem::path& path) {
    const std::vector<std::uint8_t> bytes = io::readAllBytes(path, "PNG");
    if (bytes.size() < sizeof(pngSignature)
        || std::memcmp(bytes.data(), pngSignature, sizeof(pngSignature)) != 0) {
        throw std::runtime_error("Not a PNG file: " + path.string());
    }

    ImageHeader header;
    const std::vector<std::uint8_t> compressed = collectImageData(bytes, header);

    const int bytesPerPixel = header.channels * header.bitDepth / 8;
    const std::size_t rowBytes = static_cast<std::size_t>(header.width) * bytesPerPixel;
    std::vector<std::uint8_t> raw =
        inflateAll(compressed, static_cast<std::size_t>(header.height) * (rowBytes + 1));

    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(header.height) * rowBytes);
    std::vector<std::uint8_t> previous(rowBytes, 0);
    for (int row = 0; row < header.height; ++row) {
        const std::size_t rawOffset = static_cast<std::size_t>(row) * (rowBytes + 1);
        std::uint8_t* destination = pixels.data() + static_cast<std::size_t>(row) * rowBytes;
        std::memcpy(destination, raw.data() + rawOffset + 1, rowBytes);
        unfilterRow(raw[rawOffset], {destination, rowBytes}, previous, bytesPerPixel);
        std::memcpy(previous.data(), destination, rowBytes);
    }

    return PngImage{header.width, header.height, header.channels, header.bitDepth,
                    expandSamples(pixels, header)};
}

void writePngGray16(const std::filesystem::path& path, int width, int height,
                    std::span<const std::uint16_t> samples) {
    const std::size_t rowBytes = static_cast<std::size_t>(width) * 2;
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(height) * rowBytes);
    for (std::size_t index = 0; index < samples.size(); ++index) {
        binary::writeBig16(pixels.data() + index * 2, samples[index]);
    }
    writePngFile(path, width, height, 16, 0, prefixRowFilters(pixels, height, rowBytes));
}

void writePngRgb8(const std::filesystem::path& path, int width, int height,
                  std::span<const std::uint8_t> pixels) {
    const std::size_t rowBytes = static_cast<std::size_t>(width) * 3;
    writePngFile(path, width, height, 8, 2, prefixRowFilters(pixels, height, rowBytes));
}

void writePngRgba8(const std::filesystem::path& path, int width, int height,
                   std::span<const std::uint8_t> pixels) {
    const std::size_t rowBytes = static_cast<std::size_t>(width) * 4;
    writePngFile(path, width, height, 8, 6, prefixRowFilters(pixels, height, rowBytes));
}

}  // namespace terrain::formats
