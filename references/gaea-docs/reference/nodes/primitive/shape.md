Source: https://docs.gaea.app/reference/nodes/primitive/shape.html

# Shape

Primitive › Basic

Shortcode `s`

Generates basic geometric terrain shapes for use as primitives or masks.

Node must be baked when executing Hybrid or Tiled builds.

## When to Use It

Use Shape when you need basic geometric terrain shapes for use as primitives or masks.

## Visual Direction

![](https://docs.gaea.app/.data/assets/Shape_VD.png)

## Properties

**Geometry** (`Enum`)  
Specifies the base geometric shape used for the node. Determines the fundamental outline or profile of the generated object.

- **Circle** — Generates a circular shape as the base geometry, resulting in round or elliptical forms.
- **Rectangle** — Generates a rectangular shape as the base geometry, resulting in box-like or elongated forms.

**Sides** (`Int32`)  
Defines the number of sides for polygonal shapes. Higher values create shapes closer to a circle, while lower values produce more angular forms.

**Uniform** (`Boolean`)  
When enabled, ensures that scaling and transformations are applied equally in all directions, maintaining the original proportions of the shape.

**Scale** (`Single`)  
Controls the overall size of the shape, scaling it uniformly along both the X and Y axes.

**Scale X** (`Single`)  
Adjusts the width of the shape by scaling it along the X axis independently.

**Scale Y** (`Single`)  
Adjusts the height of the shape by scaling it along the Y axis independently.

**Thickness** (`Single`)  
Specifies the thickness of the shape's outline or border, or the width of a ring for hollow shapes.

**Height** (`Single`)  
Sets the vertical extent or elevation of the shape, often used to define extrusion or relief in terrain generation.

**X** (`Single`)  
Sets the horizontal position or offset of the shape along the X axis.

**Y** (`Single`)  
Sets the vertical position or offset of the shape along the Y axis.
