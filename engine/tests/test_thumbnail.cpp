#include <catch2/catch_test_macros.hpp>

#include <set>

#include "terrain/thumbnail.h"

using terrain::FieldSemantics;
using terrain::Heightfield;
using terrain::renderThumbnail;

namespace {

constexpr int gridSize = 64;

Heightfield maskWithHorizontalRamp() {
    Heightfield mask(gridSize, gridSize);
    mask.setSemantics(FieldSemantics::Mask);
    for (int row = 0; row < gridSize; ++row) {
        for (int column = 0; column < gridSize; ++column) {
            mask.at(row, column) = static_cast<float>(column) / (gridSize - 1);
        }
    }
    return mask;
}

}  // namespace

TEST_CASE("a mask thumbnail is neutral grey", "[thumbnail]") {
    const terrain::ThumbnailImage image = renderThumbnail(maskWithHorizontalRamp(), gridSize);

    for (std::size_t pixel = 0; pixel < image.rgba.size(); pixel += 4) {
        REQUIRE(image.rgba[pixel] == image.rgba[pixel + 1]);
        REQUIRE(image.rgba[pixel + 1] == image.rgba[pixel + 2]);
        REQUIRE(image.rgba[pixel + 3] == 255);
    }
}

TEST_CASE("a mask thumbnail quantises to eight steps", "[thumbnail]") {
    const terrain::ThumbnailImage image = renderThumbnail(maskWithHorizontalRamp(), gridSize);

    std::set<std::uint8_t> levels;
    for (std::size_t pixel = 0; pixel < image.rgba.size(); pixel += 4) {
        levels.insert(image.rgba[pixel]);
    }

    REQUIRE(levels.size() == 8);
    REQUIRE(*levels.begin() == 20);
    REQUIRE(*levels.rbegin() == 239);
}
