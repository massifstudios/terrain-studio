#pragma once

#include <filesystem>

#include "terrain/heightfield.h"
#include "terrain/point_set.h"

namespace terrain::formats {

/**
 * Point sets leave as plain tables. Consumers rasterise or instance them as
 * they please, so neither format carries anything about what the points are.
 *
 * Positions are written in the georeference's own frame when the graph has
 * one, so a point set lines up with the DEM it came from; without a
 * georeference they stay in graph-local meters.
 */
void writePointsCsv(const std::filesystem::path& path, const PointSet& points,
                    const GeoReference& georeference);

void writePointsJson(const std::filesystem::path& path, const PointSet& points,
                     const GeoReference& georeference);

}  // namespace terrain::formats
