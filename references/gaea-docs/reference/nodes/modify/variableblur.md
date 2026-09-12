Source: https://docs.gaea.app/reference/nodes/modify/variableblur.html

# VariableBlur

Modify › Blur

Shortcode `vb`

Blurs the input by varying the blur amount across the surface using local data or a guide.

## When to Use It

Use VariableBlur when the terrain shape is mostly working and you need a targeted reshape, filter, or remap pass.

## Visual Direction

![](https://docs.gaea.app/.data/assets/VariableBlur_VD.png)

## Properties

**Radius** (`Single`)  
Controls the blur radius for each pixel. The radius can vary across the image, enabling localized or mask-driven blur effects.
