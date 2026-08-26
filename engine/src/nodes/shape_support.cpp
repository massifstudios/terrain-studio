#include "shape_support.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace terrain::nodes {
namespace {

double fractionOf(double value) {
    return value - std::floor(value);
}

double distanceToNearestInteger(double value) {
    const double fraction = fractionOf(value);
    return std::min(fraction, 1.0 - fraction);
}

/// Which cell of a repeating pattern a coordinate falls in, with a parity that
/// keeps alternating across zero rather than mirroring about it.
bool isEvenCell(double value) {
    const long long cell = static_cast<long long>(std::floor(value));
    return ((cell % 2) + 2) % 2 == 0;
}

/// Negative inside the filled cells of a two-colour tiling, positive outside,
/// with the magnitude being the distance to the nearest cell edge.
double alternatingDistance(bool filled, double edgeDistance) {
    if (filled) {
        return -edgeDistance;
    }
    return edgeDistance;
}

double checkerDistance(double alongWidth, double alongDepth) {
    const double edgeDistance =
        std::min(distanceToNearestInteger(alongWidth), distanceToNearestInteger(alongDepth));
    const bool filled = isEvenCell(alongWidth) == isEvenCell(alongDepth);
    return alternatingDistance(filled, edgeDistance);
}

double stripeDistance(double alongWidth) {
    return alternatingDistance(isEvenCell(alongWidth), distanceToNearestInteger(alongWidth));
}

double gridDistance(double alongWidth, double alongDepth, double halfLine) {
    const double toLine =
        std::min(distanceToNearestInteger(alongWidth), distanceToNearestInteger(alongDepth));
    return toLine - halfLine;
}

/// Every other course is shifted half a brick, so the head joints stagger the
/// way a running bond does.
double brickDistance(double alongWidth, double alongDepth, double halfLine) {
    const double staggered = alongWidth + 0.5 * std::floor(alongDepth);
    const double toJoint =
        std::min(distanceToNearestInteger(staggered), distanceToNearestInteger(alongDepth));
    return toJoint - halfLine;
}

double dotDistance(double alongWidth, double alongDepth, double radius) {
    const double offsetWidth = fractionOf(alongWidth) - 0.5;
    const double offsetDepth = fractionOf(alongDepth) - 0.5;
    return std::hypot(offsetWidth, offsetDepth) - radius;
}

double patternDistanceInCells(PatternKind kind, double alongWidth, double alongDepth,
                              double halfLine) {
    if (kind == PatternKind::Stripes) {
        return stripeDistance(alongWidth);
    }
    if (kind == PatternKind::Grid) {
        return gridDistance(alongWidth, alongDepth, halfLine);
    }
    if (kind == PatternKind::Bricks) {
        return brickDistance(alongWidth, alongDepth, halfLine);
    }
    if (kind == PatternKind::Dots) {
        return dotDistance(alongWidth, alongDepth, halfLine);
    }
    return checkerDistance(alongWidth, alongDepth);
}

}  // namespace

double WorldFrame::extentMeters() const {
    return size * cellSizeMeters;
}

double WorldFrame::eastingAt(int column) const {
    return (column + 0.5) * cellSizeMeters;
}

double WorldFrame::northingAt(int row) const {
    return (size - row - 0.5) * cellSizeMeters;
}

WorldFrame worldFrameFor(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    return WorldFrame{api->width(context), api->cellSizeMeters(context)};
}

double LocalPoint::distanceFromCentre() const {
    return std::hypot(alongWidth, alongDepth);
}

double LocalPoint::angleRadians() const {
    return std::atan2(alongDepth, alongWidth);
}

LocalPoint ShapePlacement::localPoint(double eastingMeters, double northingMeters) const {
    const double eastOffset = eastingMeters - centreEastingMeters;
    const double northOffset = northingMeters - centreNorthingMeters;
    return LocalPoint{eastOffset * cosine + northOffset * sine,
                      northOffset * cosine - eastOffset * sine};
}

ShapePlacement placementFrom(const WorldFrame& frame, double centreEastFraction,
                             double centreNorthFraction, double rotationDegrees) {
    const double radians = rotationDegrees * std::numbers::pi / 180.0;
    ShapePlacement placement;
    placement.centreEastingMeters = centreEastFraction * frame.extentMeters();
    placement.centreNorthingMeters = centreNorthFraction * frame.extentMeters();
    placement.cosine = std::cos(radians);
    placement.sine = std::sin(radians);
    return placement;
}

double discDistance(const LocalPoint& point, double radiusMeters) {
    return point.distanceFromCentre() - radiusMeters;
}

double squareDistance(const LocalPoint& point, double halfWidthMeters) {
    const double outsideWidth = std::abs(point.alongWidth) - halfWidthMeters;
    const double outsideDepth = std::abs(point.alongDepth) - halfWidthMeters;
    const double beyond = std::hypot(std::max(outsideWidth, 0.0), std::max(outsideDepth, 0.0));
    const double within = std::min(std::max(outsideWidth, outsideDepth), 0.0);
    return beyond + within;
}

double ringDistance(const LocalPoint& point, double radiusMeters, double widthMeters) {
    return std::abs(point.distanceFromCentre() - radiusMeters) - widthMeters * 0.5;
}

/// The polygon is regular and `radiusMeters` is its apothem, so a side count
/// raised towards infinity converges on a disc of the same radius.
double polygonDistance(const LocalPoint& point, double radiusMeters, int sideCount) {
    const double sides = static_cast<double>(std::max(sideCount, 3));
    const double sector = 2.0 * std::numbers::pi / sides;
    const double angle = point.angleRadians();
    const double folded = angle - sector * std::floor(angle / sector + 0.5);
    return point.distanceFromCentre() * std::cos(folded) - radiusMeters;
}

double segmentDistance(double eastingMeters, double northingMeters, double firstEasting,
                       double firstNorthing, double secondEasting, double secondNorthing) {
    const double runEast = secondEasting - firstEasting;
    const double runNorth = secondNorthing - firstNorthing;
    const double lengthSquared = runEast * runEast + runNorth * runNorth;
    if (lengthSquared <= 0.0) {
        return std::hypot(eastingMeters - firstEasting, northingMeters - firstNorthing);
    }

    const double projection =
        ((eastingMeters - firstEasting) * runEast + (northingMeters - firstNorthing) * runNorth)
        / lengthSquared;
    const double along = std::clamp(projection, 0.0, 1.0);
    return std::hypot(eastingMeters - (firstEasting + runEast * along),
                      northingMeters - (firstNorthing + runNorth * along));
}

float insideWeight(double signedDistanceMeters, double falloffMeters) {
    if (signedDistanceMeters <= 0.0) {
        return 1.0f;
    }
    if (falloffMeters <= 0.0) {
        return 0.0f;
    }
    return smoothStep(static_cast<float>(1.0 - signedDistanceMeters / falloffMeters));
}

float radialProfileValue(RadialProfile profile, double distanceMeters, double radiusMeters) {
    if (radiusMeters <= 0.0) {
        return 0.0f;
    }
    const double unit = std::clamp(1.0 - distanceMeters / radiusMeters, 0.0, 1.0);
    if (profile == RadialProfile::Dome) {
        return static_cast<float>(std::sqrt(unit * (2.0 - unit)));
    }
    if (profile == RadialProfile::Smooth) {
        return smoothStep(static_cast<float>(unit));
    }
    return static_cast<float>(unit);
}

double patternDistance(PatternKind kind, const LocalPoint& point, double featureSizeMeters,
                       double lineWidthMeters) {
    const double cellSize = std::max(featureSizeMeters, 1.0e-3);
    const double halfLine = lineWidthMeters * 0.5 / cellSize;
    const double inCells = patternDistanceInCells(kind, point.alongWidth / cellSize,
                                                  point.alongDepth / cellSize, halfLine);
    return inCells * cellSize;
}

}  // namespace terrain::nodes
