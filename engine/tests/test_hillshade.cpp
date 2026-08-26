#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>

#include "terrain/formats/hillshade.h"
#include "terrain/formats/massifdem.h"
#include "terrain/formats/png_image.h"
#include "test_support.h"

using terrain::formats::previewPathFor;
using terrain::formats::readMassifDem;
using terrain::formats::readPng;
using terrain::formats::writeHillshadePreview;
using terrain::testing::dataDirectory;
using terrain::testing::ScratchFile;

TEST_CASE("the preview matches the reference hillshade pixel for pixel", "[hillshade]") {
    const auto document = readMassifDem(dataDirectory() / "fixture.massifdem");

    ScratchFile output("preview.png");
    writeHillshadePreview(output.path(), document.field);

    const terrain::formats::PngImage ours = readPng(output.path());
    const terrain::formats::PngImage reference = readPng(dataDirectory() / "fixture_preview.png");

    REQUIRE(ours.width == reference.width);
    REQUIRE(ours.height == reference.height);
    REQUIRE(ours.channels == reference.channels);
    REQUIRE(ours.samples.size() == reference.samples.size());

    // The two paths do the same arithmetic in double; only the last bit of a
    // libm call can separate them, and that lands inside one 8-bit level.
    int worstDifference = 0;
    for (std::size_t index = 0; index < ours.samples.size(); ++index) {
        const int difference = std::abs(static_cast<int>(ours.samples[index])
                                        - static_cast<int>(reference.samples[index]));
        worstDifference = std::max(worstDifference, difference);
    }
    REQUIRE(worstDifference <= 1);
}

TEST_CASE("the preview path sits beside the DEM", "[hillshade]") {
    const std::filesystem::path preview = previewPathFor("/tmp/terrain/oetztal.massifdem");
    REQUIRE(preview == std::filesystem::path("/tmp/terrain/oetztal_preview.png"));
}
