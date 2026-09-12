Source: https://docs.gaea.app/reference/nodes/modify/curve.html

# Curve

Modify › Profile

Shortcode `cu`

Curve adjusts the height curve of a terrain.

## When to Use It

Use Curve when the terrain shape is mostly working and you need a targeted reshape, filter, or remap pass.

## Properties

**Curve Spline** (`List`1`)  
Defines the shape of the curve or spline used to remap or adjust values. The curve controls how input values are transformed, allowing for custom response profiles or non-linear adjustments.

**Mode** (`Enum`)  
Specifies how the curve is applied to the input values.

- **Absolute** — Applies the curve using absolute input values, mapping the entire range directly according to the curve shape.
- **Relative** — Applies the curve relative to the input's current range or context, allowing for adaptive or proportional remapping.

**Fit Curve** (`Command`)  
Automatically fits or scales the curve to match the input range, ensuring the curve covers the full extent of the data.
