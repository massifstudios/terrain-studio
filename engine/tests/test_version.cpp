#include <catch2/catch_test_macros.hpp>

#include "terrain/version.h"

TEST_CASE("engine reports a non-empty semantic version", "[version]") {
    const std::string_view version = terrain::engineVersion();

    REQUIRE_FALSE(version.empty());
    REQUIRE(version.find('.') != std::string_view::npos);
}
