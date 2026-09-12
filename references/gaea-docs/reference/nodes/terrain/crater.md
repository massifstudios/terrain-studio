Source: https://docs.gaea.app/reference/nodes/terrain/crater.html

# Crater

Terrain › Primitive

Shortcode `cra`

Crater primitive creates different types of crater shapes.

Node must be baked when executing Hybrid or Tiled builds.

The Crater primitive creates different types of crater shapes. Like all primitives, it is not meant to be the finished product, but rather a starting point from which to develop your own detailed shape.

Craters can be either *volcanic*, which start at the base and go up in a conical shape; or they can be *impact*, where the inside of the crater can be deeper than the base or the ground around the crater.

![](https://docs.gaea.app/.data/ref/Crater/Crater--Impact.webp)

Impact Crater: The base is higher than the "ground" as the impact crater goes below the surface.

![](https://docs.gaea.app/.data/ref/Crater/Crater--Volcanic.webp)

Volcanic Crater: The base is a flat area and easier to combine with existing terrains.

## When to Use It

Use Crater when you need a controllable volcanic or impact crater base that can be developed further with erosion, blending, or surface detail.

## Visual Direction

![](https://docs.gaea.app/.data/assets/Crater_VD.png)

## Properties

**Style** (`Enum`)  
Selects the overall appearance and shaping method for the crater.

- **New** — Uses the modern crater generation algorithm, producing more realistic and detailed crater shapes.
- **Classic** — Applies the legacy crater style, creating traditional, stylized crater forms.

**Scale** (`Single`)  
Controls the overall size of the crater. Higher values produce larger craters.

**Formation** (`Single`)  
Determines the formation characteristics of the crater, such as depth, width, and rim prominence.

**Height** (`Single`)  
Sets the maximum elevation or prominence of the crater above the base terrain.

**Rim** (`Enum`)  
Specifies the sharpness and profile of the crater's rim.

- **Soft** — Creates a gently sloping, eroded rim for a more weathered appearance.
- **Sharp** — Produces a steep, well-defined rim for a freshly formed or less eroded crater.

**Shape** (`Single`)  
Defines the overall outline and contour of the crater, such as roundness or irregularity.

**X** (`Single`)  
Horizontal placement of the crater or crater field within the terrain.

**Y** (`Single`)  
Vertical placement of the crater or crater field within the terrain.

**Seed** (`Int32`)  
The randomization pattern or seed for the node's process. Changing the seed produces different crater variations.
