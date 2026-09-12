Source: https://docs.gaea.app/reference/nodes/surface/stratify.html

# Stratify

Surface › Terrace

Shortcode `ct`

Stratify creates broken strata or rock layers on the terrain in a non-linear fashion, producing a robust, realistic formation.

Stratify allows you to create broken strata or rock layers on the terrain in a non-linear fashion. Unlike terracing, stratification involves substrata/subterraces created in confined local zones, such as between two broken plates. Each stratified layer exists independent from the rest of the layer, creating a robust, realistic formation.

## When to Use It

Use Stratify when the terrain needs broken rock layering or sedimentary bands that feel uneven and geologic.

## Visual Direction

## Properties

**Spacing** (`Single`)  
Controls the distance between individual stratified layers, affecting the frequency and thickness of visible strata in the terrain.

**Octaves** (`Int32`)  
Sets the number of noise octaves used to add variation to the stratified layers. Higher values introduce more complexity and detail.

**Intensity** (`Single`)  
Adjusts the overall strength of the stratification effect, making the layers more or less pronounced.

**Shape** (`Single`)  
Modifies the profile or contour of the stratified layers, influencing how sharply or smoothly the layers transition.

**Seed** (`Int32`)  
Defines the randomization seed for procedural generation, ensuring unique but repeatable stratification patterns.

**Tilt Amount** (`Single`)  
Applies an angular tilt to the stratified layers, simulating geological tilting or shifting of strata.

**Direction** (`Int32`)  
Sets the orientation of the stratified layers, determining the primary direction in which the strata are aligned.
