#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <vector>

#include "terrain/formats/heightfield_png.h"
#include "terrain/formats/png_image.h"
#include "terrain/formats/raw_float32.h"
#include "test_support.h"

using Catch::Approx;
using terrain::Heightfield;
using terrain::formats::HeightPngBand;
using terrain::formats::RawFloat32Layout;
using terrain::formats::readHeightfieldPng;
using terrain::formats::readPng;
using terrain::formats::readRawFloat32;
using terrain::formats::writeHeightfieldPng16;
using terrain::formats::writePngRgba8;
using terrain::testing::dataDirectory;
using terrain::testing::ScratchFile;

namespace {

Heightfield rampField(int width, int height) {
    Heightfield field(width, height);
    field.setCellSizeMeters(10.0);
    for (int row = 0; row < height; ++row) {
        for (int column = 0; column < width; ++column) {
            field.at(row, column) = static_cast<float>(500.0 + row * 3.0 + column * 0.5);
        }
    }
    return field;
}

}  // namespace

TEST_CASE("an 8-bit RGB PNG from another writer decodes", "[png]") {
    const terrain::formats::PngImage image = readPng(dataDirectory() / "fixture_preview.png");

    REQUIRE(image.width == 96);
    REQUIRE(image.height == 96);
    REQUIRE(image.channels == 3);
    REQUIRE(image.bitDepth == 8);
    REQUIRE(image.samples.size() == 96u * 96u * 3u);
}

TEST_CASE("a 16-bit heightfield survives a PNG round trip", "[png]") {
    const Heightfield original = rampField(37, 23);

    ScratchFile output("height.png");
    writeHeightfieldPng16(output.path(), original);

    const HeightPngBand band{10.0, original.minimum(), original.maximum()};
    const Heightfield reloaded = readHeightfieldPng(output.path(), band);

    REQUIRE(reloaded.width() == original.width());
    REQUIRE(reloaded.height() == original.height());

    const double tolerance = (original.maximum() - original.minimum()) / 65535.0;
    for (int row = 0; row < original.height(); ++row) {
        for (int column = 0; column < original.width(); ++column) {
            REQUIRE(reloaded.at(row, column)
                    == Approx(original.at(row, column)).margin(tolerance));
        }
    }
}

TEST_CASE("an RGBA PNG round-trips through the codec", "[png]") {
    std::vector<std::uint8_t> pixels(4 * 3 * 4);
    for (std::size_t index = 0; index < pixels.size(); ++index) {
        pixels[index] = static_cast<std::uint8_t>(index * 7);
    }

    ScratchFile output("splat.png");
    writePngRgba8(output.path(), 4, 3, pixels);

    const terrain::formats::PngImage image = readPng(output.path());
    REQUIRE(image.channels == 4);
    REQUIRE(image.bitDepth == 8);
    for (std::size_t index = 0; index < pixels.size(); ++index) {
        REQUIRE(image.samples[index] == pixels[index]);
    }
}

TEST_CASE("raw float32 imports at the stated shape", "[raw]") {
    ScratchFile output("terrain.r32");
    {
        std::ofstream file(output.path(), std::ios::binary);
        for (int index = 0; index < 6; ++index) {
            const float value = static_cast<float>(index) * 1.5f;
            file.write(reinterpret_cast<const char*>(&value), sizeof(value));
        }
    }

    const Heightfield field = readRawFloat32(output.path(), RawFloat32Layout{3, 2, 25.0});

    REQUIRE(field.width() == 3);
    REQUIRE(field.height() == 2);
    REQUIRE(field.cellSizeMeters() == 25.0);
    REQUIRE(field.at(0, 0) == 0.0f);
    REQUIRE(field.at(1, 2) == 7.5f);
}

TEST_CASE("a raw file shorter than its stated shape is refused", "[raw]") {
    ScratchFile output("short.r32");
    {
        std::ofstream file(output.path(), std::ios::binary);
        const float value = 1.0f;
        file.write(reinterpret_cast<const char*>(&value), sizeof(value));
    }

    REQUIRE_THROWS(readRawFloat32(output.path(), RawFloat32Layout{64, 64, 1.0}));
}
