Source: https://docs.gaea.app/reference/nodes/output/ao.html

# AO

Output › Render

Shortcode `ao`

Creates an ambient occlusion map that darkens recessed areas and enhances terrain depth.

## Official Video

For a video breakdown of this node, see [the official AO video](https://docs.gaea.app/videos/official/nodes/yt-ao.html).

## When to Use It

Use AO when you need an ambient occlusion map for previewing depth or driving downstream texturing and shading.

## Properties

**Quality** (`Enum`)  
Sets the processing resolution for the ambient occlusion calculation. Higher quality provides more accurate and detailed results, but may increase computation time.

- **Low** — Fastest processing with minimal detail. Suitable for previews or low-fidelity outputs.
- **Medium** — Balanced processing speed and detail. Recommended for general use.
- **High** — High detail with slower processing. Use when fine ambient occlusion is required.
- **Ultra** — Maximum detail and accuracy. Best for final renders, but slowest to compute.

**Enhance** (`Single`)  
Boosts the strength and clarity of the ambient occlusion effect, making shadows and depth more pronounced in the terrain.
