Source: https://docs.gaea.app/using/using-gaea/colorizing-and-textures/

# Colorizing and Textures

Colorizing in Gaea is built around masks. A terrain's height, slope, curvature, flow, soil, and other derived data can drive gradients, SatMaps, layered materials, and exported masks for downstream tools.

![](https://docs.gaea.app/.data/assets/datamaps.webp)

The goal is not only to make the viewport look finished. Good color and texture graphs also produce useful production data: albedo or diffuse maps, splat masks, erosion masks, vegetation masks, wetness masks, and other control maps that can be used in engines and DCC applications.

## Core Ideas

[Crafting Masks](https://docs.gaea.app/using/using-gaea/colorizing-and-textures/crafting-masks.html) explains how grayscale masks and color lookup tables shape most texture workflows. A mask can distribute a gradient by elevation, isolate steep slopes, follow erosion flow, or target specific terrain features.

[Working with SatMaps](https://docs.gaea.app/using/using-gaea/colorizing-and-textures/working-with-satmaps.html) covers Gaea's satellite-derived color maps. SatMaps are useful for fast natural color variation, especially when combined with masks or refined through other color nodes.

[Synthesizing Colors](https://docs.gaea.app/using/using-gaea/colorizing-and-textures/synthesizing-colors.html) shows how [Synth](https://docs.gaea.app/reference/nodes/colorize/synth.html) can convert an existing color source into an editable gradient. This is helpful when you want to extract a palette from a map and then art-direct it inside Gaea.

[Layering Textures](https://docs.gaea.app/using/using-gaea/colorizing-and-textures/layering-textures.html) focuses on the [Mixer](https://docs.gaea.app/reference/nodes/utility/mixer.html) node, which lets you stack multiple color layers and control them with terrain masks.

[ColorErosion](https://docs.gaea.app/using/using-gaea/colorizing-and-textures/colorerosion.html) applies sediment-style logic to color maps so color can flow, collect, and weather in a way that follows the terrain.

## Useful Data Sources

- Aspect maps such as [Height](https://docs.gaea.app/reference/nodes/derive/height.html), [Slope](https://docs.gaea.app/reference/nodes/derive/slope.html), [Angle](https://docs.gaea.app/reference/nodes/derive/angle.html), [Curvature](https://docs.gaea.app/reference/nodes/derive/curvature.html), and [Peaks](https://docs.gaea.app/reference/nodes/derive/peaks.html) help isolate structural terrain features.
- Generative maps such as [FlowMap](https://docs.gaea.app/reference/nodes/derive/flowmap.html), [Soil](https://docs.gaea.app/reference/nodes/derive/soil.html), [RockMap](https://docs.gaea.app/reference/nodes/derive/rockmap.html), [Occlusion](https://docs.gaea.app/reference/nodes/derive/occlusion.html), [TextureBase](https://docs.gaea.app/reference/nodes/derive/texturebase.html), and [Texturizer](https://docs.gaea.app/reference/nodes/derive/texturizer.html) create more natural variation than simple height or slope masks alone.
- Simulation outputs from erosion, snow, water, debris, or vegetation can become highly specific masks for color and material work.

## Practical Workflow

1. Start with large color zones, usually driven by height, slope, or a SatMap.
2. Add feature masks from erosion, soil, flow, rock, or occlusion data.
3. Layer secondary color variation with [Mixer](https://docs.gaea.app/reference/nodes/utility/mixer.html) or additional CLUT-style nodes.
4. Keep important masks available for export if they will drive materials, scattering, or landscape layers in another application.
5. Check exported maps as data, not just as viewport color, especially for masks and splat maps.
