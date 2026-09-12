Source: https://docs.gaea.app/reference/nodes/simulate/erosion2.html

# Erosion2

Simulate › Erosion

Shortcode `e2`

An advanced hydraulic erosion node that creates more detailed and realistic erosion results.

## Official Video

For a video breakdown of this node, see [the official Erosion2 video](https://docs.gaea.app/videos/official/nodes/yt-erosion2.html).

The **Erosion\_2** node in Gaea 2 is a powerful new erosion algorithm designed to enhance terrain shaping capabilities beyond the traditional [Erosion](https://docs.gaea.app/reference/nodes/simulate/erosion.html).

Despite its complexity, Erosion\_2 remains user-friendly and delivers deterministic results with up to 10x faster performance, even on the CPU.

It simulates advanced hydraulic erosion with realistic sediment transport and terrain reshaping. It offers fine control over downcutting, sediment deposition, and orographic (rainfall-driven) effects for highly detailed or stylized terrains.

![](https://docs.gaea.app/.data/assets/erosion.webp)

## When to Use It

Use Erosion2 when you want a more advanced hydraulic erosion pass with strong control over downcutting, sediment behavior, and rainfall-driven shaping.

## Properties

**Duration** (`Single`)  
Controls how long erosion runs. Longer durations produce more elaborate terrain reshaping, while shorter durations help preserve original forms.

**Downcutting** (`Single`)  
Governs how deeply water cuts into the terrain, carving valleys and channels.

**Erosion Scale** (`Single`)  
Sets the overall size of erosion features, from fine gullies to massive ravines.

**Seed** (`Int32`)  
Randomizes the erosion pattern while keeping other settings the same.

**No Erosion** (`Boolean`)

**Suspended Load** (`Single`)  
Fine, light sediments that travel far and fill gullies with subtle detail.

**Discharge Angle** (`Single`)  
Sets the slope at which suspended load settles.

**Bed Load** (`Single`)  
Heavier sediments like gravel that fill and shape gullies and slopes.

**Discharge Angle** (`Single`)  
Sets the slope at which suspended load settles.

**Coarse Sediments** (`Single`)  
The heaviest materials (boulders, scree) that deposit on steep slopes.

**Discharge Angle** (`Single`)  
Sets the slope at which suspended load settles.

**Deposition Boost** (`Single`)

**Extra Deposition Boost** (`Single`)

**Shape** (`Single`)  
Enables artistic control over how erosion reshapes terrain.

**Shape Sharpness** (`Single`)  
Defines how sharp or concave ridges become after erosion.

**Shape Detail Scale** (`Single`)  
Sets the scale of simulated details, from broad ravines to fine gullies.

**Enable** (`Boolean`)  
Toggles orographic influence on or off.

**Directional Precipitation** (`Boolean`)  
Simulates rain falling more intensely from one direction to create rain shadows.

**Direction** (`Int32`)  
Sets the direction from which rainfall comes most strongly.

**Rain Shadow** (`Single`)  
Controls how strongly terrain blocks rainfall from certain directions.

**Slope** (`Float2`)  
Limits rainfall effects to specific elevation ranges.

**Altitude** (`Float2`)  
Limits rainfall effects to specific slope ranges.

**Reverse** (`Boolean`)  
Inverts the influence mask for creative control.
