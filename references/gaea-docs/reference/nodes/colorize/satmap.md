Source: https://docs.gaea.app/reference/nodes/colorize/satmap.html

# SatMap

Colorize › Color

Shortcode `sa`

Generates terrain color from preset satellite-style palettes with controllable range and processing.

## When to Use It

Use SatMap when you need a fast, realistic base color pass derived from satellite-style palettes. It is strongest early in the color pipeline, where it can establish believable terrain color before you refine, layer, or weather that color with Mixer, ColorErosion, or other color nodes.

## Visual Direction

## Properties

**Library** (`Enum`)

- **New**
- **Rock**
- **Sand**
- **Green**
- **Blue**
- **Color**

**Library Item** (`Int32`)

**Range** (`Float2`)

**Bias** (`Single`)

**Enhance** (`Enum`)

- **None**
- **Autolevel**
- **Equalize**

**Reverse** (`Boolean`)

**Rough** (`Enum`)

- **None**
- **Low**
- **Med**
- **High**
- **Ultra**

**Hue** (`Single`)

**Saturation** (`Single`)

**Lightness** (`Single`)

**Randomize** (`Command`)
