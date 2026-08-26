#include "terrain/hash.h"

#include <bit>

#include "terrain_node_sdk.h"

namespace terrain {
namespace {

constexpr std::uint64_t fnvPrime = 1099511628211ull;

}  // namespace

void Hasher::combine(std::uint64_t value) {
    for (int byteIndex = 0; byteIndex < 8; ++byteIndex) {
        state_ ^= (value >> (8 * byteIndex)) & 0xFFull;
        state_ *= fnvPrime;
    }
}

void Hasher::combine(double value) {
    // Normalise the two zeros so -0.0 and 0.0 cannot key different cache lines.
    if (value == 0.0) {
        combine(std::uint64_t{0});
        return;
    }
    combine(std::bit_cast<std::uint64_t>(value));
}

void Hasher::combine(std::string_view text) {
    combine(static_cast<std::uint64_t>(text.size()));
    for (const char character : text) {
        state_ ^= static_cast<unsigned char>(character);
        state_ *= fnvPrime;
    }
}

std::uint64_t stableHash(std::string_view text) {
    Hasher hasher;
    hasher.combine(text);
    return hasher.digest();
}

std::uint64_t deriveNodeSeed(std::uint64_t graphSeed, std::string_view nodeId) {
    std::uint64_t state = graphSeed ^ stableHash(nodeId);
    return terrainRandomNext(&state);
}

}  // namespace terrain
