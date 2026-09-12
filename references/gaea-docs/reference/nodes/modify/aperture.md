Source: https://docs.gaea.app/reference/nodes/modify/aperture.html

# Aperture

Modify › Transform

Shortcode `ap`

Aperture expands or compacts every feature on the terrain, similar to a camera's bokeh.

## When to Use It

Use Aperture when the terrain shape is mostly working and you need a targeted reshape, filter, or remap pass.

## Visual Direction

![](https://docs.gaea.app/.data/assets/Aperture_VD.png)

## Properties

**Information** (`Byte`)  
Displays additional information or metadata about the aperture operation and its current settings.

**Mode** (`Enum`)  
Specifies the morphological operation to apply to the input, affecting how features are expanded or contracted.

- **Dilation** — Expands or grows features in the input by the specified shape and size, filling small holes and connecting nearby elements.
- **Erosion** — Contracts or shrinks features in the input by the specified shape and size, removing small details and separating connected elements.

**Size** (`Single`)  
Sets the size of the structuring element (aperture) used for the operation. Larger values increase the effect's reach.

**Shape** (`Enum`)  
Determines the geometric shape of the structuring element used for dilation or erosion, affecting the style of the transformation.

- **Disk** — Uses a circular (disk-shaped) structuring element for smooth, rounded transformations.
- **Polygon** — Uses a regular polygon as the structuring element, allowing for custom-sided shapes.
- **Asterisk** — Uses a star or asterisk-shaped structuring element, emphasizing cross-like or radiating effects.
- **Line** — Uses a straight line as the structuring element, applying the effect directionally.
- **Corner** — Uses a corner-shaped structuring element, targeting angular or edge features.

**Vertices Count** (`Int32`)  
For polygon shapes, sets the number of vertices (sides) used, controlling the regularity and appearance of the structuring element.

**Corner Angle** (`Int32`)  
Specifies the angle of the corner for the 'Corner' shape, adjusting the sharpness or openness of the structuring element.

**Direction** (`Int32`)  
Sets the orientation or direction for directional shapes (such as Line or Asterisk), controlling the angle of the effect.

**Antialiased** (`Boolean`)  
If enabled, applies antialiasing to the aperture operation, smoothing the edges of the transformed features for a cleaner result.
