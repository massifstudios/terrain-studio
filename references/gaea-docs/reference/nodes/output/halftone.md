Source: https://docs.gaea.app/reference/nodes/output/halftone.html

# Halftone

Output › Render

Shortcode

Converts the terrain or mask into a halftone-style rendered output.

## When to Use It

Use Halftone when you need converts the terrain or mask into a halftone-style rendered output.

## Properties

**Pattern** (`Enum`)  
Specifies the halftone pattern or dithering algorithm used to convert a black and white map into halftone shading. Each pattern produces a distinct visual effect, simulating various print or screen halftone styles.

- **Floyd Steinberg** — Applies Floyd-Steinberg error diffusion dithering, producing a classic, high-quality halftone effect with smooth gradients.
- **Riemersma** — Uses Riemersma dithering, which distributes error along a space-filling curve for a unique, less directional halftone appearance.
- **O 2 X 2** — Uses a 2x2 ordered dithering matrix to create a small, repeating halftone pattern.
- **O 3 X 3** — Uses a 3x3 ordered dithering matrix for a slightly larger, repeating halftone pattern.
- **O 4 X 4** — Uses a 4x4 ordered dithering matrix, producing a more detailed and less repetitive halftone effect.
- **O 8 X 8** — Applies an 8x8 ordered dithering matrix for fine, detailed halftone shading with minimal visible repetition.
- **H 6 X 6 a** — Uses a 6x6 halftone matrix (variant A) for a larger, more complex dot pattern.
- **H 8 X 8 a** — Applies an 8x8 halftone matrix (variant A), generating a dense and detailed halftone texture.
- **H 6 X 6 O** — Applies a 6x6 halftone matrix (variant O), producing a unique halftone pattern with distinct dot placement.
- **H 8 X 8 O** — Uses an 8x8 halftone matrix (variant O), providing a dense and alternative halftone style.
- **C 5 X 5 B** — Uses a 5x5 circular halftone matrix with black dots, simulating traditional print halftone screens.
- **C 5 X 5 W** — Uses a 5x5 circular halftone matrix with white dots, inverting the typical halftone dot style.
- **C 6 X 6 B** — Applies a 6x6 circular halftone matrix with black dots for a slightly larger dot pattern.
- **C 6 X 6 W** — Uses a 6x6 circular halftone matrix with white dots, providing an inverted halftone effect.
- **C 7 X 7 B** — Applies a 7x7 circular halftone matrix with black dots, creating a larger, more pronounced dot pattern.
- **C 7 X 7 W** — Uses a 7x7 circular halftone matrix with white dots, offering a bold, inverted halftone style.
- **DIAG**
- **HLINE**

**Posterize** (`Boolean`)

**Depth** (`Int32`)
