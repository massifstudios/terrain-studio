#ifndef TERRAIN_EROSION_COMMON_GLSL
#define TERRAIN_EROSION_COMMON_GLSL

#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "exact_math.glsl"

/*
 * The pieces both erosion shaders share with `erosion_support.h` and
 * `erosion_nodes.cpp` on the host.
 *
 * Everything here mirrors host code line for line, including the order the
 * arithmetic happens in, because the two have to agree bit for bit. Where the
 * host writes `a / b` this writes `terrainDivide(a, b)` and where it writes
 * `std::sqrt` this writes `terrainSquareRoot`; those are the only two
 * substitutions, and they exist precisely so that nothing else has to change.
 */

/// One unit is 10 micrometres. Matches FixedPointGrid::unitsPerMeter.
const float unitsPerMeter = 100000.0;

/// std::min and std::max compare with `<` and return the first argument on a
/// tie. GLSL's min and max are specified the same way, but writing the
/// comparison out removes the question.
float smallerOf(float first, float second) {
    if (second < first) {
        return second;
    }
    return first;
}

float largerOf(float first, float second) {
    if (first < second) {
        return second;
    }
    return first;
}

float clampedTo(float value, float low, float high) {
    if (value < low) {
        return low;
    }
    if (high < value) {
        return high;
    }
    return value;
}

/// The eight neighbours, in the order the host visits them. The order is part
/// of the result: the excess total is a running float sum.
const ivec2 neighbourOffsets[8] = ivec2[8](ivec2(-1, -1), ivec2(-1, 0), ivec2(-1, 1),
                                           ivec2(0, -1), ivec2(0, 1), ivec2(1, -1),
                                           ivec2(1, 0), ivec2(1, 1));

bool isDiagonalDirection(int direction) {
    return neighbourOffsets[direction].x != 0 && neighbourOffsets[direction].y != 0;
}

bool isInsideGrid(int row, int column, int size) {
    return row >= 0 && row < size && column >= 0 && column < size;
}

/// std::numbers::sqrt2_v<float>, written out past the last bit a float holds
/// so that both compilers round to the same 0x3fb504f3 rather than to
/// whichever value their own shorter literal happened to name.
const float squareRootOfTwo = 1.41421353816986083984375;

float thresholdFor(int direction, float talusDropMeters) {
    if (isDiagonalDirection(direction)) {
        return talusDropMeters * squareRootOfTwo;
    }
    return talusDropMeters;
}

/* --------------------------------------------------------------- the RNG -- */

/*
 * splitmix64, matching the inline generator in the node SDK exactly. It is
 * repeated rather than included because the SDK header is C and a plugin
 * compiles it for the host; a shader cannot include it, and a droplet has to
 * draw the same landing spot on both. `the RNG agrees with the SDK` in the
 * test suite is what keeps the two copies honest.
 */
uint64_t terrainRandomNext(inout uint64_t state) {
    state = state + 0x9E3779B97F4A7C15ul;
    uint64_t value = state;
    value = (value ^ (value >> 30)) * 0xBF58476D1CE4E5B9ul;
    value = (value ^ (value >> 27)) * 0x94D049BB133111EBul;
    return value ^ (value >> 31);
}

float terrainRandomUnitFloat(inout uint64_t state) {
    return float(uint(terrainRandomNext(state) >> 40)) * (1.0 / 16777216.0);
}

uint64_t terrainSeedForLane(uint64_t seed, uint64_t lane) {
    uint64_t state = seed ^ (lane * 0x9E3779B97F4A7C15ul);
    return terrainRandomNext(state);
}

#endif
