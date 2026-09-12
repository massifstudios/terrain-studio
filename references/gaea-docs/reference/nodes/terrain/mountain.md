Source: https://docs.gaea.app/reference/nodes/terrain/mountain.html

# Mountain

Terrain › Primitive

Shortcode `m`

Mountain is one of Gaea's most versatile primitives, using a modulated Voronoi pattern and distortions to create realistic mountain shapes, ready for further modification and erosion.

Node must be baked when executing Hybrid or Tiled builds.

Mountain is one of the most versatile primitives in Gaea. It uses a modulated Voronoi pattern and distortions to create realistic mountain shapes, ready for further modification and erosion.

![](https://docs.gaea.app/.data/ref/Mountain/Mountain.webp)

For simple results, such as distant mountains that don't require much detail, you can create a Mountain primitive with a scale of your choice and add the Erosion node. This is the easiest way to create a lot of simple assets.

## When to Use It

Use Mountain when you need a fast large-scale landform that can anchor the rest of the terrain graph.

## Visual Direction

![](https://docs.gaea.app/.data/assets/Mountain_VD.png)

## Properties

**Scale** (`Single`)  
Controls the overall size and extent of the mountain. Higher values create broader, more expansive mountain shapes.

**Height** (`Single`)  
Sets the maximum elevation of the mountain, determining how tall the peak rises above the terrain.

**Style** (`Enum`)  
Specifies the geological and visual style of the mountain, affecting its shape, erosion, and surface features.

- **Basic** — Generates a simple, unmodified mountain with minimal erosion or geological detail.
- **Eroded** — Applies erosion effects to the mountain, creating realistic weathering, gullies, and worn surfaces.
- **Old** — Simulates an ancient mountain with softened features, rounded slopes, and significant erosion.
- **Alpine** — Creates sharp, rugged alpine features with steep faces and dramatic relief, typical of young mountain ranges.
- **Strata** — Adds visible geological strata or layers to the mountain, emphasizing sedimentary or volcanic banding.

**Bulk** (`Enum`)  
Controls the mass and volume of the mountain, affecting how thick or substantial the formation appears.

- **Low** — Produces slender, less massive mountains with more delicate features.
- **Medium** — Creates mountains with a balanced amount of mass, offering a natural appearance.
- **High** — Generates thick, heavy mountains with substantial volume and broad bases.

**Reduce Details** (`Boolean`)  
Reduces the amount of fine surface detail on the mountain, resulting in a smoother, more simplified shape.

**Seed** (`Int32`)  
Randomization seed for generating unique variations of the mountain. Changing the seed produces different shapes and arrangements.

**X** (`Single`)  
Offsets the mountain horizontally along the X axis, shifting its position within the terrain.

**Y** (`Single`)  
Offsets the mountain horizontally along the Y axis, shifting its position within the terrain.
