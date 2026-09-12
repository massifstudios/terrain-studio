Source: https://docs.gaea.app/guides/use-in/bridges/gaea2unreal/

# Gaea2Unreal

Gaea2Unreal is the bridge between Gaea and Unreal Engine. It reads the terrain files prepared by Gaea's Unreal workflow, applies the scale and size metadata Gaea exports, and creates an Unreal Landscape through the Gaea Landscape Importer.

Use this bridge when you want Gaea to handle Unreal-friendly terrain sizing, naming, and metadata instead of calculating the landscape import settings manually.

## Recommended Workflow

1. Install Gaea using the standard [Regular Installation](https://docs.gaea.app/install/install-gaea/regular-installation.html) so bridge integration data is registered correctly.
2. Download and install the Gaea2Unreal plugin from Gaea's `Tools > Plugins` menu.
3. Prepare your terrain with the [Unreal](https://docs.gaea.app/guides/use-in/engines/unreal.html) workflow so the heightfield, optional weight maps, and JSON metadata are exported together.
4. Open Unreal Engine, launch the Gaea Landscape Importer, and import the Gaea heightmap.
5. Add weight maps and landscape layer information if your terrain uses material layers.

## In This Section

| Page | Use it for |
| --- | --- |
| [Installing Gaea2Unreal](https://docs.gaea.app/guides/use-in/bridges/gaea2unreal/installing-gaea2unreal.html) | Installing the plugin into Unreal Engine or a project. |
| [Importing Terrains](https://docs.gaea.app/guides/use-in/bridges/gaea2unreal/importing-terrains.html) | Creating an Unreal Landscape from a Gaea heightmap. |
| [Importing Weight Maps](https://docs.gaea.app/guides/use-in/bridges/gaea2unreal/importing-weight-maps.html) | Importing Gaea masks as Unreal Landscape weight maps. |
| [Source Code](https://docs.gaea.app/guides/use-in/bridges/gaea2unreal/source-code.html) | Finding the open source plugin repository and branch guidance. |
| [Troubleshooting](https://docs.gaea.app/guides/use-in/bridges/gaea2unreal/troubleshooting.html) | Fixing common install, visibility, and import issues. |

## Related Pages

- [Unreal](https://docs.gaea.app/guides/use-in/engines/unreal.html) explains the Unreal-specific export workflow inside Gaea.
- [Unreal](https://docs.gaea.app/reference/nodes/output/unreal.html) documents the Unreal output node in the node reference.
- [Build Options](https://docs.gaea.app/ui/interface/build-options/index.html) covers the build settings that affect exported terrain files.
