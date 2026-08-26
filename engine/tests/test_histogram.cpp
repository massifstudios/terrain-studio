#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "terrain/histogram.h"

using Catch::Approx;
using terrain::Histogram;

namespace {

std::vector<float> evenSpread(int count) {
    std::vector<float> values;
    values.reserve(static_cast<std::size_t>(count));
    for (int index = 0; index < count; ++index) {
        values.push_back(static_cast<float>(index) / count);
    }
    return values;
}

/// Nine tenths of the field on the floor, one tenth on the ceiling: the mean
/// says one thing about this field and the median says another.
std::vector<float> lopsidedSpread(int count) {
    std::vector<float> values(static_cast<std::size_t>(count), 0.0f);
    for (std::size_t index = values.size() * 9 / 10; index < values.size(); ++index) {
        values[index] = 100.0f;
    }
    return values;
}

}  // namespace

TEST_CASE("a flat field has no distribution to read", "[histogram]") {
    const std::vector<float> values(64, 5.0f);
    const Histogram histogram(values.data(), values.size());

    REQUIRE(histogram.isFlat());
    REQUIRE(histogram.minimum() == Approx(5.0));
    REQUIRE(histogram.quantileOfValue(5.0f) == Approx(0.0));
}

TEST_CASE("an even spread puts every quantile where it belongs", "[histogram]") {
    const std::vector<float> values = evenSpread(1000);
    const Histogram histogram(values.data(), values.size());

    REQUIRE(histogram.minimum() == Approx(0.0));
    REQUIRE(histogram.maximum() == Approx(0.999));
    REQUIRE(histogram.valueAtQuantile(0.5f) == Approx(0.5).margin(0.01));
    REQUIRE(histogram.quantileOfValue(0.25f) == Approx(0.25).margin(0.01));
}

TEST_CASE("a quantile follows the cells, not the range", "[histogram]") {
    const std::vector<float> values = lopsidedSpread(1000);
    const Histogram histogram(values.data(), values.size());

    REQUIRE(histogram.valueAtQuantile(0.5f) == Approx(0.0).margin(0.1));
    REQUIRE(histogram.valueAtQuantile(0.95f) == Approx(100.0).margin(0.1));
    REQUIRE(histogram.quantileOfValue(50.0f) == Approx(0.9).margin(0.01));
}

TEST_CASE("the same values give the same curve whatever order they arrive in",
          "[histogram][determinism]") {
    std::vector<float> forwards = evenSpread(512);
    std::vector<float> backwards(forwards.rbegin(), forwards.rend());

    const Histogram first(forwards.data(), forwards.size());
    const Histogram second(backwards.data(), backwards.size());
    REQUIRE(first.valueAtQuantile(0.37f) == second.valueAtQuantile(0.37f));
    REQUIRE(first.quantileOfValue(0.62f) == second.quantileOfValue(0.62f));
}
