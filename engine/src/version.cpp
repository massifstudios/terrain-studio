#include "terrain/version.h"

#ifndef TERRAIN_ENGINE_VERSION
#define TERRAIN_ENGINE_VERSION "0.0.0"
#endif

namespace terrain {

std::string_view engineVersion() {
    return TERRAIN_ENGINE_VERSION;
}

}  // namespace terrain
