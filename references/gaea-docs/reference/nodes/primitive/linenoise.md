Source: https://docs.gaea.app/reference/nodes/primitive/linenoise.html

# LineNoise

Primitive › Basic

Shortcode `ln`

LineNoise is a geometry generator that creates sets of lines which can be distorted and used with other noises and fractals to produce layered ridges.

Node must be baked when executing Hybrid or Tiled builds.

The Auto Level, Log, and Equalize post-process options can be very useful for managing the noise values.

## When to Use It

Use LineNoise when you need a geometry generator that creates sets of lines which can be distorted and used with other noises and fractals to produce layered ridges.

## Visual Direction

![](https://docs.gaea.app/.data/assets/LineNoise_VD.png)

## Properties

**Bucket Size** (`Enum`)

- **X 1**
- **X 2**
- **X 3**
- **X 4**

**Style** (`Enum`)

- **Sharp**
- **Soft**
- **Flat**
- **Vague**

**Clamp** (`Single`)

**Direction** (`Int32`)

**Seed** (`Int32`)  
The randomization pattern or seed for the node's process.
