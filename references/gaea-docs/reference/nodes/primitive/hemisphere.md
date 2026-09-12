Source: https://docs.gaea.app/reference/nodes/primitive/hemisphere.html

# Hemisphere

Primitive › Gradient

Shortcode `do`

Generates a hemispherical gradient useful for domes, mounds, and radial falloffs.

Node must be baked when executing Hybrid or Tiled builds.

## When to Use It

Use Hemisphere when you need a hemispherical gradient useful for domes, mounds, and radial falloffs.

## Visual Direction

![](https://docs.gaea.app/.data/assets/Hemisphere_VD.png)

## Properties

**Scale** (`Single`)  
Controls the overall size of the hemisphere. Larger values increase the radius and footprint of the shape on the terrain.

**Height** (`Single`)  
Sets the maximum elevation of the hemisphere, determining how tall the dome rises above the base level.

**X** (`Single`)  
Adjusts the horizontal position of the hemisphere along the X axis, allowing you to move it left or right within the terrain.

**Y** (`Single`)  
Adjusts the horizontal position of the hemisphere along the Y axis, allowing you to move it forward or backward within the terrain.

**Flatten** (`Boolean`)  
Flattens the top of the hemisphere, creating a dome with a plateau or truncated appearance instead of a perfect curve.
