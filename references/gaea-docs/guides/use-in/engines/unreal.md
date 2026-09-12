Source: https://docs.gaea.app/guides/use-in/engines/unreal.html

# Unreal

The easiest way to import a Gaea terrain into Unreal Engine is to use [Gaea2Unreal](https://docs.gaea.app/guides/use-in/bridges/gaea2unreal/index.html). Start with [Installing Gaea2Unreal](https://docs.gaea.app/guides/use-in/bridges/gaea2unreal/installing-gaea2unreal.html) if the plugin is not installed yet.

If you wish to import it by yourself, use the [Unreal](https://docs.gaea.app/reference/nodes/output/unreal.html) output node to prepare the output to fit your terrain size as required by Unreal Engine. Then you can manually import the terrain.

![](https://docs.gaea.app/.data/assets/new-import-settings.png)

1. Enter **Landscape** mode.
2. Click **Import from File**.
3. Click the button next to **Heightmap File** and select your heightmap.
4. Calculate the Z scale for your heightmap using the formula (Max height of your heightmap in meters) x (100 to convert to centimeters) x 0.001953125.
5. Click **Import**.

If the Z-scale calculation feels too complex or you are not getting accurate results, use [Gaea2Unreal](https://docs.gaea.app/guides/use-in/bridges/gaea2unreal/index.html) to import your terrain. Gaea will prepare these calculations and other factors for you.

You can follow the exact instructions on Unreal's official documentation:

<https://dev.epicgames.com/documentation/en-us/unreal-engine/importing-and-exporting-landscape-heightmaps-in-unreal-engine>

---

## Unreal Node

![The Unreal Node can accept multiple inputs and save them for Unreal Engine consumption.](https://docs.gaea.app/.data/assets/Gaea_-_Untitled_10-13-41-AM.png)

The Unreal node is made to prepare your data to be imported into Unreal Engine very easily using the Gaea2Unreal bridge plugin. The Unreal node will resample the data to a Unreal friendly resolution (for example, a 2048 x 2048 build will become 2017 x 2017 to accommodate UE's considerations).

### Adding Inputs

You can add multiple inputs to the Unreal node.

![You can add additional Input Ports by dropping a connection in the middle of the Unreal node and selecting "Add new port"](https://docs.gaea.app/.data/assets/Gaea_-_Untitled_10-15-28-AM.png)

![You can also click the pink "Add Port" button in the Node Properties.](https://docs.gaea.app/.data/assets/Gaea_-_Untitled_10-15-53-AM.png)

### Target Size and Format

The node's Target Size property lets you select the resample size. It is recommended that you select your Gaea build resolution to be the closest to the size you need. Based on your current Build Resolution set in the [Build Options](https://docs.gaea.app/ui/interface/build-options/index.html), you will be given a recommended Target Size.

You can set the file format for all the exported files.

RAW is currently not fully supported. Both PNG and R16 are fully supported right now, but we recommend using PNG for maximum compatibility.

![](https://docs.gaea.app/.data/assets/image-(33).png)

### Unreal Friendly Naming

By default, the filename for each input is decided by the name of the node. You can rename a node to rename the output filename.

To use the Gaea2Unreal bridge, you should enable Unreal Friendly Naming. This will prefix `T_` in front of masks and textures and `H_` in front of the heightfield.

The heightfield should always be the primary input.

## Building

When you build your terrain, the Unreal node will save all the appropriate asset files along with a JSON definition file. This is the key file you will need for [Importing Terrains](https://docs.gaea.app/guides/use-in/bridges/gaea2unreal/importing-terrains.html). See [Build and Export](https://docs.gaea.app/using/using-gaea/build-and-export/index.html) for the general build workflow.

See the next section on how to set up your exported terrain in Unreal Engine.
