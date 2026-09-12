Source: https://docs.gaea.app/reference/nodes/modify/dilate.html

# Dilate

Modify › Transform

Shortcode `di`

Dilate the pixels of the heightfield or mask.

## When to Use It

Use Dilate when the terrain shape is mostly working and you need a targeted reshape, filter, or remap pass.

## Visual Direction

![](https://docs.gaea.app/.data/assets/Dilate_VD.png)

## Properties

**Size** (`Single`)  
Sets the size of the dilation effect. Larger values expand features more, increasing the reach of the dilation.

**Shape** (`Single`)  
Specifies the geometric shape used for the dilation operation, affecting how features are expanded.

**Iterations** (`Int32`)  
Controls how many times the dilation process is repeated. More iterations result in greater expansion of features.

**Kernel** (`Enum`)  
Selects the kernel type used for dilation, determining the pattern and direction of expansion.

- **Cross** — Uses a cross-shaped kernel, expanding features in horizontal and vertical directions.
- **Rectangle** — Uses a rectangular kernel, expanding features uniformly in all directions.
- **Line** — Uses a line-shaped kernel, expanding features along a specified direction.
- **Line Vertical** — Uses a vertical line kernel, expanding features only in the vertical direction.

**Invert** (`Boolean`)  
Inverts the input before applying dilation, causing the operation to expand background areas instead of foreground features.
