#pragma once

#include <string_view>

namespace terrain {

/// Semantic version of the engine library, reported through the bridge.
std::string_view engineVersion();

}  // namespace terrain
