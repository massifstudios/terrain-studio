Source: https://docs.gaea.app/reference/nodes/modify/graphiceq.html

# GraphicEQ

Modify › Profile

Shortcode `gq`

Reshapes terrain using multiple adjustable bands, similar to an equalizer for height profiles.

## Official Video

For a video breakdown of this node, see [the official GraphicEQ video](https://docs.gaea.app/videos/official/nodes/yt-graphiceq.html).

Applies a multi-band equalizer-style filter to shape terrain features across different scales.

- **Band 1-7**  
  Seven adjustable frequency bands. Each band controls the gain for a specific range of detail scales:
  - Lower bands affect large-scale, broad features.
  - Higher bands target fine details.
  - Positive values boost features at that scale; negative values suppress them.

Use the bands together to fine-tune the terrain's overall frequency profile and achieve precise control over its shape and detail distribution.

## When to Use It

Use GraphicEQ when you want to emphasize or suppress terrain features at specific scales instead of applying one broad filter.

## Properties

**Band 1** (`Single`)

**Band 2** (`Single`)

**Band 3** (`Single`)

**Band 4** (`Single`)

**Band 5** (`Single`)

**Band 6** (`Single`)

**Band 7** (`Single`)
