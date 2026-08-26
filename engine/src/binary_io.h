#pragma once

#include <bit>
#include <cstdint>
#include <cstring>

/// Explicit byte-order helpers. File layouts are fixed by their formats, so
/// nothing here may depend on the host's endianness.
namespace terrain::binary {

inline std::uint16_t readLittle16(const std::uint8_t* bytes) {
    return static_cast<std::uint16_t>(bytes[0] | (bytes[1] << 8));
}

inline std::uint32_t readLittle32(const std::uint8_t* bytes) {
    return static_cast<std::uint32_t>(bytes[0]) | (static_cast<std::uint32_t>(bytes[1]) << 8)
           | (static_cast<std::uint32_t>(bytes[2]) << 16)
           | (static_cast<std::uint32_t>(bytes[3]) << 24);
}

inline std::uint64_t readLittle64(const std::uint8_t* bytes) {
    std::uint64_t value = 0;
    for (int index = 7; index >= 0; --index) {
        value = (value << 8) | bytes[index];
    }
    return value;
}

inline double readLittleDouble(const std::uint8_t* bytes) {
    return std::bit_cast<double>(readLittle64(bytes));
}

inline void writeLittle16(std::uint8_t* bytes, std::uint16_t value) {
    bytes[0] = static_cast<std::uint8_t>(value & 0xFF);
    bytes[1] = static_cast<std::uint8_t>((value >> 8) & 0xFF);
}

inline void writeLittle32(std::uint8_t* bytes, std::uint32_t value) {
    for (int index = 0; index < 4; ++index) {
        bytes[index] = static_cast<std::uint8_t>((value >> (8 * index)) & 0xFF);
    }
}

inline void writeLittle64(std::uint8_t* bytes, std::uint64_t value) {
    for (int index = 0; index < 8; ++index) {
        bytes[index] = static_cast<std::uint8_t>((value >> (8 * index)) & 0xFF);
    }
}

inline void writeLittleDouble(std::uint8_t* bytes, double value) {
    writeLittle64(bytes, std::bit_cast<std::uint64_t>(value));
}

inline std::uint32_t readBig32(const std::uint8_t* bytes) {
    return (static_cast<std::uint32_t>(bytes[0]) << 24)
           | (static_cast<std::uint32_t>(bytes[1]) << 16)
           | (static_cast<std::uint32_t>(bytes[2]) << 8) | static_cast<std::uint32_t>(bytes[3]);
}

inline void writeBig32(std::uint8_t* bytes, std::uint32_t value) {
    for (int index = 0; index < 4; ++index) {
        bytes[index] = static_cast<std::uint8_t>((value >> (8 * (3 - index))) & 0xFF);
    }
}

inline std::uint16_t readBig16(const std::uint8_t* bytes) {
    return static_cast<std::uint16_t>((bytes[0] << 8) | bytes[1]);
}

inline void writeBig16(std::uint8_t* bytes, std::uint16_t value) {
    bytes[0] = static_cast<std::uint8_t>((value >> 8) & 0xFF);
    bytes[1] = static_cast<std::uint8_t>(value & 0xFF);
}

inline float readLittleFloat(const std::uint8_t* bytes) {
    return std::bit_cast<float>(readLittle32(bytes));
}

}  // namespace terrain::binary
