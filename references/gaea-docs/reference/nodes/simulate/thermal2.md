Source: https://docs.gaea.app/reference/nodes/simulate/thermal2.html

# Thermal2

Simulate › Erosion

Shortcode `t2`

Thermal2 is an evolution of the previous node. It creates Thermal Erosion to create realistic talus and debris.

## Official Video

For a video breakdown of this node, see [the official Thermal2 video](https://docs.gaea.app/videos/official/nodes/yt-thermal2.html).

## When to Use It

Use Thermal2 when you want talus, debris, and slope breakdown that feel driven by thermal weathering rather than hydraulic flow.

## Properties

**Duration** (`Single`)  
Controls the duration of the thermal erosion process. Higher values extend the erosion period, resulting in more significant changes to the terrain.

**Strength** (`Single`)  
Determines the intensity of thermal erosion. Increasing this value produces more pronounced smoothing and displacement of materials, especially on steeper slopes.

**Anisotropy** (`Single`)  
Governs the directional bias of erosion and rock deposits. Low values preserve the original terrain with minimal smoothing, while high values intensify erosion, especially on sharp peaks, leading to stronger talus formation and more heavily eroded terrain. Useful for simulating environments with directional erosion influences such as wind or gravity.

**Angle** (`Single`)  
Sets the maximum slope angle before materials begin to move downward, forming talus. Lower angles create more gradual slopes, while higher angles allow steeper terrain before material shifts.

**Sediment Removal** (`Single`)  
Controls the amount of sediment removed during talus formation. Higher values produce cleaner, sharper features by removing excess material, while lower values allow more sediment to accumulate, resulting in smoother transitions.

**Feature Scale** (`Single`)  
Adjusts the scale of thermal erosion features in meters. Larger values create broader, more pronounced features, while smaller values result in finer details.
