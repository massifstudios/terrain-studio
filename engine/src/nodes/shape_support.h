#pragma once

#include "node_support.h"

namespace terrain::nodes {

/**
 * Cell centres in world meters, measured from the south-west corner of the
 * graph square. Row 0 is north, so northing counts down from the top edge.
 *
 * Analytic shapes are placed in this frame rather than in cells, so the same
 * graph draws the same landscape at any resolution.
 */
struct WorldFrame {
    int size = 0;
    double cellSizeMeters = 1.0;

    double extentMeters() const;
    double eastingAt(int column) const;
    double northingAt(int row) const;
};

WorldFrame worldFrameFor(const TerrainContextApi* api, TerrainEvaluationContext* context);

/// A point measured from a shape's centre, along the shape's own axes.
struct LocalPoint {
    double alongWidth = 0.0;
    double alongDepth = 0.0;

    double distanceFromCentre() const;
    double angleRadians() const;
};

/**
 * Where a shape sits and how far it is turned. The centre arrives as a
 * fraction of the world square, because a composition should survive a change
 * of world size; every other measurement a shape takes is in meters.
 */
struct ShapePlacement {
    double centreEastingMeters = 0.0;
    double centreNorthingMeters = 0.0;
    double cosine = 1.0;
    double sine = 0.0;

    LocalPoint localPoint(double eastingMeters, double northingMeters) const;
};

ShapePlacement placementFrom(const WorldFrame& frame, double centreEastFraction,
                             double centreNorthFraction, double rotationDegrees);

/* ------------------------------------------------------ signed distances -- */

/// All of these are meters from the shape's edge, negative inside it.
double discDistance(const LocalPoint& point, double radiusMeters);
double squareDistance(const LocalPoint& point, double halfWidthMeters);
double ringDistance(const LocalPoint& point, double radiusMeters, double widthMeters);
double polygonDistance(const LocalPoint& point, double radiusMeters, int sideCount);

/// Distance from a point to the segment between two others, in meters.
double segmentDistance(double eastingMeters, double northingMeters, double firstEasting,
                       double firstNorthing, double secondEasting, double secondNorthing);

/// 1 inside the shape, 0 further than `falloffMeters` outside it, smooth
/// between. A falloff of 0 leaves a hard edge.
float insideWeight(double signedDistanceMeters, double falloffMeters);

/* --------------------------------------------------------------- profiles -- */

enum class RadialProfile { Linear = 0, Dome = 1, Smooth = 2 };

/// 1 at the centre, 0 at the radius and beyond.
float radialProfileValue(RadialProfile profile, double distanceMeters, double radiusMeters);

enum class PatternKind { Checker = 0, Stripes = 1, Grid = 2, Bricks = 3, Dots = 4 };

/**
 * Meters from the edge of the pattern's raised part, negative inside it.
 * Checkers, stripes and dots raise areas; grids and bricks raise the lines
 * between them, and read `lineWidthMeters` as that line's width — or, for
 * dots, as their diameter.
 */
double patternDistance(PatternKind kind, const LocalPoint& point, double featureSizeMeters,
                       double lineWidthMeters);

}  // namespace terrain::nodes
