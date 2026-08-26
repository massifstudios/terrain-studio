#include "terrain/resample.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace terrain {
namespace {

int clampIndex(int index, int limit) {
    return std::clamp(index, 0, limit - 1);
}

double blockAverage(const Heightfield& source, double rowStart, double rowEnd, double columnStart,
                    double columnEnd) {
    const int firstRow = clampIndex(static_cast<int>(rowStart), source.height());
    const int lastRow = clampIndex(static_cast<int>(std::ceil(rowEnd)) - 1, source.height());
    const int firstColumn = clampIndex(static_cast<int>(columnStart), source.width());
    const int lastColumn = clampIndex(static_cast<int>(std::ceil(columnEnd)) - 1, source.width());

    double total = 0.0;
    int counted = 0;
    for (int row = firstRow; row <= lastRow; ++row) {
        for (int column = firstColumn; column <= lastColumn; ++column) {
            total += source.at(row, column);
            counted += 1;
        }
    }
    return total / counted;
}

}  // namespace

float sampleBilinear(const Heightfield& field, double row, double column) {
    const double clampedRow = std::clamp(row, 0.0, static_cast<double>(field.height() - 1));
    const double clampedColumn = std::clamp(column, 0.0, static_cast<double>(field.width() - 1));

    const int rowLow = static_cast<int>(std::floor(clampedRow));
    const int columnLow = static_cast<int>(std::floor(clampedColumn));
    const int rowHigh = clampIndex(rowLow + 1, field.height());
    const int columnHigh = clampIndex(columnLow + 1, field.width());

    const double rowFraction = clampedRow - rowLow;
    const double columnFraction = clampedColumn - columnLow;

    const double top = field.at(rowLow, columnLow) * (1.0 - columnFraction)
                       + field.at(rowLow, columnHigh) * columnFraction;
    const double bottom = field.at(rowHigh, columnLow) * (1.0 - columnFraction)
                          + field.at(rowHigh, columnHigh) * columnFraction;
    return static_cast<float>(top * (1.0 - rowFraction) + bottom * rowFraction);
}

Heightfield downsampleAverage(const Heightfield& source, int size) {
    if (source.isEmpty()) {
        throw std::invalid_argument("Cannot downsample an empty heightfield");
    }

    Heightfield target(size, size);
    target.setCellSizeMeters(source.cellSizeMeters() * source.width() / size);
    target.setSemantics(source.semantics());

    const double rowScale = static_cast<double>(source.height()) / size;
    const double columnScale = static_cast<double>(source.width()) / size;

    for (int row = 0; row < size; ++row) {
        for (int column = 0; column < size; ++column) {
            target.at(row, column) = static_cast<float>(
                blockAverage(source, row * rowScale, (row + 1) * rowScale, column * columnScale,
                             (column + 1) * columnScale));
        }
    }
    return target;
}

Heightfield resampleToSquare(const Heightfield& source, int resolution, double worldSizeMeters) {
    if (source.isEmpty()) {
        throw std::invalid_argument("Cannot resample an empty heightfield");
    }

    Heightfield target(resolution, resolution);
    target.setCellSizeMeters(worldSizeMeters / resolution);
    target.setGeoreference(source.georeference());
    target.setSemantics(source.semantics());

    const double rowScale = static_cast<double>(source.height()) / resolution;
    const double columnScale = static_cast<double>(source.width()) / resolution;

    for (int row = 0; row < resolution; ++row) {
        for (int column = 0; column < resolution; ++column) {
            const double sourceRow = (row + 0.5) * rowScale - 0.5;
            const double sourceColumn = (column + 0.5) * columnScale - 0.5;
            target.at(row, column) = sampleBilinear(source, sourceRow, sourceColumn);
        }
    }
    return target;
}

}  // namespace terrain
