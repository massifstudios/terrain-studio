Source: https://docs.gaea.app/reference/nodes/modify/adjust.html

# Adjust

Modify › Adjust

Shortcode `fx`

The Fx node makes that process easier by combining all the common adjustments inside a single node and allowing you to toggle them on and off.

The Adjust node is pretty much the same as the Modifier Stack, but in node form. It is often a process of trial and error where am adjustment node would need to be replaced with another. Sometimes the node would need to be chained with another.

This node makes that process easier by combining all the common adjustments inside a single node and allowing you to toggle them on and off. Additionally, it has its own [Modifier Stack](https://docs.gaea.app/ui/interface/property-editor/modifier-stack.html) so you can apply further adjustments.

## When to Use It

Use Adjust when you want modifier-stack style height remapping in the graph without committing to a baked upstream result.

## Properties

**Multiply** (`Single`)  
Multiplies each value by a specified factor.

**Shaper** (`Single`)  
Applies a shaping function to modify the curve of values.

**Clamp** (`Float2`)  
Restricts values to a specified minimum and maximum range.

**Clip** (`Float2`)  
Limits values by cutting off those outside a given range.

**Drop** (`Boolean`)  
Removes values below or above a certain threshold.

**Autolevel** (`Boolean`)  
Automatically adjusts values to span the full output range.

**Equalize** (`Boolean`)  
Redistributes values to achieve a more uniform distribution.

**Strong** (`Boolean`)  
Enhances the contrast or intensity of values.

**Invert** (`Boolean`)  
Flips values to their opposite within the range.
