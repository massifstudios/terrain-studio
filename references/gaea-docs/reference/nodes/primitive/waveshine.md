Source: https://docs.gaea.app/reference/nodes/primitive/waveshine.html

# WaveShine

Primitive › Extended

Shortcode `ws`

Generates wave-like reflective or rippling patterns useful for stylized surfaces and water-like forms.

Node must be baked when executing Hybrid or Tiled builds.

## When to Use It

Use WaveShine when you need wave-like reflective or rippling patterns useful for stylized surfaces and water-like forms.

## Visual Direction

![](https://docs.gaea.app/.data/assets/WaveShine_VD.png)

## Properties

**Frame** (`Int32`)  
Specifies the current animation frame or time step for the wave simulation, allowing for frame-by-frame control or preview.

**Octaves** (`Int32`)  
Sets the number of detail layers (octaves) used in the wave pattern, increasing complexity and realism with higher values.

**Seed** (`Int32`)  
Randomization seed for generating unique wave patterns. Changing the seed produces different wave arrangements.

**Animation Speed** (`Single`)  
Controls the speed at which ripples animate across the surface, affecting how quickly the pattern evolves over time.

**Size** (`Single`)  
Sets the scale of individual ripples, with larger values producing broader, more widely spaced ripples.

**Amplitude** (`Single`)  
Adjusts the height or intensity of the ripples, making them more or less pronounced.

**Direction** (`Single`)  
Specifies the primary direction in which ripples travel, simulating wind or current-driven movement.

**Speed** (`Single`)  
Controls the movement speed of the ripples, determining how fast they propagate across the surface.

**Intensity** (`Single`)  
Sets the strength of the wind patch effect, increasing the impact of wind on the wave or ripple pattern.

**Animation Speed** (`Single`)  
Controls the speed at which ripples animate across the surface, affecting how quickly the pattern evolves over time.

**Size** (`Single`)  
Sets the scale of individual ripples, with larger values producing broader, more widely spaced ripples.

**Contrast** (`Single`)  
Adjusts the contrast of the wind patch effect, making the difference between calm and windy areas more or less pronounced.

**Brightness** (`Single`)  
Controls the overall brightness of the rendered wave surface, affecting how light or dark the result appears.

**Sun Azimuth** (`Single`)  
Sets the horizontal angle of the sun, determining the direction of sunlight and resulting highlights or reflections.

**Sun Elevation** (`Single`)  
Sets the vertical angle of the sun above the horizon, affecting the length and intensity of highlights and shadows on the waves.
