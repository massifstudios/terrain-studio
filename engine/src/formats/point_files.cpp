#include "terrain/formats/point_files.h"

#include <fstream>
#include <stdexcept>

#include "terrain/json_writer.h"

namespace terrain::formats {
namespace {

struct WorldOffset {
    double easting = 0.0;
    double northing = 0.0;
};

WorldOffset offsetFor(const GeoReference& georeference) {
    if (!georeference.isPresent()) {
        return WorldOffset{};
    }
    // The graph's northing grows northwards from its south edge, while the
    // georeference names the north-west corner, so the two meet at the top.
    return WorldOffset{georeference.eastingMinMeters, georeference.northingMaxMeters};
}

std::ofstream openForWriting(const std::filesystem::path& path) {
    std::ofstream file(path, std::ios::trunc);
    if (!file) {
        throw std::runtime_error("Cannot write " + path.string());
    }
    return file;
}

}  // namespace

void writePointsCsv(const std::filesystem::path& path, const PointSet& points,
                    const GeoReference& georeference) {
    const WorldOffset offset = offsetFor(georeference);
    std::ofstream file = openForWriting(path);
    file << "x_meters,y_meters,altitude_meters,rotation_degrees,scale,id\n";

    for (const TerrainPoint& point : points.points()) {
        file << (offset.easting + point.eastingMeters) << ','
             << (offset.northing - point.northingMeters) << ',' << point.altitudeMeters << ','
             << point.rotationDegrees << ',' << point.scale << ',' << point.id << '\n';
    }
}

void writePointsJson(const std::filesystem::path& path, const PointSet& points,
                     const GeoReference& georeference) {
    const WorldOffset offset = offsetFor(georeference);
    JsonWriter writer;
    writer.beginObject();
    writer.member("coordinateSystem", georeference.coordinateSystem);
    writer.member("count", static_cast<long long>(points.size()));
    writer.key("points");
    writer.beginArray();

    for (const TerrainPoint& point : points.points()) {
        writer.beginObject();
        writer.member("x", offset.easting + point.eastingMeters);
        writer.member("y", offset.northing - point.northingMeters);
        writer.member("altitude", point.altitudeMeters);
        writer.member("rotation", point.rotationDegrees);
        writer.member("scale", point.scale);
        writer.member("id", static_cast<long long>(point.id));
        writer.endObject();
    }
    writer.endArray();
    writer.endObject();

    std::ofstream file = openForWriting(path);
    file << writer.text();
}

}  // namespace terrain::formats
