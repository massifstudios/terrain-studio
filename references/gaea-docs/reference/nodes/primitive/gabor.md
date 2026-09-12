Source: https://docs.gaea.app/reference/nodes/primitive/gabor.html

# Gabor

Primitive › Basic

Shortcode `g`

Gabor noise is one of the lesser used noises, but provides a wonderful pattern-friendly noise that can be used to simulate various aspects of nature.

Node must be baked when executing Hybrid or Tiled builds.

Gabor shapes are dramatically different when compared to the common Perlin and Voronoi nodes. It can be used to create sand-like patterns, modulated hills, and more. Along with Perlin and Voronoi, we consider Gabor to be one of the foundation nodes of Gaea.

## When to Use It

Use Gabor when you need noise is one of the lesser used noises, but provides a wonderful pattern-friendly noise that can be used to simulate various aspects of nature.

## Visual Direction

![](https://docs.gaea.app/.data/assets/Garbor_VD.png)

## Properties

**Size** (`Single`)  
Controls the scale of the Gabor noise features. Larger values produce broader, more widely spaced patterns, while smaller values create finer details.

**Entropy** (`Single`)  
Adjusts the randomness or complexity of the Gabor noise. Higher entropy values result in more chaotic and less predictable patterns.

**Anisotropy** (`Single`)  
Determines the degree of directionality in the noise. Higher values stretch the pattern along a specific axis, creating elongated or striated features.

**Azimuth** (`Single`)  
Sets the orientation angle (in degrees) for the anisotropic direction of the Gabor noise, controlling the alignment of elongated features.

**Gain** (`Single`)  
Controls the amplitude or intensity of the Gabor noise, affecting the contrast and prominence of the generated features.

**Seed** (`Int32`)  
The randomization pattern or seed for the node's process. Changing the seed produces different variations of the Gabor noise.
