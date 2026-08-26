#pragma once

#include <cstdint>
#include <string_view>

namespace terrain {

/**
 * Stable hashing for cache keys and seed derivation. `std::hash` is not usable
 * here: its results may differ between standard-library versions, and the
 * determinism contract reaches across machines, not just across runs.
 */
class Hasher {
public:
    void combine(std::uint64_t value);
    void combine(double value);
    void combine(std::string_view text);

    std::uint64_t digest() const { return state_; }

private:
    static constexpr std::uint64_t offsetBasis = 1469598103934665603ull;
    std::uint64_t state_ = offsetBasis;
};

std::uint64_t stableHash(std::string_view text);

/// Per-node seed: the graph seed combined with the node's stable id, so adding
/// a node never shifts another node's random sequence.
std::uint64_t deriveNodeSeed(std::uint64_t graphSeed, std::string_view nodeId);

}  // namespace terrain
