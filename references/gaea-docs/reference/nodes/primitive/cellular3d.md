Source: https://docs.gaea.app/reference/nodes/primitive/cellular3d.html

# Cellular3D

Primitive › Extended

Shortcode `c3`

Generates three-dimensional cellular noise useful for volumetric-looking patterns and complex terrain breakup.

Node must be baked when executing Hybrid or Tiled builds.

## Official Video

For a video breakdown of this node, see [the official Cellular3D video](https://docs.gaea.app/videos/official/nodes/yt-cellular3d.html).

## When to Use It

Use Cellular3D when you want a volumetric-looking cellular pattern for breakup, cavities, or dense procedural structure that feels less flat than standard 2D cells.

## Visual Direction

![](https://docs.gaea.app/.data/assets/Celluar3D_VD.png)

## Properties

**Size** (`Single`)  
Controls the overall scale of the 3D cellular pattern. Higher values produce larger, more widely spaced cells.

**Gap** (`Single`)  
Sets the distance or separation between individual cells, affecting the openness and density of the pattern.

**Jitter X** (`Single`)  
Applies random offset to cell positions along the X axis, introducing irregularity and breaking uniformity.

**Jitter Y** (`Single`)  
Applies random offset to cell positions along the Y axis, adding variation and natural randomness.

**Jitter Z** (`Single`)  
Applies random offset to cell positions along the Z axis, further enhancing the organic appearance of the 3D pattern.

**Scale X** (`Single`)  
Adjusts the scale of the cellular pattern along the X axis, stretching or compressing cells horizontally.

**Scale Y** (`Single`)  
Adjusts the scale of the cellular pattern along the Y axis, stretching or compressing cells vertically.

**Scale Z** (`Single`)  
Adjusts the scale of the cellular pattern along the Z axis, stretching or compressing cells in depth.

**Seed** (`Int32`)  
Randomization seed for generating unique variations of the 3D cellular pattern. Changing the seed produces different cell arrangements.

**Normalize Input** (`Boolean`)  
If enabled, normalizes the input values to a standard range before applying the cellular pattern, ensuring consistent results regardless of input scale.
