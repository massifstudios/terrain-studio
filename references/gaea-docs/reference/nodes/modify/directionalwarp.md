Source: https://docs.gaea.app/reference/nodes/modify/directionalwarp.html

# DirectionalWarp

Modify › Warp

Shortcode `dw`

DWarp or Directional Warp uses a secondary input to warp the first input in a uniform direction.

The **DirectionalWarp** node deforms the terrain using a guide input to control the displacement. This allows for controlled warping effects such as wind-swept erosion, river carving, and other directional deformations.

## When to Use It

Use DirectionalWarp when you want controlled deformation pushed along a guide direction rather than a general organic warp.

## Inputs

- **Primary Input:** The terrain to be warped.
- **Guide Input:** The directional guide that influences the warping effect. This can be any terrain or pattern used to determine the displacement direction.

## Visual Direction

![](https://docs.gaea.app/.data/assets/DirectionalWarp_VD.png)

## Properties

**Strength** (`Single`)

**Direction** (`Single`)

**Edge Behaviour** (`Enum`)

- **Edge**
- **Mirror**
