Source: https://docs.gaea.app/reference/nodes/derive/texturizer.html

# Texturizer

Derive › Texture

Shortcode `tz`

Generates terrain texture masks using preset styles and controllable secondary effects.

Works like **TextureBase** but uses pre-defined, curated profiles to quickly generate natural-looking texture masks. It's designed for fast, consistent results with less manual tuning. **Factor** is the main strength or intensity control for the chosen style, while **Secondary** adjusts the impact of supporting details and effects.

## When to Use It

Use Texturizer when you want ready-made terrain texture masks with controllable breakup instead of building every selector from scratch.

## Properties

**Style** (`Enum`)  
Select from A, B, C, D, E, F, G, H, I, J, K, or L. Each option represents a distinct pre-defined texture style, allowing you to experiment and choose the look that best fits your terrain.

- **A**
- **B**
- **C**
- **D**
- **E**
- **F**
- **G**
- **H**
- **I**
- **J**
- **K**
- **L**

**Factor** (`Single`)  
Primary driver controlling the overall strength or intensity of the selected style. Higher values increase the prominence of the texture effect.

**Secondary** (`Single`)  
Adjusts the strength of secondary effects such as soil accumulation, flow streaks, and breakup patterns. Use to fine-tune the complexity and realism of the texture.

**Seed** (`Int32`)  
Randomizes the pattern while keeping other settings the same, allowing for variation without the need to re-tune parameters.

**Flows** (`Single`)  
Simulates natural erosion channels and directional streaks, adding realism by mimicking the effects of water or wind movement across the terrain.

**Slope** (`Single`)  
Adds slope-based weighting to the texture, favoring either steeper or flatter areas to enhance natural variation.

**Soil** (`Single`)  
Controls soil-like accumulation in flatter regions, adding natural buildup and enhancing the realism of the terrain surface.
