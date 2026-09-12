Source: https://docs.gaea.app/using/using-gaea/crafting-the-surface/

# Crafting the Surface

Crafting the surface is where a basic heightfield becomes a believable landform. In Gaea, this usually means building a clear primary shape first, then layering erosion, rock formation, surface breakup, and smaller material cues until the terrain has the right geological character.

![](https://docs.gaea.app/.data/assets/CraftingSurface.png)

Most graphs are strongest when they separate the broad form from the surface treatment. Use primitives and terrain nodes to establish the main mass, erosion to introduce natural drainage and sediment behavior, and surface nodes to add rock, strata, roughness, ledges, or localized detail without losing the larger silhouette.

## Building Blocks

[Noises, Primitives, and Landscapes](https://docs.gaea.app/using/using-gaea/crafting-the-surface/noises-primitives-and-landscapes.html) explains the starting points for terrain construction: basic primitives, geological primitives, and larger landscape nodes. These are useful for blocking out mountains, ridges, craters, islands, and other major shapes before adding heavier processing.

[Erosion](https://docs.gaea.app/using/using-gaea/crafting-the-surface/erosion.html) shows how erosion can transform simple forms into natural terrain. A small number of erosion passes can add drainage, deposits, worn ridges, and scale cues that are difficult to create by hand.

[Surface Nodes](https://docs.gaea.app/using/using-gaea/crafting-the-surface/surface-nodes.html) covers the tools that work mostly at the surface level. Nodes such as [Stratify](https://docs.gaea.app/reference/nodes/surface/stratify.html), [Sandstone](https://docs.gaea.app/reference/nodes/surface/sandstone.html), [Outcrops](https://docs.gaea.app/reference/nodes/surface/outcrops.html), [Craggy](https://docs.gaea.app/reference/nodes/surface/craggy.html), and [Stones](https://docs.gaea.app/reference/nodes/surface/stones.html) can add rock structure and local detail while preserving the underlying volume.

[Transpose Shapes](https://docs.gaea.app/using/using-gaea/crafting-the-surface/transpose-shapes.html) is useful when you want to borrow the surface character of one terrain and apply it to another. This can help you keep a designed silhouette while reusing interesting natural detail.

## Suggested Workflow

1. Start with a readable base form using primitives, landscapes, imported data, or hand-built shapes.
2. Add erosion early enough that it can influence the terrain's main identity.
3. Use surface nodes to add material-specific structure such as strata, outcrops, roughness, or small stones.
4. Rebalance height, slope, or masks with modifiers when the shape needs tighter control.
5. Review the terrain at the scale it will be used, since small details that look useful close up may disappear in a distant environment.

## Related Topics

- [Understanding Erosion](https://docs.gaea.app/using/using-gaea/understanding-erosion/index.html) for the main erosion concepts behind natural terrain shaping.
- [Simulations](https://docs.gaea.app/using/using-gaea/simulations/index.html) for snow, water, debris, vegetation, and other process-driven effects.
- [Colorizing and Textures](https://docs.gaea.app/using/using-gaea/colorizing-and-textures/index.html) for turning the finished surface into useful color and mask outputs.
