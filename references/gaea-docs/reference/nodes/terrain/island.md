Source: https://docs.gaea.app/reference/nodes/terrain/island.html

# Island

Terrain › Primitive

Shortcode `is`

Island draws basic shapes from which entire landmasses can be generated.

Node must be baked when executing Hybrid or Tiled builds.

To draw an Island, open the Editor from the Properties panel and draw your basic island shape. It is best to create coarse, less-defined shapes and let the algorithm generate the details for you.

## When to Use It

Use Island when you want to sketch a landmass silhouette first and let the node build an island from that broad shape.

## Visual Direction

![](https://docs.gaea.app/.data/assets/Island_VD.png)

## Properties

**Size** (`Single`)

**Chaos** (`Single`)

**Seed** (`Int32`)  
The randomization pattern or seed for the node's process.
