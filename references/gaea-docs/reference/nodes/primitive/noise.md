Source: https://docs.gaea.app/reference/nodes/primitive/noise.html

# Noise

Primitive › Basic

Shortcode `kn`

Noise applies controlled single-pixel noise onto the terrain.

Node must be baked when executing Hybrid or Tiled builds.

Noise applies controlled single-pixel noise onto the terrain.

## When to Use It

Use Noise when you need a controlled speckle or grain layer to disturb an otherwise clean terrain or mask.

## Visual Direction

![](https://docs.gaea.app/.data/assets/Noise_VD.png)

## Properties

**Noise Type** (`Enum`)  
Selects the algorithm used to generate noise. Each type produces a distinct pattern and character, suitable for different terrain or texture effects.

- **Random** — Generates pure random noise, resulting in a highly chaotic and unstructured pattern.
- **Perlin** — Produces smooth, gradient-based noise with natural transitions, ideal for organic terrain features.
- **Gaussian** — Applies a Gaussian distribution to the noise, creating patterns with a bell-curve intensity and softer transitions.
- **Fixed** — Generates a noise pattern based on large, finite shapes.
- **Micro** — Creates very fine, high-frequency noise suitable for adding subtle surface detail or micro-variation.

**Height** (`Single`)  
Controls the maximum amplitude or elevation of the noise, affecting how pronounced the noise appears in the output.

**Density** (`Single`)  
Adjusts the frequency or scale of the noise pattern. Higher density results in more frequent variations across the terrain.

**Relative Density** (`Boolean`)  
When enabled, density is scaled relative to the current resolution or context, maintaining consistent detail at different scales.

**Blend Mode** (`Enum`)  
Determines how the generated noise is combined with the input or base layer, affecting the final appearance.

- **Add** — Adds the noise values to the input, increasing elevation or intensity where noise is present.
- **Multiply** — Multiplies the input by the noise values, modulating the base layer and enhancing contrast.
- **Subtract** — Subtracts the noise values from the input, reducing elevation or intensity in noisy areas.

**Seed** (`Int32`)  
Sets the randomization seed for the noise generation. Changing the seed produces a different noise pattern while keeping other settings the same.
