Source: https://docs.gaea.app/reference/nodes/modify/flip.html

# Flip

Modify › Adjust

Shortcode `fp`

Flip flips the terrain horizontally, vertically, or both.

The Flip node flips the terrain horizontally, vertically, or both. It can also invert the X-Y ordering of the individual pixels.

## When to Use It

Use Flip when a terrain, mask, or pattern is correct in shape but needs its orientation mirrored.

## Visual Direction

![](https://docs.gaea.app/.data/assets/Flip_VD.png)

## Properties

**Type** (`Enum`)  
Selects which axis the input is mirrored across.

- **X** — Flips the input horizontally (mirrors along the X axis).
- **Y** — Flips the input vertically (mirrors along the Y axis).
- **XY** — Flips both axes (180° mirror of the input).
