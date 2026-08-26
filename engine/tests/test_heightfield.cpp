#include <catch2/catch_test_macros.hpp>

#include "terrain/heightfield.h"

using terrain::GeoReference;
using terrain::Heightfield;

TEST_CASE("a heightfield is row-major with row 0 north", "[heightfield]") {
    Heightfield field(4, 3);

    REQUIRE(field.width() == 4);
    REQUIRE(field.height() == 3);
    REQUIRE(field.sampleCount() == 12);

    field.at(2, 1) = 42.0f;
    REQUIRE(field.data()[2 * 4 + 1] == 42.0f);
}

TEST_CASE("world size follows the cell size", "[heightfield]") {
    Heightfield field(100, 50);
    field.setCellSizeMeters(30.0);

    REQUIRE(field.worldWidthMeters() == 3000.0);
    REQUIRE(field.worldHeightMeters() == 1500.0);
}

TEST_CASE("a georeference is absent until one is set", "[heightfield]") {
    Heightfield field(2, 2);
    REQUIRE_FALSE(field.georeference().isPresent());

    field.setGeoreference(GeoReference{"EPSG:31254", 39075.5, 214292.25});
    REQUIRE(field.georeference().isPresent());
    REQUIRE(field.georeference().coordinateSystem == "EPSG:31254");
}

TEST_CASE("minimum and maximum span the samples", "[heightfield]") {
    Heightfield field(2, 2);
    field.fill(5.0f);
    field.at(1, 1) = -3.0f;
    field.at(0, 1) = 11.0f;

    REQUIRE(field.minimum() == -3.0f);
    REQUIRE(field.maximum() == 11.0f);
}

TEST_CASE("zero or negative dimensions are refused", "[heightfield]") {
    REQUIRE_THROWS(Heightfield(0, 4));
    REQUIRE_THROWS(Heightfield(4, -1));
}
