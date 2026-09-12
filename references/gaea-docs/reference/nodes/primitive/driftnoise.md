Source: https://docs.gaea.app/reference/nodes/primitive/driftnoise.html

# DriftNoise

Primitive › Extended

Shortcode `dr`

DriftNoise is a unique primitive that can create overlapping "shelves". It is a versatile primitive noise that can be used in various scenarios from cliffs to crack production.

Node must be baked when executing Hybrid or Tiled builds.

## When to Use It

Use DriftNoise when you want a base pattern that naturally produces overlapping shelves, layered breaks, or cliff-like structures. It is a strong choice when the terrain needs more directional or stepped character than a softer all-purpose noise.

## Visual Direction

![](https://docs.gaea.app/.data/assets/DriftNoise_VD.png)

## Properties

**Passes** (`Int32`)

**Chaos** (`Single`)  
Gives uneveness to the overlapping shelves.

**Scale Large** (`Single`)

**Scale Small** (`Single`)

**Turbulence Large** (`Single`)

**Turbulence Small** (`Single`)

**Wind Direction Large** (`Single`)

**Wind Direction Small** (`Single`)

**Seed** (`Int32`)  
The randomization pattern or seed for the node's process.

**Overlap Shapes** (`Boolean`)
