#pragma once

#include <cstddef>

namespace terrain::nodes {

/**
 * Water routed downhill over a whole surface. Every cell starts with one unit
 * of rain and passes what reached it on to every lower neighbour at once, in
 * proportion to how steeply each falls away, so channels branch the way a
 * hillside does rather than following the grid axes.
 *
 * The result is the units of rain that passed over each cell: at least one,
 * and orders of magnitude more on a valley floor. Cells are visited from the
 * highest down with ties broken on index, so the sum is the same on every
 * machine.
 */
void accumulateFlow(const float* height, int size, double cellSizeMeters, float concentration,
                    float* accumulation);

/// Flow on a log scale over [0, 1] against the strongest cell, raised by
/// `contrast` so the faint headwaters can be lifted towards the channels.
void normaliseFlow(const float* accumulation, std::size_t count, float contrast,
                   float* destination);

}  // namespace terrain::nodes
