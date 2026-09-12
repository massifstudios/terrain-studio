Source: https://docs.gaea.app/reference/nodes/modify/warp.html

# Warp

Modify › Warp

Shortcode `w`

Warp can take the terrain and "warp" the shape creating more believable shapes or alter simple shapes into a more complex formation. It can help breakup masks too.

## When to Use It

Use Warp when the terrain feels too synthetic and needs broader organic distortion.

## Warp

Warp can take the terrain and "warp" the shape creating more organic shapes. In terrains it can help create more believable shapes or alter simple shapes into a more complex formation. In masks, it can help create breakups that can make a linear mask more random and chaotic without losing its general shape.

By using vector fields, the warping created by this node result in more comprehensive shapes without creating directional tearing or over-stretching the original shape.

**Warp as a Post Process**

Warp is also built into the [Modifier Stack](https://docs.gaea.app/ui/interface/property-editor/modifier-stack.html) for easy access.

## Visual Direction

![](https://docs.gaea.app/.data/assets/Warp_VD.png)

## Properties

**Size** (`Single`)  
The size of the warp noise. Controls the scale of the distortion pattern applied to the input.

**Strength** (`Single`)  
The strength of the warp noise. Higher values produce more pronounced distortions.

**Z Scale** (`Single`)  
Vertical axis scale for the noise, affecting the intensity of the warp along the Z (height) axis.

**Warp Source** (`Enum`)  
Selects the noise or pattern used as the source for the warp. Choose from built-in options or provide a custom source.

- **Perlin FBM** — Uses Perlin fractal Brownian motion noise as the warp source, producing smooth, natural distortions.
- **Voronoi R** — Uses Voronoi (R) noise as the warp source, creating cell-like, ridged distortions.
- **Voronoi P** — Uses Voronoi (P) noise as the warp source, suitable for generating mountainous or peak-like distortions.
- **Voronoi a** — Uses Voronoi (A) noise as the warp source, blending characteristics of other Voronoi types for varied effects.
- **Voronoi S** — Uses Voronoi (S) noise as the warp source, emphasizing separated, individual cell boundaries.
- **Voronoi M** — Uses Voronoi (M) noise as the warp source, favoring dominant cells for strong, directional warping.
- **Voronoi D** — Uses Voronoi (D) noise as the warp source, emphasizing ridges and linear features in the distortion.

**Perturbation** (`Single`)  
Applies additional random displacement to the warp, increasing the irregularity and complexity of the effect.

**Complexity** (`Int32`)  
The complexity of the warp noise source. Higher values add more detail and layers to the distortion.

**Roughness** (`Single`)  
The roughness of the warp noise. Higher values (especially above 0.5) create harsher, more jagged distortions.

**Normalized** (`Boolean`)  
Specifies whether the noise is normalized. Normalization ensures the output remains within a standard range.

**Edge Behaviour** (`Enum`)  
Determines how the warp handles the edges of the input, preventing visible seams or artifacts.

- **Edge** — Applies standard edge handling, which may result in wrapping or stretching at the borders.
- **Mirror** — Edge pixels are mirrored, creating seamless, reflected borders for the warp effect.

**Modulation** (`Single`)  
The amount of modulation to use from the Modulation input (if connected), allowing external control over the warp intensity.

**Modulation Direction** (`Single`)  
Sets the direction of the modulation, controlling how the modulation input influences the warp.

**Seed** (`Int32`)  
The randomization pattern or seed for the node's process. Changing the seed produces different warp results.

**Iterations** (`Int32`)  
Number of warp iterations to apply. More iterations increase the complexity and layering of the distortion.

**Mode** (`Enum`)  
Specifies the mode used for iterative warping, affecting how each pass is calculated and applied.

- **Bitmap** — Applies the warp using standard bitmap-based processing for each iteration.
- **Vector Field** — Uses a vector field to guide the warp, allowing for more directional and flow-based distortions.
- **Vector Field Integral** — Integrates the vector field over multiple passes, producing complex, cumulative warp effects.
