Source: https://docs.gaea.app/reference/nodes/terrain/mountainside.html

# MountainSide

Terrain › Primitive

Shortcode `ms`

Generates a single mountain slope or mountainside formation.

Node must be baked when executing Hybrid or Tiled builds.

## When to Use It

Use MountainSide when you need a single mountain slope or mountainside formation.

## Visual Direction

![](https://docs.gaea.app/.data/assets/MountainSide_VD.png)

## Properties

**Scale** (`Single`)  
Controls the overall size and extent of the mountain slope. Larger values create broader, more expansive slopes.

**Detail** (`Single`)  
Adjusts the amount of fine surface detail on the mountain. Higher values add more texture and small-scale features to the slope.

**Type** (`Enum`)  
Determines the primary form of the mountain feature to generate.

- **Slope** — Creates a continuous, sloping mountainside, ideal for simulating the flanks of large mountain ranges.
- **Peak** — Generates a prominent mountain peak, focusing elevation and detail toward a summit.

**Style** (`Enum`)  
Specifies the visual and geological style of the mountain slope, affecting erosion, layering, and surface character.

- **Basic** — Produces a simple, unmodified slope or peak with minimal erosion or geological features.
- **Eroded** — Applies erosion effects to the mountain, creating realistic weathering, gullies, and worn surfaces.
- **Old** — Simulates an ancient mountain with softened features, rounded slopes, and significant erosion.
- **Alpine** — Creates sharp, rugged alpine features with steep faces and dramatic relief, typical of young mountain ranges.
- **Strata** — Adds visible geological strata or layers to the mountain, emphasizing sedimentary or volcanic banding.

**Direction** (`Int32`)  
Sets the orientation of the slope or peak, determining which direction the mountain face or ridge is aligned.

**Seed** (`Int32`)  
Defines the randomization seed for the node, ensuring reproducible or varied mountain shapes with each value.
