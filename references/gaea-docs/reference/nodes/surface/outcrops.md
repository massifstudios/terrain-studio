Source: https://docs.gaea.app/reference/nodes/surface/outcrops.html

# Outcrops

Surface › Rocky

Shortcode `oc`

The Outcrops LookDev node creates rocky outcrops on any surface. You can control coverage, breakage, and surface details. Several Outcrops nodes can be layered for complex results.

## When to Use It

Use Outcrops when you need lookDev node creates rocky outcrops on any surface.

## Visual Direction

## Properties

**Variations** (`Int32`)  
Controls the number of unique outcrop shapes generated before repetition. Higher values create more visual diversity among outcrops.

**Strata** (`Single`)  
Adds visible geological layering or banding to the outcrops, simulating sedimentary or stratified rock formations. High values increase the distance between Strata layers.

**Density** (`Single`)  
Adjusts how many outcrops are placed across the terrain. Higher values increase the number of outcrops per area.

**Shape** (`Single`)  
Determines the overall form and silhouette of each outcrop, affecting their appearance and how they blend with the terrain.

**Chipped** (`Boolean`)  
Controls the amount of chipping or breakage on the outcrops, simulating natural weathering and erosion effects.

**Seed** (`Int32`)  
Sets the randomization seed for outcrop generation, ensuring reproducible or varied results with each value.

**Size** (`Float2`)  
Specifies the base size of each outcrop, affecting their footprint and prominence on the terrain.

**Height** (`Float2`)  
Controls the vertical scale of the outcrops, determining how tall or flat they appear.

**Rotation** (`Float2`)  
Applies random or specified rotation to each outcrop, increasing natural variation and preventing repetitive patterns.
