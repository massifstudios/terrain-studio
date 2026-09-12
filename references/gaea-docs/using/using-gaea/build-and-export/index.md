Source: https://docs.gaea.app/using/using-gaea/build-and-export/

# Build and Export

Build and Export is the final handoff stage of a Gaea project. This is where you decide what data leaves the graph, at what resolution, in which format, and how it should be organized for the target application or pipeline.

Gaea can export heightfields, masks, color maps, splat maps, meshes, point clouds, tiled worlds, and application-specific outputs. The right setup depends on what the receiving tool expects and how much precision the terrain needs.

## Main Export Paths

[Exporting Nodes](https://docs.gaea.app/using/using-gaea/build-and-export/exporting-nodes.html) explains how to mark nodes for export, choose output ports, name files, and use dedicated output nodes such as [Export](https://docs.gaea.app/reference/nodes/output/export.html), [Mesher](https://docs.gaea.app/reference/nodes/output/mesher.html), [Unreal](https://docs.gaea.app/reference/nodes/output/unreal.html), and [Unity](https://docs.gaea.app/reference/nodes/output/unity.html).

[Mesh Exports](https://docs.gaea.app/using/using-gaea/build-and-export/mesh-exports.html) covers mesh output through the [Mesher](https://docs.gaea.app/reference/nodes/output/mesher.html) node, including grid meshes, optimized triangle meshes, scale choices, texture coordinates, and normals.

[Tiled Builds](https://docs.gaea.app/using/using-gaea/build-and-export/tiled-builds.html) explains how to build large worlds as tiles or hybrid builds, including when to bake tile-unfriendly graph sections.

[Using Regions](https://docs.gaea.app/using/using-gaea/build-and-export/regions/index.html) shows how to build a specific part of a terrain at higher resolution without rebuilding the entire world.

For the interface that controls resolution, build scope, tiles, regions, profiles, output nodes, and scripts, see [Build Options](https://docs.gaea.app/ui/interface/build-options/index.html).

## Before You Build

- Confirm the final terrain size and height range in [Terrain](https://docs.gaea.app/ui/interface/build-options/terrain.html).
- Check that every required output is marked for export or connected to an output node.
- Choose formats based on the type of data: heightfields need more precision than many masks or preview textures.
- Treat masks and packed maps as data so downstream tools do not apply unwanted color correction.
- Bake stable or tile-unfriendly sections before a tiled or hybrid build when needed.
- Run a small test build before committing to a large final output.

## Target Applications

For guidance specific to software such as [Blender](https://docs.gaea.app/guides/use-in/apps/blender.html) or [Unreal](https://docs.gaea.app/guides/use-in/engines/unreal.html), see [Apps](https://docs.gaea.app/guides/use-in/apps/index.html).

For automation scenarios where output paths, variables, or repeated builds are controlled outside the UI, see [Command Line Automation](https://docs.gaea.app/developers/automation/cli/command-line-automation.html) and [Managing Input and Output](https://docs.gaea.app/developers/automation/building-bridges/managing-input-and-output.html).
