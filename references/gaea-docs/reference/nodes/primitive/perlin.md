Source: https://docs.gaea.app/reference/nodes/primitive/perlin.html

# Perlin

Primitive › Basic

Shortcode `p`

Perlin is the most basic and essential noise. In Gaea, we've made a "geo-variant" by modifying the base Perlin shape to fit the needs of terrain design from the beginning.

Node must be baked when executing Hybrid or Tiled builds.

![](https://docs.gaea.app/.data/ref/Perlin/Perlin.webp)

## When to Use It

Use Perlin when you need a dependable all-purpose base noise for blocking out terrain, testing erosion, or blending with other primitives.

## Visual Direction

![](https://docs.gaea.app/.data/assets/Perlin_VD.png)

## Properties

**Type** (`Enum`)  
Specifies the style of Perlin noise to generate, affecting the overall appearance and structure of the noise pattern.

- **FBM** — Fractal Brownian Motion. Produces classic Perlin noise with layered octaves for natural, smooth terrain.
- **Ridged** — Generates ridged noise, emphasizing sharp crests and valleys for more dramatic, mountainous features.
- **Billowy** — Creates billowy, cloud-like noise with soft, rounded shapes and less pronounced valleys.

**Scale** (`Single`)  
The scale of the noise. Higher values create larger, less detailed shapes. Lower values create noisier, highly detailed shapes.

**Octaves** (`Int32`)  
The number of noise octaves. Higher values create more complexity by layering additional detail at smaller scales.

**Gain** (`Single`)  
Controls the amplitude reduction between octaves, affecting the persistence and contrast of the noise.

**Height** (`Single`)  
Sets the maximum elevation or intensity of the noise, scaling the output vertically.

**Seed** (`Int32`)  
The randomization pattern or seed for the node's process. Changing the seed produces different noise patterns.

**Type** (`Enum`)  
Specifies the style of Perlin noise to generate, affecting the overall appearance and structure of the noise pattern.

- **None**
- **Simple**
- **Complex**

**Frequency** (`Single`)  
Controls the frequency of the warp, determining how often the pattern is distorted across the terrain.

**Amplitude** (`Single`)  
Sets the strength of the warp effect, controlling how far the noise is displaced.

**Octaves** (`Int32`)  
The number of noise octaves. Higher values create more complexity by layering additional detail at smaller scales.

**Scale X** (`Single`)  
The non-uniform scale of the X axis, stretching or compressing the noise horizontally.

**Scale Y** (`Single`)  
The non-uniform scale of the Y axis, stretching or compressing the noise vertically.

**X** (`Single`)  
Offsets the Perlin noise along the X axis, shifting its position horizontally.

**Y** (`Single`)  
Offsets the Perlin noise along the Y axis, shifting its position vertically.
