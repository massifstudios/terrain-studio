#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include "terrain/noise.h"

using Catch::Approx;
using terrain::noise::FractalOptions;

namespace {

constexpr std::uint64_t seed = 0x9E3779B97F4A7C15ull;

}  // namespace

TEST_CASE("gradient noise is exactly zero on the lattice", "[noise]") {
    // The defining property of Perlin noise: every corner offset is the zero
    // vector, so the dot products vanish. If this drifts, the noise has
    // stopped being gradient noise.
    for (int y = -3; y <= 3; ++y) {
        for (int x = -3; x <= 3; ++x) {
            REQUIRE(terrain::noise::perlin(static_cast<float>(x), static_cast<float>(y), seed)
                    == Approx(0.0).margin(1.0e-6));
        }
    }
}

TEST_CASE("gradient noise stays inside its nominal range", "[noise]") {
    float strongest = 0.0f;
    for (int step = 0; step < 4000; ++step) {
        const float position = step * 0.017f;
        strongest = std::max(strongest, std::abs(terrain::noise::perlin(position, position * 0.7f,
                                                                       seed)));
    }
    REQUIRE(strongest <= 1.0f);
    // A field that never gets near its own range would be a bug of its own.
    REQUIRE(strongest > 0.5f);
}

TEST_CASE("the same coordinate and seed always give the same noise", "[noise][determinism]") {
    REQUIRE(terrain::noise::perlin(3.25f, -7.5f, seed)
            == terrain::noise::perlin(3.25f, -7.5f, seed));
    REQUIRE(terrain::noise::perlin(3.25f, -7.5f, seed)
            != terrain::noise::perlin(3.25f, -7.5f, seed + 1));
}

TEST_CASE("stacked octaves stay normalised", "[noise]") {
    FractalOptions options;
    options.octaves = 8;

    for (int step = 0; step < 2000; ++step) {
        const float position = step * 0.011f;
        const float value = terrain::noise::fractalPerlin(position, position * 1.3f, seed, options);
        REQUIRE(value >= -1.0f);
        REQUIRE(value <= 1.0f);
    }
}

TEST_CASE("ridged noise is one-sided", "[noise]") {
    FractalOptions options;
    options.octaves = 6;

    float highest = 0.0f;
    for (int step = 0; step < 2000; ++step) {
        const float position = step * 0.011f;
        const float value = terrain::noise::ridged(position, position * 1.3f, seed, options, 1.0f);
        REQUIRE(value >= 0.0f);
        REQUIRE(value <= 1.0f);
        highest = std::max(highest, value);
    }
    REQUIRE(highest > 0.5f);
}

TEST_CASE("the second voronoi distance is never nearer than the first", "[noise]") {
    for (int step = 0; step < 500; ++step) {
        const float position = step * 0.037f;
        const terrain::noise::VoronoiSample sample =
            terrain::noise::voronoi(position, position * 0.61f, seed, 1.0f);

        REQUIRE(sample.nearestDistance <= sample.secondDistance);
        REQUIRE(sample.cellValue >= 0.0f);
        REQUIRE(sample.cellValue < 1.0f);
    }
}

TEST_CASE("voronoi without jitter puts every point on the grid centre", "[noise]") {
    // At a cell centre the nearest feature point is the centre itself.
    const terrain::noise::VoronoiSample sample = terrain::noise::voronoi(2.5f, 3.5f, seed, 0.0f);
    REQUIRE(sample.nearestDistance == Approx(0.0).margin(1.0e-6));
    REQUIRE(sample.secondDistance == Approx(1.0).margin(1.0e-5));
}
