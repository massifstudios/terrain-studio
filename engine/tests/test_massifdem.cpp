#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <fstream>
#include <vector>

#include "terrain/formats/massifdem.h"
#include "test_support.h"

using Catch::Approx;
using terrain::Heightfield;
using terrain::formats::massifDemHeaderBytes;
using terrain::formats::readMassifDem;
using terrain::formats::writeMassifDem;
using terrain::testing::dataDirectory;
using terrain::testing::readAllBytes;
using terrain::testing::ScratchFile;

namespace {

/// Mirrors `analytic_surface` in tests/data/make_fixture.py.
double analyticSurface(int row, int column, int size) {
    const double northing = (static_cast<double>(row) / (size - 1)) * 2.0 - 1.0;
    const double easting = (static_cast<double>(column) / (size - 1)) * 2.0 - 1.0;
    const double radius = std::sqrt(northing * northing + easting * easting);
    const double cone = std::clamp(1.0 - radius, 0.0, 1.0) * 900.0;
    const double ridge = std::exp(-std::pow(northing * 3.0, 2.0)) * 350.0;
    return 1200.0 + cone + ridge + easting * 120.0;
}

std::filesystem::path fixturePath() {
    return dataDirectory() / "fixture.massifdem";
}

}  // namespace

TEST_CASE("the reference header is read field for field", "[massifdem]") {
    const auto document = readMassifDem(fixturePath());

    REQUIRE(document.field.width() == 96);
    REQUIRE(document.field.height() == 96);
    REQUIRE(document.field.cellSizeMeters() == 30.0);
    REQUIRE(document.field.georeference().coordinateSystem == "EPSG:31254");
    REQUIRE(document.field.georeference().eastingMinMeters == 39075.5);
    REQUIRE(document.field.georeference().northingMaxMeters == 214292.25);
    REQUIRE(document.label == "Terrain Studio format fixture");
}

TEST_CASE("samples decode to the surface the reference writer quantised", "[massifdem]") {
    const auto document = readMassifDem(fixturePath());
    const Heightfield& field = document.field;

    // uint16 over the window's own band; half a step is the worst case.
    const double tolerance = (field.maximum() - field.minimum()) / 65535.0;

    for (int row = 0; row < field.height(); row += 7) {
        for (int column = 0; column < field.width(); column += 5) {
            REQUIRE(field.at(row, column)
                    == Approx(analyticSurface(row, column, 96)).margin(tolerance));
        }
    }
}

TEST_CASE("a read then a write reproduces the reference sample block exactly", "[massifdem]") {
    const auto document = readMassifDem(fixturePath());

    ScratchFile output("roundtrip.massifdem");
    writeMassifDem(output.path(), document.field, document.label);

    const std::vector<std::uint8_t> written = readAllBytes(output.path());
    const std::vector<std::uint8_t> reference = readAllBytes(fixturePath());
    REQUIRE(written.size() == reference.size());

    // Every quantised level survives the trip through float32 storage.
    const std::vector<std::uint8_t> writtenSamples(written.begin() + massifDemHeaderBytes,
                                                   written.end());
    const std::vector<std::uint8_t> referenceSamples(reference.begin() + massifDemHeaderBytes,
                                                     reference.end());
    REQUIRE(writtenSamples == referenceSamples);
}

TEST_CASE("the header survives a round trip except for float32 storage error", "[massifdem]") {
    const auto document = readMassifDem(fixturePath());

    ScratchFile output("roundtrip-header.massifdem");
    writeMassifDem(output.path(), document.field, document.label);

    const auto reread = readMassifDem(output.path());

    REQUIRE(reread.label == document.label);
    REQUIRE(reread.field.width() == document.field.width());
    REQUIRE(reread.field.height() == document.field.height());
    REQUIRE(reread.field.cellSizeMeters() == document.field.cellSizeMeters());
    REQUIRE(reread.field.georeference().coordinateSystem
            == document.field.georeference().coordinateSystem);
    REQUIRE(reread.field.georeference().eastingMinMeters
            == document.field.georeference().eastingMinMeters);
    REQUIRE(reread.field.georeference().northingMaxMeters
            == document.field.georeference().northingMaxMeters);

    // Levels are preserved exactly; the band they decode against is recomputed
    // from float32 samples, so altitudes land within a float32 ULP.
    const double tolerance = (document.field.maximum() - document.field.minimum()) * 1.0e-6;
    for (int row = 0; row < document.field.height(); ++row) {
        for (int column = 0; column < document.field.width(); ++column) {
            REQUIRE(reread.field.at(row, column)
                    == Approx(document.field.at(row, column)).margin(tolerance));
        }
    }
}

TEST_CASE("a label longer than the header field is truncated, not overflowed", "[massifdem]") {
    const auto document = readMassifDem(fixturePath());
    const std::string longLabel(200, 'x');

    ScratchFile output("longlabel.massifdem");
    writeMassifDem(output.path(), document.field, longLabel);

    const auto reread = readMassifDem(output.path());
    REQUIRE(reread.label.size() == 95);
    REQUIRE(readAllBytes(output.path()).size() == readAllBytes(fixturePath()).size());
}

TEST_CASE("a file that is not a DEM is refused", "[massifdem]") {
    ScratchFile bogus("not-a-dem.massifdem");
    {
        std::ofstream file(bogus.path(), std::ios::binary);
        file << "this is not a digital elevation model, it is a sentence";
    }

    REQUIRE_THROWS(readMassifDem(bogus.path()));
}
