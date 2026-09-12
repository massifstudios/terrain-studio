Source: https://docs.gaea.app/reference/nodes/primitive/dotnoise.html

# DotNoise

Primitive › Extended

Shortcode `do`

Generates dotted or speckled noise patterns for texturing and breakup.

Node must be baked when executing Hybrid or Tiled builds.

## When to Use It

Use DotNoise when you need dotted or speckled noise patterns for texturing and breakup.

## Visual Direction

![](https://docs.gaea.app/.data/assets/DotNoise_VD.png)

## Properties

**Style** (`Enum`)  
Specifies the shape of the random elements placed on the surface.

- **Dot** — Generates round blobs or spheres, creating soft, circular noise patterns.
- **Square** — Generates square-shaped blobs, resulting in blocky, grid-like noise patterns.

**Iterations** (`Int32`)  
Sets the number of times dots or blobs are placed on the surface. Higher values increase the density and coverage of the noise.

**Multiplier** (`Single`)  
Scales the intensity or strength of the dot noise effect, amplifying the contribution of each blob.

**Size** (`Float2`)  
Controls the average size of each dot, blob, or sphere. Larger values produce bigger features.

**Height** (`Single`)  
Sets the maximum elevation or intensity of the dots or blobs above the base surface.

**Blend Mode** (`Enum`)  
Determines how the dot noise is combined with the underlying surface or input.

- **None** — No blending is applied; the dot noise is used as-is.
- **Blend** — Blends the dot noise with the input using standard alpha or linear blending.
- **Add** — Adds the dot noise values to the input, increasing elevation or intensity where dots are present.
- **Subtract** — Subtracts the dot noise from the input, creating depressions or pits where dots are placed.
- **Difference** — Applies a difference operation, emphasizing areas where the dot noise and input differ.
- **Multiply** — Multiplies the dot noise with the input, modulating the surface based on dot presence.
- **Screen** — Uses a screen blend, lightening the surface where dots are present.
- **Max** — Takes the maximum value between the dot noise and the input at each point.
- **Min** — Takes the minimum value between the dot noise and the input at each point.

**Seed** (`Int32`)  
Randomization seed for dot placement and variation. Changing the seed produces different dot patterns.
