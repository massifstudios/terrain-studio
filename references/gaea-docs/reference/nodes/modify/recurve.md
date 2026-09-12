Source: https://docs.gaea.app/reference/nodes/modify/recurve.html

# Recurve

Modify › Profile

Shortcode `rc`

Recurve is a curvature-based expander that inflates and/or deflates the terrain to create stronger slopes and larger, softer formations from a given noise or terrain.

Recurve is a curvature-based expander node. It can inflate and/or deflate the terrain to create stronger slopes and larger, softer formations from a given noise or terrain.

Despite appearances, Recurve's output is very sensitive to subtle changes in the source heightfield. For example, if prior to Recurve you apply MicroErosion or Breaker, then Recurve will create a very specific flow pattern on those grooves and valleys.

## When to Use It

Use Recurve when you need curvature-based expansion or contraction that reshapes existing forms without replacing them.

## Visual Direction

![](https://docs.gaea.app/.data/assets/Recurve.png)

## Properties

**Power** (`Single`)  
The strength of the inflate/deflate operation. Higher values cause drastic shifts in shape formation.

**Scale** (`Single`)  
The amount of details to preserve. Higher values cause more prominent shapes to form.

**Iterations** (`Int32`)

**Style** (`Enum`)

- **Inward**
- **Outward**
