Source: https://docs.gaea.app/reference/nodes/utility/edge.html

# Edge

Utility › Structure

Shortcode `ed`

Zero Borders softly transitions the edges of a terrain into nothing, similar to a vignette effect in paint applications. Useful when using Bomber or other effects where a tall edge would be problematic.

## Official Video

For a video breakdown of this node, see [the official Edge video](https://docs.gaea.app/videos/official/nodes/yt-edge.html).

Edge was known as "Zero Borders" in Gaea 1.

## When to Use It

Use Edge when you need to fade a terrain or mask out toward the borders so downstream nodes do not produce hard edge artifacts.

## Properties

**Style** (`Enum`)  
Specifies the shape and profile of the edge effect applied to the terrain or mask.

- **Round** — Creates a smooth, rounded edge transition for a softer boundary.
- **Square** — Produces a straight, angular edge for a more defined and abrupt boundary.
- **Precise** — Applies an exact edge in pixels, preserving sharpness and detail at the boundary.

**Size** (`Single`)  
Controls the width or thickness of the edge effect. Higher values create a broader edge region.

**Pixels** (`Int32`)  
Sets the edge width in pixel units, allowing for precise control over the edge's extent.

**Softness** (`Single`)  
Adjusts the smoothness of the edge transition. Higher values result in a more gradual, blended edge.
