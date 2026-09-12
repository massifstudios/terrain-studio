Source: https://docs.gaea.app/reference/nodes/terrain/mountainrange.html

# MountainRange

Terrain › Landscape

Shortcode `mr`

Range primitive quickly sets up an entire mountain terrain with shapes highly optimized for eroding large-scale landscapes.

Node must be baked when executing Hybrid or Tiled builds.

## When to Use It

Use MountainRange when you need range primitive quickly sets up an entire mountain terrain with shapes highly optimized for eroding large-scale landscapes.

## Visual Direction

![](https://docs.gaea.app/.data/assets/MountainRange_VD.png)

## Properties

**Scale** (`Single`)  
Controls the overall size and extent of the mountain ranges. Higher values create broader, more expansive ranges.

**Height** (`Single`)  
Sets the maximum elevation of the mountain range, determining how tall the peaks and ridges will be.

**Style** (`Enum`)  
Specifies the geological and visual style of the mountain range, affecting its shape, erosion, and surface features.

- **Basic** — Generates a simple, unmodified mountain range with minimal erosion or geological detail.
- **Eroded** — Applies erosion effects to the mountain range, creating realistic weathering, gullies, and worn surfaces.
- **Stratified** — Adds visible geological strata or layers to the mountains, simulating sedimentary or volcanic banding.
- **Alpine** — Creates sharp, rugged alpine features with steep faces and dramatic relief, typical of young mountain ranges.

**Bulk** (`Enum`)  
Controls the mass and volume of the mountain range, affecting how thick or substantial the mountains appear.

- **Low** — Produces slender, less massive mountains with more delicate features.
- **Medium** — Creates mountains with a balanced amount of mass, offering a natural appearance.
- **High** — Generates thick, heavy mountains with substantial volume and broad bases.

**Seed** (`Int32`)  
Randomization seed for generating unique variations of the mountain range. Changing the seed produces different shapes and arrangements.
