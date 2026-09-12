Source: https://docs.gaea.app/reference/nodes/simulate/icefloe.html

# IceFloe

Simulate › Snow

Shortcode `if`

Icefloe creates flat plates of ice inside a water body.

## Official Video

For a video breakdown of this node, see [the official IceFloe video](https://docs.gaea.app/videos/official/nodes/yt-icefloe.html).

Icefloe creates flat plates of ice inside a water body.

## When to Use It

Use IceFloe when a water body needs broken plates of ice rather than open water or a continuous frozen surface.

## Visual Direction

## Properties

**Size** (`Single`)  
Controls the overall scale of the ice floes. Higher values produce larger ice sheets or floes across the terrain.

**Density** (`Single`)  
Adjusts how many ice floes are distributed across the surface. Higher values increase the number of floes per area.

**Layers** (`Single`)  
Sets the number of distinct ice layers, allowing for multi-layered ice formations and increased visual complexity.

**Thickness** (`Single`)  
Specifies the vertical thickness of the ice floes, determining how prominent or deep the ice appears.

**Shaper 1** (`Single`)  
Applies the first shaping function or modifier to the ice floes, affecting their outline and internal structure.

**Shaper 2** (`Single`)  
Applies a secondary shaping function or modifier, allowing for additional variation and detail in the ice floes.

**Reverse** (`Boolean`)  
Inverts the effect of the primary shaping function, altering the distribution or orientation of the ice floes.

**Reverse 2** (`Boolean`)  
Inverts the effect of the secondary shaping function, providing further control over the appearance of the ice floes.

**Seed** (`Int32`)  
The randomization pattern or seed for the node's process. Changing the seed produces different ice floe arrangements.
