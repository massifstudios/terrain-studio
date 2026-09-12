Source: https://docs.gaea.app/reference/nodes/modify/whorl.html

# Whorl

Modify › Effect

Shortcode `wh`

Whorl applies multiple vortex-like distortions across the terrain to create large-scale warping. Very useful for creating massive landscapes without a patterning in the noise and for creating slightly surreal or alien landscapes.

Whorl applies multiple vortex-like distortions across the terrain to create large scale warping. This can be very useful when creating massive landscapes and wanting to avoid a patterning in the noise, or for creating slightly surreal or alien landscapes.

Whorl can be overpowering if used by itself. An effective way to tone down Whorl is to lower the Influence value in [Modifier Stack](https://docs.gaea.app/ui/interface/property-editor/modifier-stack.html) or use one of the blend modes.

## When to Use It

Use Whorl when you want multiple swirling distortions spread across the terrain instead of one central vortex.

## Visual Direction

![](https://docs.gaea.app/.data/assets/Whorl_VD.png)

## Properties

**Type** (`Enum`)

- **Stretch** — Whorls appear as stretched, interconnected formations.
- **Spin** — Whorls appear as round, independent formations.

**Whorls** (`Int32`)  
The number of whorls to add to the terrain.

**Power** (`Single`)  
The strength of the distortion applied with each whorl.

**Seed** (`Int32`)  
The randomization pattern or seed for the node's process.
