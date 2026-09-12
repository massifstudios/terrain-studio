Source: https://docs.gaea.app/reference/nodes/modify/threshold.html

# Threshold

Modify › Adjust

Shortcode `th`

Abs can modify the terrain or mask it by eliminating values higher or lower than the exact value specified.

The Threshold node lets you alter the terrain based on the height limit specified. It can especially useful when converting a soft mask to a hard mask.

## When to Use It

Use Threshold when you need to isolate values above or below a cutoff for masking or hard value cleanup.

## Visual Direction

![](https://docs.gaea.app/.data/assets/Threshold_VD.png)

## Properties

**Level** (`Single`)  
Specifies the threshold value used to separate terrain features. Values above this level are retained, while values below are discarded or set to a minimum. Adjusting the level controls the cutoff point for terrain masking or feature isolation.
