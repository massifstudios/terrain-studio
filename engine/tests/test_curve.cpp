#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "terrain/curve.h"

using Catch::Approx;
using terrain::Curve;

TEST_CASE("the default curve is the identity", "[curve]") {
    const Curve curve = Curve::parse("0,0;1,1");

    REQUIRE(curve.valueAt(0.0f) == Approx(0.0));
    REQUIRE(curve.valueAt(0.25f) == Approx(0.25));
    REQUIRE(curve.valueAt(0.5f) == Approx(0.5));
    REQUIRE(curve.valueAt(1.0f) == Approx(1.0));
}

TEST_CASE("a curve interpolates through every point it is given", "[curve]") {
    const Curve curve = Curve::parse("0,0;0.5,0.9;1,1");

    REQUIRE(curve.valueAt(0.0f) == Approx(0.0));
    REQUIRE(curve.valueAt(0.5f) == Approx(0.9));
    REQUIRE(curve.valueAt(1.0f) == Approx(1.0));
}

TEST_CASE("a rising curve never overshoots its points", "[curve]") {
    // A natural cubic spline would bulge above 1 after this steep rise; the
    // monotone fit must not, because that would invent altitude.
    const Curve curve = Curve::parse("0,0;0.1,0.95;1,1");

    for (int step = 0; step <= 100; ++step) {
        const float position = step / 100.0f;
        REQUIRE(curve.valueAt(position) >= 0.0f);
        REQUIRE(curve.valueAt(position) <= 1.0f);
    }
}

TEST_CASE("a curve reads outside its ends as flat", "[curve]") {
    const Curve curve = Curve::parse("0.25,0.4;0.75,0.6");

    REQUIRE(curve.valueAt(0.0f) == Approx(0.4));
    REQUIRE(curve.valueAt(1.0f) == Approx(0.6));
}

TEST_CASE("points arrive sorted however they were written", "[curve]") {
    const Curve curve = Curve::parse("1,1;0.5,0.2;0,0");

    REQUIRE(curve.points().size() == 3);
    REQUIRE(curve.valueAt(0.5f) == Approx(0.2));
}

TEST_CASE("an unusable curve falls back to the identity", "[curve]") {
    for (const char* text : {"", "nonsense", "0,0", "0,0;0,1"}) {
        const Curve curve = Curve::parse(text);
        REQUIRE(curve.points().size() == 2);
        REQUIRE(curve.valueAt(0.3f) == Approx(0.3));
    }
}

TEST_CASE("a curve can invert a field", "[curve]") {
    const Curve curve = Curve::parse("0,1;1,0");

    REQUIRE(curve.valueAt(0.0f) == Approx(1.0));
    REQUIRE(curve.valueAt(1.0f) == Approx(0.0));
    REQUIRE(curve.valueAt(0.5f) == Approx(0.5));
}
