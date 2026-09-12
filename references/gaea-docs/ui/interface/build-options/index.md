Source: https://docs.gaea.app/ui/interface/build-options/

# Build Options

The **Build Options** window gathers the settings that define how Gaea builds and exports a terrain. Use the tabs on the left to choose build scope, output destinations, tiled build behavior, export nodes, Terrain Definition, regions, profiles, scripts, and execution commands.

![](https://docs.gaea.app/.data/assets/Build-Options-Overview.webp)

| Tab | Purpose |
| --- | --- |
| [Resolution](https://docs.gaea.app/ui/interface/build-options/resolution.html) | Choose the build scope, resolution, subdivision mode, and output layout. |
| [Build](https://docs.gaea.app/ui/interface/build-options/build.html) | Set the build destination, filename behavior, and overwrite handling. |
| [Tiles](https://docs.gaea.app/ui/interface/build-options/tiles.html) | Configure tile naming, organization, overlap, and axis handling. |
| [Nodes](https://docs.gaea.app/ui/interface/build-options/nodes.html) | Select exportable ports and per-node output settings. |
| [Script](https://docs.gaea.app/ui/interface/build-options/script.html) | Run post-build commands automatically after a build finishes. |
| [Terrain](https://docs.gaea.app/ui/interface/build-options/terrain.html) | Define terrain width, height, and real-scale values in the Terrain Definition panel. |
| [Regions](https://docs.gaea.app/ui/interface/build-options/regions.html) | Create focused high-detail regions inside a larger terrain. |
| [Profiles](https://docs.gaea.app/ui/interface/build-options/profiles.html) | Save and recall complete sets of build settings. |
| [Commands](https://docs.gaea.app/ui/interface/build-options/commands.html) | Start the build or copy the generated command line. |

## How the tabs work together

Two tabs usually define the foundation of a build:

- **Resolution** determines how many pixels are used to represent the build.
- **Terrain** defines the physical size of the world through the **Terrain Definition** settings.

Together, those settings determine the terrain's meters-per-pixel value and help keep erosion, scale judgments, and downstream exports grounded in a realistic size.

For background on scale, see [Scale and Resolution](https://docs.gaea.app/using/getting-started/important/scale-and-resolution.html) and [Dimensions and Scale](https://docs.gaea.app/using/advanced-topics/technical-information/dimensions-and-scale.html).
