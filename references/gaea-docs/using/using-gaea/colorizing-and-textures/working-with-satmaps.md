Source: https://docs.gaea.app/using/using-gaea/colorizing-and-textures/working-with-satmaps.html

# Working with SatMaps

Our library of over 1400 color maps, derived from real satellite data, helps you colorize your terrains quickly without sacrificing realism.

![](https://docs.gaea.app/.data/assets/SatMap.png)

This tech was originally pioneered by QuadSpinner for GeoGlyph in 2014, Gaea's latest iteration provides an extensive library covering all natural locations, giving you a vast playground for colorizing your terrains.

![](https://docs.gaea.app/.data/assets/Gaea_-_Untitled_03-22-12-AM.webp) ![](https://docs.gaea.app/.data/assets/Gaea_-_Untitled_03-22-15-AM.webp)

This TextureMap node is attached to a SatMap node. The SatMap is adapted to the mask as described in [Crafting Masks](https://docs.gaea.app/using/using-gaea/colorizing-and-textures/crafting-masks.html).

## Editing SatMaps

There a few ways to quickly tweak the SatMaps to better fit your needs.

### Map Bias

By moving the Bias slider, you can have the SatMap apply more towards the left (negative values) or right (positive values) of the selected Color Map.

|  |  |  |
| --- | --- | --- |
|  |  |  |

### Map Clipping

You can use the Clip slider to tell SatMaps to only use the selected segment of the entire color map.

![](https://docs.gaea.app/.data/assets/Gaea_-_Untitled_03-23-01-AM.webp)

![](https://docs.gaea.app/.data/assets/satmap_clipping.png)

Here, we clip out most of the left half of the color map, and you can see how that changes the influence of the color map on the terrain.

#### Roughness

The Roughness setting scatters the pixels of the color map to add the desired level of chaos and distortion.

![](https://docs.gaea.app/.data/assets/Gaea_-_Untitled_03-35-36-AM.webp) ![](https://docs.gaea.app/.data/assets/Gaea_-_Untitled_03-35-39-AM.webp) ![](https://docs.gaea.app/.data/assets/Gaea_-_Untitled_03-35-41-AM.webp) ![](https://docs.gaea.app/.data/assets/Gaea_-_Untitled_03-35-44-AM.webp) ![](https://docs.gaea.app/.data/assets/Gaea_-_Untitled_03-35-47-AM.webp)

#### HSL

The final tweaks come from Hue/Saturation/Luminosity editing.

---

SatMaps are one of the crucial aspects of colorizing your terrains. You can elevate your SatMaps by combining them with a Mixer as explained in [Layering Textures](https://docs.gaea.app/using/using-gaea/colorizing-and-textures/layering-textures.html).

And finally, add [ColorErosion](https://docs.gaea.app/reference/nodes/colorize/colorerosion.html) to add a new level of realism to your texture maps.
