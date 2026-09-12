Source: https://docs.gaea.app/reference/nodes/derive/texturebase.html

# TextureBase

Derive › Texture

Shortcode `tb`

Texture uses multiple factors (such as chaos, slopes, flow zones, and other geological features) to create a texture mask for any color node to create a texture map. Can be used by itself, or combined with other data maps and selectors.

Creates a mask simulating natural material distribution onto which color from [SatMap](https://docs.gaea.app/reference/nodes/colorize/satmap.html) or [CLUTer](https://docs.gaea.app/reference/nodes/colorize/cluter.html) or other color node is "flowed". Useful for driving texturing, shading, or erosion masks with a natural, layered look.

{% include "/.data/includes/color-lookup.md" %}

## When to Use It

Use TextureBase when you want a broad natural-distribution mask that gives SatMap, CLUT, or other color nodes a believable base to follow.

## Properties

**Slope** (`Single`)  
Controls the influence of terrain slope on the texture, allowing highlights or patterns to follow steeper areas.

**Flows** (`Single`)  
Adds flow-based features to the texture, simulating the effect of water or material movement across the surface.

**Soil** (`Single`)  
Introduces soil-like patterns or noise, adding organic variation to the texture.

**Patches** (`Single`)  
Controls the presence and size of patchy areas within the texture, creating irregular or clustered features.

**Chaos** (`Single`)  
Adjusts the randomness and disorder in the texture, increasing natural variation and reducing uniformity.

**Peaks** (`Single`)  
Emphasizes or introduces features that follow terrain peaks, adding detail to high areas in the texture.

**Accentuate** (`Enum`)  
Enhances specific features or contrasts in the texture, making details more pronounced.

- **Off** — No accentuation is applied; the texture remains unmodified.
- **Old** — Applies the legacy accentuation method, increasing contrast and detail using the previous algorithm.
- **New** — Uses the latest accentuation method for improved feature enhancement and clarity.

**Enhance** (`Enum`)  
Applies additional enhancement to the texture's tonal range or contrast.

- **None** — No enhancement is applied.
- **Autolevel** — Automatically adjusts the levels to maximize the full tonal range, improving contrast.
- **Equalize** — Equalizes the histogram, distributing intensity values more evenly for a balanced look.

**Reverse** (`Boolean`)  
Inverts the black and white values of the texture, swapping dark and light areas.

**Seed** (`Int32`)  
Randomization seed for generating the texture. Changing the seed produces different pattern variations.
