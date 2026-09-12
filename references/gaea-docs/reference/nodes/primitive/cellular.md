Source: https://docs.gaea.app/reference/nodes/primitive/cellular.html

# Cellular

Primitive › Extended

Shortcode `ll`

Cellular creates cell-based patterns and noises that can be quite versatile as they're ever-present in nature.

Node must be baked when executing Hybrid or Tiled builds.

## When to Use It

Use Cellular when you need cell-based patterns and noises that can be quite versatile as they're ever-present in nature.

## Visual Direction

![](https://docs.gaea.app/.data/assets/Cellular_VD.png)

## Properties

**Size** (`Single`)  
Controls the overall scale of the cellular pattern. Higher values produce smaller cells (finer detail); lower values produce larger cells (coarser detail).

**Density** (`Int32`)  
Controls how many feature points are generated per area. Higher density creates more, smaller cells; lower density creates fewer, larger cells.

**Jitter** (`Single`)  
Randomizes feature-point placement. Low values are more uniform/structured; higher values create more organic, irregular cells.

**Metric Morph** (`Single`)  
Blends between the paired distance behaviors implied by the selected Distance Metric (e.g., Euclidian ↔ Manhattan/Chebyshev), changing cell geometry continuously.

**Distance Metric** (`Enum`)  
Selects the distance-space used to evaluate the cellular field, affecting the fundamental shape of cells and borders.

- **Euclidian Manhattan** — Morph between rounded Euclidian cells and diamond-like Manhattan cells (axis-aligned, sharper corners) using Metric Morph.
- **Euclidian Chebyshev** — Morph between rounded Euclidian cells and square-like Chebyshev cells using Metric Morph.
- **Minkowski** — Generalized metric that spans a continuous family of distance shapes (useful for smoothly varying from rounded to more boxy forms).

**Coordinate Axes Jitter** (`Single`)  
Adds jitter in coordinate/axis space before evaluation, warping the field and introducing directional wobble and boundary distortion.

**CAJ Clustering** (`Single`)  
Controls how Coordinate Axes Jitter groups together. Low values spread distortion evenly; higher values create patchy/clumped distortion regions.

**Anisotropy** (`Single`)  
Stretches the pattern directionally, producing elongated/elliptical cells and emphasizing a dominant direction.

**Distance Function** (`Enum`)  
Defines what value is output from the cellular field (cell IDs, distances to points, borders, or edges).

- **Hash** — Outputs a stable random value per cell region (cell ID-style mask), useful for random patching, selection, and variation.
- **Distance Inv** — Outputs inverse distance to the nearest feature point (bright centers fading toward borders).
- **Distance 2 Inv** — Outputs inverse distance to the second-nearest feature point, producing a different internal falloff useful for variation and breakup.
- **Distance Diff** — Outputs the difference between nearest distances (classic Voronoi edge emphasis), great for ridges, veins, and cell borders.
- **Distance to Edge** — Outputs distance to the nearest cell border, ideal for crack networks and edge masks when remapped.

**Seed** (`Int32`)  
Changes the random distribution of feature points without changing other settings.
