Source: https://docs.gaea.app/reference/nodes/modify/deflate.html

# Deflate

Modify › Profile

Shortcode `df`

Compresses and pulls terrain features inward, reducing bulk and prominence.

## Official Video

For a video breakdown of this node, see [the official Deflate video](https://docs.gaea.app/videos/official/nodes/yt-deflate.html).

Deflate takes away the bulk of the terrain leaving only finer details. The higher the amount, the fewer large details.

This is useful for extracting DetailNormal-friendly shapes, isolating fine/high-frequency shapes for adding on other terrains, and simply reducing the bulk of the terrain.

## When to Use It

Use Deflate when you want to strip away bulky forms and leave a thinner, more detail-driven version of the terrain.

## Visual Direction

![](https://docs.gaea.app/.data/assets/Deflate_VD.png)

## Properties

**Amount** (`Single`)  
Controls the degree to which the terrain is compressed or lowered. Higher values result in a more pronounced flattening or reduction of elevation, effectively 'deflating' the terrain features.
