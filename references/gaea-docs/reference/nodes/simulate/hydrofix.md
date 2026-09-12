Source: https://docs.gaea.app/reference/nodes/simulate/hydrofix.html

# HydroFix

Simulate › Erosion

Shortcode `hf`

HydroFix alters the terrain to create continuous drainage network friendly slopes. It can be used before Erosion or other flow related nodes to get better results.

HydroFix is a low level utility that can help create unbroken flows. It does not change the landscape completely, but rather creates small adjustments along flow zones to promote long and better looking flows.

![](https://docs.gaea.app/.data/ref/hydrofix-before.webp)
![](https://docs.gaea.app/.data/ref/hydrofix-after.webp)

In this example, you can see a simple Perlin with a Flow map. The first image shows the default Perlin, while the second shows the Perlin noise with HydroFix applied. You can see the flow map produces longer and more coherent structures.

You do not need to use HydroFix with Rivers as it has a special version of it built into its algorithm.

## When to Use It

Use HydroFix when you need a low level utility that can help create unbroken flows.

## Properties

**Downcutting** (`Single`)  
The amount of downcutting to apply while modifying the terrain for hydrological flow.
