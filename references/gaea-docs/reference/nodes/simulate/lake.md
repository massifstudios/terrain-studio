Source: https://docs.gaea.app/reference/nodes/simulate/lake.html

# Lake

Simulate › Water

Shortcode

Lake simulates rainfall on the terrain and creates physically accurate lakes.

Water, Shore

## When to Use It

Use Lake when you need simulates rainfall on the terrain and creates physically accurate lakes.

## Visual Direction

## Properties

**Type** (`Enum`)

- **Simple**
- **Advanced**

**X** (`Single`)  
X position of the lake source.

**Y** (`Single`)  
Y position of the lake source.

**Water Level** (`Single`)  
Level of the water.

**Render Surface** (`Boolean`)  
Whether to render the surface of the water or just create masks.

**Accuracy** (`Enum`)  
The accuracy of the water placement and water containment within depressions.

**Precipitation** (`Single`)  
Controls the amount of water available for lake formation, influencing the size and number of lakes generated.

**Small Lakes** (`Single`)  
Enables or adjusts the creation of smaller lakes, increasing the frequency of minor water bodies across the terrain.

**Flood Control** (`Boolean`)  
Limits excessive flooding by managing water distribution, preventing lakes from overflowing or merging unnaturally.

**Water Floor** (`Single`)  
Sets the elevation of the lake bed, determining the minimum depth and base level of the water body.

**Shore Size** (`Single`)  
Controls the width and extent of the shoreline around lakes, affecting how far the shore transitions into the surrounding terrain.

**Altitude Bias** (`Single`)  
Biases lake placement towards certain altitudes, favoring higher or lower terrain for lake formation.

**Size Bias** (`Single`)  
Adjusts the likelihood of generating larger or smaller lakes, influencing the overall distribution of lake sizes.
