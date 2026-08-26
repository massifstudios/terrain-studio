#include "terrain/point_set.h"

#include <algorithm>

namespace terrain {

void PointSet::canonicalise() {
    std::sort(points_.begin(), points_.end(),
              [](const TerrainPoint& left, const TerrainPoint& right) {
                  return left.id < right.id;
              });
}

}  // namespace terrain
