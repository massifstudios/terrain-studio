Source: https://docs.gaea.app/reference/nodes/primitive/radialgradient.html

# RadialGradient

Primitive › Gradient

Shortcode `rg`

Generates a circular gradient radiating outward from a center point.

Node must be baked when executing Hybrid or Tiled builds.

## When to Use It

Use RadialGradient when you need a circular gradient radiating outward from a center point.

## Visual Direction

![](https://docs.gaea.app/.data/assets/RadialGradient_VD.png)

## Properties

**Scale** (`Single`)  
Controls the overall size of the radial gradient. Increasing the scale expands the gradient outward, affecting a larger area of the terrain.

**Height** (`Single`)  
Sets the maximum height or intensity at the center of the radial gradient. Higher values create a more pronounced peak in the center.

**X** (`Single`)  
Specifies the horizontal position of the gradient's center. Adjust this value to move the gradient left or right across the terrain.

**Y** (`Single`)  
Specifies the vertical position of the gradient's center. Adjust this value to move the gradient up or down across the terrain.
