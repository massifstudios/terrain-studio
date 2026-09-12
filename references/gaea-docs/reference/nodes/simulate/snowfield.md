Source: https://docs.gaea.app/reference/nodes/simulate/snowfield.html

# Snowfield

Simulate › Snow

Shortcode `sf`

Creates broad settled snow coverage across the terrain surface.

Snow

## When to Use It

Use Snowfield when you need broad settled snow coverage across the terrain surface.

## Visual Direction

## Properties

**Cascades** (`Int32`)  
The number of cascades, or periods of snowfall.

**Duration** (`Single`)  
Controls the length of time snow accumulates on the terrain, affecting overall snow coverage and depth.

**Intensity** (`Single`)  
Determines the rate at which snow falls and accumulates, influencing the thickness and density of the snow layer.

**Taper Snowfall** (`Boolean`)  
Sequentially reduce the amount of snowfall per cascade.

**Taper Amount** (`Single`)  
The amount to reduce in each snowfall period.

**Settle Duration** (`Single`)  
Specifies the time it takes for freshly fallen snow to settle and compact, impacting the smoothness and stability of the snow surface.

**Melt** (`Single`)  
Snow melts evenly across the entire terrain, resulting in a consistent reduction of snow coverage.

**Snow Line** (`Single`)  
Defines the altitude or threshold below which snow does not accumulate, simulating natural snow lines on mountains.

**Slip off Angle** (`Single`)  
Specifies the minimum slope angle at which snow will slide off, affecting snow accumulation on steep surfaces.

**Adhered Snow Mass** (`Single`)  
Controls the amount of snow that remains attached to surfaces, even on steep or vertical terrain.

**Flows** (`Boolean`)  
Create flow structures like furrows and grooves from sharp snow accumulation.

**Flows Depth** (`Single`)  
The depth of the flow structures.

**Seed** (`Int32`)  
Sets the randomization seed for snow distribution, ensuring reproducible results or introducing variation in snow patterns.

**Sharp Buildup** (`Boolean`)  
Forces sharp edges where large amounts of snow gets piled up.

**Alternate Snowfall** (`Boolean`)  
Use an alternate algorithm to create snow cascades.

**Surface Details** (`Single`)  
Add small surface details to accentuate snow buildup.

**Direction** (`Enum`)  
The general direction of snowfall.

- **X** — None
- **N** — North
- **E** — East
- **S** — South
- **W** — West
