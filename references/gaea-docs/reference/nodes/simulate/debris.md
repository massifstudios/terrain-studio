Source: https://docs.gaea.app/reference/nodes/simulate/debris.html

# Debris

Simulate › Scatter

Shortcode `db`

Simulates loose debris across the terrain surface to create broken rocky accumulation.

Debris

## When to Use It

Use Debris when you need simulates loose debris across the terrain surface to create broken rocky accumulation.

## Visual Direction

## Properties

**Debris Amount** (`Int32`)  
Controls how many individual rocks or debris pieces are emitted across the terrain.

**Amount Multiplier** (`Single`)  
Scales the emitted debris amount globally without changing other parameters, allowing quick adjustment of overall density.

**Scale** (`Single`)  
Controls the overall scale of debris elements, affecting their size relative to the terrain.

**Friction** (`Single`)  
Determines how easily debris slides; higher values make debris stop sooner and form steeper piles.

**Restitution** (`Single`)  
Controls energy loss on impact, affecting how far debris bounces or continues sliding after being emitted.

**Size** (`Float2`)  
Sets the overall size of the emitted rocks or debris pieces.

**Height** (`Single`)  
Defines the vertical offset or emission height for debris release.

**Seed** (`Int32`)  
Randomization seed for generating varied debris distributions.

**Shape** (`Enum`)  
Determines the geometric style of debris elements, affecting their appearance.

- **Rounded** — Produces smoother, rounded rock fragments.
- **Sharp** — Produces jagged, angular rock fragments.

**Render Still Rocks** (`Boolean`)  
Enables rendering of non-moving debris that has settled on the terrain.

**Distribution** (`Enum`)  
Specifies how debris is distributed across the emission area.

- **Uniform** — Evenly spreads debris across the emission area.
- **Natural** — Creates more organic, irregular clustering of debris.

**Export Point Cloud** (`Boolean`)  
Enables the export of debris positions as a point cloud for further processing or use in external applications.

**Information** (`Byte`)
