Source: https://docs.gaea.app/reference/nodes/simulate/lichtenberg.html

# Lichtenberg

Simulate › Erosion

Shortcode `lct`

Creates branching, lightning-like erosion or fracture patterns.

## When to Use It

Use Lichtenberg when you need branching, lightning-like erosion or fracture patterns.

## Visual Direction

## Properties

**Style** (`Enum`)  
Selects the style for the cracks, determining the overall appearance and pattern of the Lichtenberg figures.

- **A** — Style A: Produces a classic branching crack pattern, suitable for general terrain fracturing.
- **B** — Style B: Generates a denser, more intricate crack network for detailed surface effects.
- **C** — Style C: Creates a sparse, wide-spread crack pattern, ideal for subtle terrain disruption.

**Scale** (`Single`)  
Controls the scale of the larger cracks, affecting how prominent and wide the main crack structures appear.

**Complexity** (`Int32`)  
Adjusts the level of detail in the crack network. Higher values produce more branches and finer crack structures.

**Depth** (`Single`)  
Sets the depth of the cracks, determining how deeply the cracks cut into the terrain surface.

**Expand** (`Single`)  
Expands the cracks outward, increasing their coverage and making the crack network more pronounced.

**Bias** (`Single`)  
Provides artistic control over the shape and direction of the cracks, allowing for customized crack patterns.

**Seed** (`Int32`)  
Random seed for generating the crack pattern. Changing the seed produces different crack arrangements while keeping other settings the same.
