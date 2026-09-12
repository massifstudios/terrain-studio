Source: https://docs.gaea.app/reference/nodes/modify/meshify.html

# Meshify

Modify › Shape

Shortcode `ms`

Converts terrain into a faceted, mesh-like surface with controllable vertex density and detail sensitivity.

## When to Use It

Use Meshify when the terrain shape is mostly working and you need a targeted reshape, filter, or remap pass.

## Visual Direction

![](https://docs.gaea.app/.data/assets/Meshify_VD.png)

## Properties

**Vertices** (`Single`)  
Sets the number of vertices used to construct the mesh facets. Higher values produce finer, more detailed meshes; lower values create larger, more pronounced facets.

**Seed** (`Int32`)  
Randomization seed for mesh facet arrangement. Changing the seed produces different polygonal patterns and facet layouts.

**Detail Sensitivity** (`Single`)  
Controls how closely the mesh facets follow the underlying terrain detail. Higher sensitivity preserves more terrain features; lower sensitivity results in smoother, more abstract faceting.
