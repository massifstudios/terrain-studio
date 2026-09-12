#pragma once

#include <cstdint>

namespace terrain::noise {

/**
 * Procedural bases, sampled in noise space (one unit = one feature). Callers
 * divide world meters by a feature size, so the same parameters give the same
 * landscape at any resolution.
 *
 * Every value here comes from hashing integer lattice coordinates with the
 * node's seed. There is no table to initialise, no state to carry, and nothing
 * that could differ between machines.
 */

/// Classic gradient noise, in [-1, 1] and exactly 0 at every lattice corner.
float perlin(float x, float y, std::uint64_t seed);

/// The same, repeating every `periodX` lattice cells along x, so a field read
/// around a loop closes on itself without a seam.
float perlinPeriodic(float x, float y, std::uint64_t seed, int periodX);

struct FractalOptions {
    int octaves = 5;
    /// How much each octave shrinks in amplitude. 0.5 is the usual choice.
    float gain = 0.5f;
    /// How much each octave grows in frequency.
    float lacunarity = 2.0f;
};

/// Sum of `octaves` Perlin octaves, normalised back into [-1, 1].
float fractalPerlin(float x, float y, std::uint64_t seed, const FractalOptions& options);

/// Musgrave-style ridges: each octave is folded about zero and sharpened, then
/// weighted by the octave above it so ridges only survive where the coarse
/// shape already had one. Returns [0, 1].
float ridged(float x, float y, std::uint64_t seed, const FractalOptions& options,
             float sharpness);

/**
 * One network of ridges, built from warped cell walls at unit spacing: 1 on a
 * crest, falling away into the catchment on either side. Returns [0, 1].
 *
 * Folded gradient noise cannot serve here. Its creases close into isolated
 * loops, and because gradient noise is exactly zero at every lattice corner
 * the fold is exactly 1 there, which lays a regular grid of dots over the
 * result. The walls of a cell diagram always join, and what they enclose is a
 * catchment — the structure a drainage divide actually has.
 *
 * This is one octave rather than a fractal because a caller stacking octaves
 * into a landform wants to read each one from a domain of its own, which no
 * fixed summation can offer.
 */
float ridgeOctave(float x, float y, std::uint64_t seed);

struct VoronoiSample {
    /// Distance to the nearest feature point, in noise units.
    float nearestDistance = 0.0f;
    /// Distance to the second nearest — the difference draws the cell walls.
    float secondDistance = 0.0f;
    /// A stable value in [0, 1) belonging to the nearest cell.
    float cellValue = 0.0f;
};

/// Jittered-grid cellular noise. `jitter` of 0 leaves the points on the grid.
VoronoiSample voronoi(float x, float y, std::uint64_t seed, float jitter);

/// The same, repeating every `periodX` cells along x, so a field read around
/// a loop closes on itself without a seam.
VoronoiSample voronoiPeriodic(float x, float y, std::uint64_t seed, float jitter, int periodX);

}  // namespace terrain::noise
