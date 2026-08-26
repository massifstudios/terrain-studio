#pragma once

#include <cstdint>
#include <vector>

#include "terrain_node_sdk.h"

namespace terrain {

/**
 * A flat list of scattered instances — trees, boulders, whatever the consumer
 * decides. Positions are in world meters on the graph's own frame, so a writer
 * can offset them by the georeference without every node having to.
 *
 * The engine sorts a finished set by id. Ids are seed-derived and unique, so
 * the order is canonical however the node emitted them — which is what keeps a
 * point set reproducible even if a node scatters from several threads.
 */
class PointSet {
public:
    void add(const TerrainPoint& point) { points_.push_back(point); }

    /// Puts the set into its canonical order. Called once, by the evaluator.
    void canonicalise();

    const std::vector<TerrainPoint>& points() const { return points_; }
    std::size_t size() const { return points_.size(); }
    bool isEmpty() const { return points_.empty(); }

private:
    std::vector<TerrainPoint> points_;
};

}  // namespace terrain
