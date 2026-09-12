Source: https://docs.gaea.app/reference/nodes/modify/slopeblur.html

# SlopeBlur

Modify › Blur

Shortcode `sb`

SlopeBlur is a versatile, low-level node that adds directional blurring based on the slopes of a Guide terrain or mask. It is one of our deceptively simple nodes that can create a wide array of shapes.

## When to Use It

Use SlopeBlur when you want to directionally soften, smear, or break up an existing terrain using the slope information from a guide terrain or mask.

## Visual Direction

![](https://docs.gaea.app/.data/assets/SlopeBlur_VD.png)

## Properties

**Intensity** (`Single`)  
Controls the strength of the slope blur effect. Higher values result in more pronounced blurring along the slope direction.

**Iterations** (`Int32`)  
Specifies how many times the slope blur operation is applied. More iterations increase the smoothness and diffusion of the blur.

**Direction** (`Single`)  
Sets the primary direction in which the blur is applied. Adjusting this value changes the orientation of the blur effect across the terrain.

**Asymmetry** (`Single`)  
Controls the asymmetry of the blur, allowing the effect to be stronger in one direction than the other for more natural or stylized results.

**Normalized** (`Boolean`)  
Lorem ipsum

**Window** (`Enum`)  
Lorem ipsum

- **Bell**
- **Box** — Uses a box-shaped window for uniform blurring, resulting in sharper transitions at the edges.

**Quality** (`Enum`)  
Settings related to the quality and precision of the slope blur operation. Higher quality may increase processing time but results in smoother, more accurate blurring.

- **Low**
- **Medium**
- **High**
- **Ultra**

**Antialiasing** (`Enum`)  
Applies antialiasing to the slope blur effect, reducing jagged edges and improving visual smoothness.

- **Off** — No antialiasing is applied. May result in sharper but potentially jagged edges.
- **X 4** — Applies 4x antialiasing for moderate smoothing of edges.
- **X 16** — Applies 16x antialiasing for maximum edge smoothing and highest visual quality.
