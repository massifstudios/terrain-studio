#pragma once

#include <napi.h>

#include "terrain/graph.h"

namespace bridge {

/// Builds an engine graph from the plain object the renderer sends. Must run
/// on the JavaScript thread: it touches V8 values.
terrain::Graph graphFromJs(const Napi::Object& source);

}  // namespace bridge
