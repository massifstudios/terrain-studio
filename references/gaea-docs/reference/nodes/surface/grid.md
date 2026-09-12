Source: https://docs.gaea.app/reference/nodes/surface/grid.html

# Grid

Surface › Cartography

Shortcode `gr`

Generates a regular grid pattern useful for cartography, masking, or stylized surfaces.

Node must be baked when executing Hybrid or Tiled builds.

## When to Use It

Use Grid when you need a regular grid pattern useful for cartography, masking, or stylized surfaces.

## Visual Direction

## Properties

**Subdivision** (`Int32`)  
Sets the number of grid divisions along each axis. Higher values create a denser grid with more cells.

**Line Intensity** (`Single`)  
Controls the brightness or opacity of the main grid lines, making them more or less prominent in the texture.

**Sub Intensity** (`Single`)  
Adjusts the brightness or opacity of the subdivision lines, which are the finer lines between main grid lines.

**Line Thickness** (`Int32`)  
Specifies the thickness of the grid lines. Higher values produce bolder, wider lines.
