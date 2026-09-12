Source: https://docs.gaea.app/reference/nodes/simulate/trees.html

# Trees

Simulate › Vegetation

Shortcode `tr`

The Arboreal Node gives you the power of creating superficial forests and vegetation. These forests are part of the heightfield while being "detached" enough for various export scenarios.

Trees

## Official Video

For a video breakdown of this node, see [the official Trees video](https://docs.gaea.app/videos/official/nodes/yt-trees.html).

## When to Use It

Use Trees when you want forest or vegetation massing to appear as part of the heightfield for distant coverage, canopy breakup, or export-friendly vegetation forms.

## Visual Direction

## Properties

**Output** (`Enum`)  
Determines the output type for tree placement. 'Instances' provides individual tree positions for rendering and export. 'AreaMask' is an experimental feature that outputs a mask of tree coverage, useful for blending or exclusion but may be less reliable.

- **Instances** — Outputs individual tree instance data for use in rendering, simulation, or export. Recommended for stable and reliable results.
- **Area Mask** — Outputs a mask indicating areas where trees are placed. Useful for visualization and blending, but currently experimental and may produce less predictable results.

**Count in Thousands** (`Int32`)  
Sets the number of trees to be placed, specified in thousands. Higher values increase density and mask quality but may impact performance and memory usage.

**Is Relative** (`Boolean`)  
If enabled, tree count scales with terrain resolution for consistent density. Recommended for predictable results at higher resolutions, but may increase resource usage.

**LOD Info** (`String`)  
Provides Level of Detail (LOD) information for tree instances, enabling optimized rendering and export at different distances or resolutions.

**Tree Size** (`Single`)  
Controls the height of each tree instance. Affects the appearance of exported trees but not the mask. Useful for adjusting the final look of tree outputs.

**Trim Under** (`Single`)  
Removes tree instances below a certain size threshold, clamping down on smaller or weaker trees to refine the mask and output.

**Shape** (`Enum`)  
Specifies the distribution pattern or type of tree shapes. 'Point' is a single pixel, 'Big' is a larger square, and 'BigOnly' restricts placement to large clusters. 'Big' is preferred for mask export, while 'Point' is better for visualization.

- **Point** — Places trees as single small pixels, resulting in sparse and detailed distribution. Best for internal visualization.
- **Big** — Places larger tree shapes, creating denser and more legible masks. Preferred for exporting tree masks.
- **Big Only** — Restricts placement to only large tree clusters, excluding smaller or individual trees for a bold mask.

**Seed** (`Int32`)  
Sets the randomization seed for procedural tree placement. Changing the seed produces different tree layouts and simulation results.

**Seek Water** (`Enum`)  
Controls whether trees grow near water flows ('Seek') or away from them ('Avoid'). Useful for simulating species preferences and biomes.

- **Seek** — Trees preferentially grow near water sources and flow areas, simulating water-loving species.
- **Avoid** — Trees avoid water sources and flow areas, simulating species that prefer drier conditions.

**Avoid Trees** (`Enum`)  
Masks out areas occupied by other tree nodes, useful for stacking multiple tree species or biomes without overlap.

- **None** — No avoidance; trees can be placed close together or overlap with other tree nodes.
- **Previous** — New trees avoid areas occupied by the previously placed tree node, reducing overlap in stacked nodes.
- **All** — New trees avoid all other tree nodes in the stack, maximizing spacing and separation.

**Health** (`Single`)  
Controls the duration and vigor of the tree simulation. Higher health increases density and allows trees to grow into smaller valleys and more challenging terrain. Lower health restricts growth to main flow areas.

**Patches** (`Single`)  
Clumps trees together into larger groups, creating stylized patches instead of individual trees. Useful for natural-looking forests.

**Spread** (`Single`)  
Blurs and expands the area where trees can grow, merging small flows into larger masses and creating more natural, contiguous forest areas.

**Random** (`Single`)  
Adds random noise to tree placement, scattering trees across the terrain without regard to inhibition or aspect controls. Rarely used for realistic masks.

**Slope** (`Float2`)  
Restricts tree growth on steep terrain. Not recommended when using a custom inhibition mask, as the mask provides more accurate control.

**Slope Falloff** (`Single`)  
Controls how quickly tree density decreases as slope increases. Use with slope inhibition for gradual or sharp transitions.

**Altitude** (`Float2`)  
Sets altitude limits for tree placement, simulating elevation-based growth restrictions. Works well with both aspect inhibition and custom masks.

**Altitude Falloff** (`Single`)  
Controls the rate at which tree density decreases with altitude, enabling smooth or sharp transitions in elevation-based inhibition.

**Peaks** (`Single`)  
Removes tree growth from exposed mountain peaks, preventing trees from growing in the most exposed areas.

**Strict** (`Boolean`)  
Enforces strict adherence to inhibition rules, resulting in sharper falloff and less natural tree distribution. Use for precise, non-organic masks.

**Dead Flow** (`Single`)  
Represents areas where tree growth is suppressed due to snow mass or adverse conditions. Controls the amount of snow mass moving down the terrain.

**Consolidate Flows** (`Single`)  
Groups snow flows into larger masses, affecting how far snow moves down the terrain and influencing tree inhibition.

**Bias** (`Single`)  
Directs snow flows to move less or more, affecting the inhibition of tree growth in snowy areas.

**Snowline** (`Single`)  
Defines the altitude at which snow flows start, restricting tree growth above this line.

**Chaos** (`Single`)  
Warps tree positions for a more natural, randomized look, breaking up straight flows and adding organic variation.

**Blur Edges** (`Single`)  
Softens and tapers the edges of tree patches, blending them into the terrain for a more natural transition.

**Visualize Data** (`Boolean`)  
Enables visualization of tree placement, flows, and snow inhibition, assisting with mask tuning and debugging.

**Custom Influence** (`Single`)  
Applies custom influence maps or masks to control tree placement according to user specifications, enabling highly tailored results.

**Trim** (`Single`)  
Removes trees from specified areas, allowing for manual or procedural clearing of tree patches for refined mask output.
