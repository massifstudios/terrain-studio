#include "terrain/noise.h"

#include <algorithm>
#include <cmath>

#include "terrain_node_sdk.h"

namespace terrain::noise {
namespace {

std::uint64_t hashLattice(std::uint64_t seed, std::int64_t x, std::int64_t y) {
    std::uint64_t state = seed;
    state ^= static_cast<std::uint64_t>(x) * 0xD6E8FEB86659FD93ull;
    state ^= static_cast<std::uint64_t>(y) * 0xA0761D6478BD642Full;
    return terrainRandomNext(&state);
}

struct Gradient {
    float x;
    float y;
};

/// Eight evenly spaced directions. A small fixed set keeps the noise free of
/// the directional bias a poorly chosen random vector can introduce.
Gradient latticeGradient(std::uint64_t seed, std::int64_t x, std::int64_t y) {
    constexpr float diagonal = 0.7071067811865476f;
    static const Gradient directions[8] = {{1.0f, 0.0f},          {-1.0f, 0.0f},
                                           {0.0f, 1.0f},          {0.0f, -1.0f},
                                           {diagonal, diagonal},  {-diagonal, diagonal},
                                           {diagonal, -diagonal}, {-diagonal, -diagonal}};
    return directions[hashLattice(seed, x, y) & 7u];
}

/// Ken Perlin's quintic fade: zero first and second derivatives at both ends,
/// so octaves meet without a visible crease along the lattice.
float fade(float t) {
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

float mix(float low, float high, float weight) {
    return low + (high - low) * weight;
}

float cornerDot(std::uint64_t seed, std::int64_t cellX, std::int64_t cellY, float offsetX,
                float offsetY) {
    const Gradient gradient = latticeGradient(seed, cellX, cellY);
    return gradient.x * offsetX + gradient.y * offsetY;
}

float randomUnitFromHash(std::uint64_t hash) {
    return static_cast<float>(hash >> 40) * (1.0f / 16777216.0f);
}

}  // namespace

float perlin(float x, float y, std::uint64_t seed) {
    const float flooredX = std::floor(x);
    const float flooredY = std::floor(y);
    const std::int64_t cellX = static_cast<std::int64_t>(flooredX);
    const std::int64_t cellY = static_cast<std::int64_t>(flooredY);
    const float localX = x - flooredX;
    const float localY = y - flooredY;

    const float bottomLeft = cornerDot(seed, cellX, cellY, localX, localY);
    const float bottomRight = cornerDot(seed, cellX + 1, cellY, localX - 1.0f, localY);
    const float topLeft = cornerDot(seed, cellX, cellY + 1, localX, localY - 1.0f);
    const float topRight = cornerDot(seed, cellX + 1, cellY + 1, localX - 1.0f, localY - 1.0f);

    const float weightX = fade(localX);
    const float weightY = fade(localY);
    // Perlin's own scale factor, which brings the unit-gradient result back
    // into [-1, 1].
    return mix(mix(bottomLeft, bottomRight, weightX), mix(topLeft, topRight, weightX), weightY)
           * 1.4142135623730951f;
}

float fractalPerlin(float x, float y, std::uint64_t seed, const FractalOptions& options) {
    float total = 0.0f;
    float amplitude = 1.0f;
    float amplitudeSum = 0.0f;
    float frequency = 1.0f;

    for (int octave = 0; octave < std::max(options.octaves, 1); ++octave) {
        const std::uint64_t octaveSeed = terrainSeedForLane(seed, static_cast<std::uint64_t>(octave));
        total += perlin(x * frequency, y * frequency, octaveSeed) * amplitude;
        amplitudeSum += amplitude;
        amplitude *= options.gain;
        frequency *= options.lacunarity;
    }
    return total / std::max(amplitudeSum, 1.0e-6f);
}

float ridged(float x, float y, std::uint64_t seed, const FractalOptions& options,
             float sharpness) {
    float total = 0.0f;
    float amplitude = 1.0f;
    float amplitudeSum = 0.0f;
    float frequency = 1.0f;
    float carriedWeight = 1.0f;

    for (int octave = 0; octave < std::max(options.octaves, 1); ++octave) {
        const std::uint64_t octaveSeed = terrainSeedForLane(seed, static_cast<std::uint64_t>(octave));
        float signal = 1.0f - std::abs(perlin(x * frequency, y * frequency, octaveSeed));
        signal = std::pow(signal, std::max(sharpness, 0.01f));
        signal *= std::clamp(carriedWeight, 0.0f, 1.0f);
        carriedWeight = signal * 2.0f;

        total += signal * amplitude;
        amplitudeSum += amplitude;
        amplitude *= options.gain;
        frequency *= options.lacunarity;
    }
    return std::clamp(total / std::max(amplitudeSum, 1.0e-6f), 0.0f, 1.0f);
}

VoronoiSample voronoi(float x, float y, std::uint64_t seed, float jitter) {
    const std::int64_t baseX = static_cast<std::int64_t>(std::floor(x));
    const std::int64_t baseY = static_cast<std::int64_t>(std::floor(y));
    const float clampedJitter = std::clamp(jitter, 0.0f, 1.0f);

    VoronoiSample sample;
    sample.nearestDistance = 1.0e30f;
    sample.secondDistance = 1.0e30f;

    for (std::int64_t offsetY = -1; offsetY <= 1; ++offsetY) {
        for (std::int64_t offsetX = -1; offsetX <= 1; ++offsetX) {
            const std::int64_t cellX = baseX + offsetX;
            const std::int64_t cellY = baseY + offsetY;
            std::uint64_t state = hashLattice(seed, cellX, cellY);
            const float pointX =
                static_cast<float>(cellX) + 0.5f
                + (terrainRandomUnitFloat(&state) - 0.5f) * clampedJitter;
            const float pointY =
                static_cast<float>(cellY) + 0.5f
                + (terrainRandomUnitFloat(&state) - 0.5f) * clampedJitter;

            const float deltaX = pointX - x;
            const float deltaY = pointY - y;
            const float distance = std::sqrt(deltaX * deltaX + deltaY * deltaY);
            if (distance < sample.nearestDistance) {
                sample.secondDistance = sample.nearestDistance;
                sample.nearestDistance = distance;
                sample.cellValue = randomUnitFromHash(hashLattice(seed ^ 0x5BF03635ull, cellX, cellY));
                continue;
            }
            sample.secondDistance = std::min(sample.secondDistance, distance);
        }
    }
    return sample;
}

}  // namespace terrain::noise
