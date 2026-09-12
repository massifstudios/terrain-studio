Source: https://docs.gaea.app/reference/nodes/surface/contours.html

# Contours

Surface › Cartography

Shortcode `cn`

Creates map-like contours for a terrain. Useful for making maps or for other overlays.

## When to Use It

Use Contours when you need map-like contours for a terrain.

## Visual Direction

## Properties

**Count** (`Int32`)  
Sets the number of main contour lines to generate. Higher values create more elevation bands across the terrain.

**Subdivision** (`Int32`)  
Controls the number of subdivisions between main contour lines, adding finer detail and intermediate lines.

**Accuracy** (`Single`)  
Adjusts the precision of contour line placement. Higher accuracy produces more exact lines that closely follow the terrain's elevation.

**Line Intensity** (`Single`)  
Controls the brightness or opacity of the main contour lines, making them more or less prominent in the output.

**Sub Intensity** (`Single`)  
Adjusts the brightness or opacity of the subdivision lines, which are the finer lines between main contours.

**Line Thickness** (`Int32`)  
Specifies the thickness of the contour lines. Higher values produce bolder, wider lines.
