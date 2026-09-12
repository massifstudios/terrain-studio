Source: https://docs.gaea.app/reference/nodes/simulate/dusting.html

# Dusting

Simulate › Snow

Shortcode `ds`

Applies a thin dusting of snow or sediment over the terrain surface.

## Official Video

For a video breakdown of this node, see [the official Dusting video](https://docs.gaea.app/videos/official/nodes/yt-dusting.html).

Dusting is a lightweight snow node which adds a thin layer of snow to your scene with a focus on creating light sprinklings of snow.

This node can be used with or without Snow.

## When to Use It

Use Dusting when you need a light snow pass that settles gently without turning the whole terrain into a deep winter result.

## Visual Direction

## Properties

**Strength** (`Single`)  
Controls the intensity of the snow dusting effect, determining how visible and thick the light snow layer appears.

**Snowline** (`Single`)  
Sets the elevation at which snow begins to appear, defining the lower boundary for snow coverage.

**Falloff** (`Single`)  
Controls the softness of the transition near the snowline, blending the snow edge smoothly into the terrain.

**Directional** (`Boolean`)  
Enables or adjusts wind-driven accumulation, causing snow to gather more on one side of terrain features.

**Direction** (`Single`)  
Sets the primary direction for wind-driven snow accumulation, simulating the effect of prevailing winds.

**Peaks** (`Single`)  
Controls additional snow accumulation on terrain peaks, enhancing coverage on high points.

**Coverage** (`Single`)  
Adjusts the overall amount of snow applied to the terrain, increasing or decreasing the extent of dusting.

**Slope Coverage** (`Float2`)  
Modifies how much snow accumulates on slopes, allowing for more or less coverage on inclined surfaces.

**Flow** (`Single`)  
Simulates subtle downhill drift and redistribution of snow, mimicking natural snow movement due to gravity.

**Melt** (`Single`)  
Reduces snow coverage in sun-exposed or warmer areas, simulating natural melting patterns.

**Gritty** (`Boolean`)  
Adds noise and texture variation to the snow, creating a more natural, patchy, and irregular appearance.

**Include Existing Snow** (`Boolean`)  
Allows the dusting effect to respect and blend with existing snow layers, ensuring seamless integration.

**Seed** (`Int32`)  
Randomizes the snow pattern, enabling varied results or reproducibility by changing the seed value.

**Artistic Control** (`Single`)  
Provides fine-tuning options for the overall look and style of the snow dusting, supporting creative or stylized results.
