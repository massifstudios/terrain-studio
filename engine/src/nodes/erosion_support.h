#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "terrain_node_sdk.h"

namespace terrain::nodes {

/* ---------------------------------------------------------- layer slump -- */

/// How many passes of neighbour-to-neighbour transport it takes for material
/// to spread a distance: passes grow with the square of the cell count, as
/// diffusion does, so a spread in meters means the same at any resolution.
int passesToSpread(double distanceMeters, double cellSizeMeters);

struct LayerSlump {
    /// The steepest slope the loose material holds on.
    double settleAngleDegrees = 30.0;
    /// How far the material spreads before it comes to rest.
    double travelMeters = 100.0;
    /// The share of the unstable excess that moves in each pass.
    float rate = 0.5f;
    /// Depth added evenly over the whole grid before each pass, so a layer can
    /// be released gradually rather than dropped at once. Zero releases nothing.
    float releasePerPassMeters = 0.0f;
};

/**
 * Lets a loose layer slide over the ground it lies on until nothing is
 * steeper than the settle angle. `surface` is rock plus layer and is what the
 * slopes are read from; `layer` is the loose depth and is all that moves, so
 * material slides off the rock without taking the rock with it. Both are
 * updated in place. A null `layer` slumps the surface itself, as thermal
 * erosion does. Runs on the GPU where one is vouched for, with the same bits
 * either way.
 */
void slumpLayer(const TerrainContextApi* api, TerrainEvaluationContext* context, float* surface,
                float* layer, const LayerSlump& slump);

/**
 * A grid of accumulated height changes, in fixed point.
 *
 * Droplets land wherever they land, so many workers add into the same cell at
 * once. Float addition is not associative, and float atomics would therefore
 * make the result depend on the order the hardware happened to schedule —
 * exactly what the determinism contract forbids. Integer addition is
 * associative, so an integer atomic gives the same total whatever the order.
 *
 * One unit is 10 micrometres, which leaves room for over 20 km of accumulated
 * change in a signed 32-bit cell: far more than any single pass produces, and
 * fine enough that a single droplet's bite does not round away.
 */
class FixedPointGrid {
public:
    explicit FixedPointGrid(std::size_t count);

    void add(std::size_t index, float meters);
    float valueAt(std::size_t index) const;
    /// The read back out of fixed point, for a count of units held elsewhere.
    static float metersFor(std::int32_t units);
    std::size_t size() const { return cells_.size(); }

    void clear();
    /// The largest absolute value in the grid, for normalising a mask output.
    float strongest() const;

    /// The raw fixed-point cells. A GPU backend accumulates into a buffer of
    /// exactly this layout and copies it back over these.
    std::int32_t* data() { return cells_.data(); }
    const std::int32_t* data() const { return cells_.data(); }

private:
    static constexpr float unitsPerMeter = 100000.0f;

    /// Plain storage reached through `std::atomic_ref` rather than an array of
    /// atomics, so the same bytes can be handed to a device.
    std::vector<std::int32_t> cells_;
};

/**
 * A weighted disc of cells over which a droplet works, so it bites a dent
 * rather than drilling a one-cell hole and settles a bank rather than a spike.
 *
 * The floor on the radius is not a detail. A footprint of one or two cells
 * concentrates everything a droplet carries into the cell it happens to be
 * standing on, and on gentle ground — where a channel is shallow and the
 * carried load is not — that builds towers instead of valleys. The requested
 * radius is in meters, so a coarse graph would otherwise ask for a fraction
 * of a cell and get exactly that failure.
 */
class ErosionBrush {
public:
    static constexpr int smallestRadius = 3;

    ErosionBrush(int radiusInCells, int gridSize);

    struct Sample {
        int offsetRow;
        int offsetColumn;
        float weight;
    };

    const std::vector<Sample>& samples() const { return samples_; }
    /// The radius actually in use, after the floor and the grid's ceiling.
    int radius() const { return radius_; }

private:
    int radius_ = smallestRadius;
    std::vector<Sample> samples_;
};

}  // namespace terrain::nodes
