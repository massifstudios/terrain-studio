Source: https://docs.gaea.app/reference/nodes/modify/slopewarp.html

# SlopeWarp

Modify › Warp

Shortcode `sw`

SlopeBlur is a versatile, low-level node that adds directional warping based on the slopes of a Guide terrain or mask. It behaves similarly to SlopeBlur.

## When to Use It

Use SlopeWarp when the terrain shape is mostly working and you need a targeted reshape, filter, or remap pass.

## Visual Direction

![](https://docs.gaea.app/.data/assets/SlopeWarp_VD.png)

## Properties

**Intensity** (`Single`)  
Controls the strength of the slope warp effect. Higher values produce more pronounced warping and distortion of the terrain along the slope.

**Iterations** (`Int32`)  
Specifies how many times the slope warp operation is applied. Increasing iterations can create more complex or exaggerated warping effects.

**Direction** (`Single`)  
Sets the primary direction in which the slope warp is applied. Adjust this to control the orientation of the warping across the terrain.

**Normalized** (`Boolean`)  
When enabled, normalizes the effect so that the intensity is consistent regardless of terrain scale or resolution.

**Quality** (`Enum`)  
Settings related to the quality and precision of the slope warp operation. Higher quality may increase processing time but results in smoother, more accurate warping.

- **Low**
- **Medium**
- **High**
- **Ultra**

**Antialiasing** (`Enum`)  
Applies antialiasing to the slope warp effect, reducing jagged edges and improving visual smoothness.

- **Off** — No antialiasing is applied. May result in sharper but potentially jagged edges.
- **X 4** — Applies 4x antialiasing for moderate smoothing of edges.
- **X 16** — Applies 16x antialiasing for maximum edge smoothing and highest visual quality.
