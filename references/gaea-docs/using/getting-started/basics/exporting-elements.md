Source: https://docs.gaea.app/using/getting-started/basics/exporting-elements.html

# Exporting Elements

![](https://docs.gaea.app/.data/assets/beyond-3.webp)

Gaea is primarily an asset generator designed for creating and exporting terrains to be used in other applications. It supports all major heightfield and mesh formats, ensuring compatibility with various platforms and rendering tools.

## Key Export Elements

Below is a list of common elements you may wish to export, and the nodes you can use to get those elements.

![](https://docs.gaea.app/.data/assets/beyond-2.webp)

### The Terrain

![](/.data/assets/r2.webp) ![](/.data/assets/sophia-2\_-\_ACDSee\_Photo\_Studio\_Home\_2024\_06-03-23-PM.webp)

This can either be a 32-bit displacement map or a high precision mesh. It is the most essential part of your project. Game engines would use 16-bit displacement maps.

### Color Map

![](/.data/assets/r4.webp)

If you have constructed a color map that will be the Diffuse/Albedo map for your terrain, then this will be exported as either an 8-bit or 16-bit bitmap.

### Masks

![](/.data/assets/Data\_Flow.webp) ![](/.data/assets/Data\_RockMap.webp)

Various aspects of the terrains such as the slope (Slope Map) or flow of sediments (Flow Map) can be exported as bitmaps to control map blending, masking, and driving scatter instances. These are usually exported as 8-bit bitmaps.

### Point Clouds

![](https://docs.gaea.app/.data/assets/growth-simulation.webp)

For data usage such as Tree or Rock scattering, or similar functions, you can also export masks as XYZ point clouds.

### Normals and Splat

![](/.data/assets/r6.webp)

Gaea has dedicated nodes for Normal Map and Splat Map creation which are exported as 8-bit bitmaps. The Normal Map node can extract Detail Normals as well.

### Ambient Occlusion and Rock Occlusion

![](/.data/assets/Data\_Occlusion.webp) ![](/.data/assets/r11.webp)

Gaea can export both Ambient Occlusion (AO node) and "rock occlusion" (Occlusion node). Rock Occlusion with specific masking or using the Shaper adjustment can often be used for Reflectance in PBR scenarios.

### Rendered / Baked Lighting

![](/.data/assets/r1.webp)

The Light node lets you use high-precision rendering to create a fully realized render that you can use for direct application on your object.

In this example below, the mountain uses a baked lightmap on an unlit shader in Blender to dramatically reduce rendering time.

![](https://docs.gaea.app/.data/assets/blender.webp)

See [Build and Export](https://docs.gaea.app/using/using-gaea/build-and-export/index.html) for more specifics.
