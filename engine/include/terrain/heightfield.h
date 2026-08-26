#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <vector>

namespace terrain {

/// Source georeference of an imported window, carried to export untouched.
struct GeoReference {
    std::string coordinateSystem;
    double eastingMinMeters = 0.0;
    double northingMaxMeters = 0.0;

    bool isPresent() const { return !coordinateSystem.empty(); }
};

/// What a field means to the viewer. Storage is identical either way.
enum class FieldSemantics {
    Heightfield,
    Mask
};

/**
 * Row-major float32 grid. Row 0 is north, matching `.massifdem`. Heights are
 * meters above the georeference datum; masks reuse the same storage with
 * values conventionally in [0, 1].
 */
class Heightfield {
public:
    Heightfield() = default;
    Heightfield(int width, int height);

    int width() const { return width_; }
    int height() const { return height_; }
    bool isEmpty() const { return samples_.empty(); }
    std::size_t sampleCount() const { return samples_.size(); }

    float& at(int row, int column);
    float at(int row, int column) const;

    float* data() { return samples_.data(); }
    const float* data() const { return samples_.data(); }

    std::span<float> samples() { return samples_; }
    std::span<const float> samples() const { return samples_; }

    double cellSizeMeters() const { return cellSizeMeters_; }
    void setCellSizeMeters(double cellSize) { cellSizeMeters_ = cellSize; }

    double worldWidthMeters() const { return width_ * cellSizeMeters_; }
    double worldHeightMeters() const { return height_ * cellSizeMeters_; }

    const GeoReference& georeference() const { return georeference_; }
    void setGeoreference(GeoReference reference) { georeference_ = std::move(reference); }

    FieldSemantics semantics() const { return semantics_; }
    void setSemantics(FieldSemantics semantics) { semantics_ = semantics; }

    void fill(float value);
    float minimum() const;
    float maximum() const;

private:
    int width_ = 0;
    int height_ = 0;
    double cellSizeMeters_ = 1.0;
    GeoReference georeference_;
    FieldSemantics semantics_ = FieldSemantics::Heightfield;
    std::vector<float> samples_;
};

}  // namespace terrain
