Source: https://docs.gaea.app/ui/graph/basic-workflow/underlays.html

# Using Underlays

By default Gaea will use the nearest "Heightfield"/Terrain node to display the structure beneath the color or mask. However, if your graph splits or follows a secondary path to the color node, then it may use an incorrect node to display the terrain. To prevent this, all you have to do is go to the correct heightfield node, right-click and select "Use as Underlay".

![The "top" branch is used by default by Gaea to show the terrain under a color map or mask.](https://docs.gaea.app/.data/assets/underlay1.webp)

To use a node as the underlay, right-click the node and select `Use as Underlay`. You can also select the node and press `G`.

![Now the RadialGradient, or bottom branch, is used as the underlay after being marked.](https://docs.gaea.app/.data/assets/underlay2.webp)

The Underlay node can be accessed from the toolbar in the Data Editor. It is shown as a purple link.

To see a flat preview, click `V` to temporarily toggle a flat preview in the viewport.

## Ignoring Nodes for Underlays

When an Underlay is not explicitly specified, Gaea nodes such as [ColorErosion](https://docs.gaea.app/reference/nodes/colorize/colorerosion.html) will try to automatically pick up the appropriate ancestor node as an underlay. However, in some situations you may get an undesired underlay.

Let's take this example. We use a [Shaper](https://docs.gaea.app/reference/nodes/modify/shaper.html) node to modify our input for the [TextureBase](https://docs.gaea.app/reference/nodes/derive/texturebase.html) node. The child nodes then use that Shaper as the underlay and not the Erosion node.

![](https://docs.gaea.app/.data/assets/underlay-skip-01.png)

To prevent this, we right-click the Shaper node and set "Exclude from Underlay". Gaea will now ignore it and go to the parent node instead.

![](https://docs.gaea.app/.data/assets/underlay-skip-02.png)

This is very useful when switching between different portions of a complex graph where local underlays may be different.

## **Beware of downstream underlays**

Be mindful when you modify nodes that are before the Underlay. If they are part of the chain that leads up to the Underlay node, modifying them will cause all nodes in between to update as well, much like normal Lock Preview. This can make Gaea slow when working in 2K or higher preview resolution.

If Gaea detects such an Underlay, it will show a warning in the [Property Editor](https://docs.gaea.app/ui/interface/property-editor/index.html).
