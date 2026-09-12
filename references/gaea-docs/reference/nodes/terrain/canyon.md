Source: https://docs.gaea.app/reference/nodes/terrain/canyon.html

# Canyon

Terrain › Landscape

Shortcode `ny`

Canyon is a simple, fast, and detailed canyon primitive. It creates a drainage based river canyon.

Node must be baked when executing Hybrid or Tiled builds.

## When to Use It

Use Canyon when you need a simple, fast, and detailed canyon primitive.

## Visual Direction

![](https://docs.gaea.app/.data/assets/Canyon_VD.png)

## Properties

**Style** (`Enum`)  
Selects the canyon shaping model used to generate the canyon form and surface character.

- **Classic** — Clean, procedural canyon cuts with a simple, carved profile.
- **Eroded** — Adds erosion-like breakup and natural irregularities to the canyon walls and floor.
- **Eroded 2** — A more advanced eroded style with richer wall detail and more natural variation.
- **Strata** — Introduces stratified, layered wall features resembling sedimentary rock bands.
- **Both** — Combines erosion breakup with stratified layering for a layered-and-worn look.

**Scale** (`Single`)  
The scale of the canyon.

**Slot** (`Single`)  
Controls the narrow, slot-canyon component - tightness and emphasis of the deepest cut.

**Valley** (`Single`)  
Controls the broader valley form - overall widening and openness of the canyon.

**Surrounding** (`Single`)  
Blends the canyon into the surrounding terrain, controlling how much the outer landform influences the final shape.

**Depth** (`Single`)  
The depth of the canyon.

**Structual Warp** (`Single`)  
Applies large-scale warping to the canyon structure to introduce bends, meanders, and macro variation.

**Seed** (`Int32`)  
The randomization pattern or seed for the node's process.

**Detail Warp** (`Single`)  
Applies finer warping to add small-scale distortion and surface complexity without changing the overall canyon layout.

**Alternate Style** (`Boolean`)  
Introduces a secondary variation of the selected style to break uniformity and add visual diversity.
