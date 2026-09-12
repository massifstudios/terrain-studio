Source: https://docs.gaea.app/reference/nodes/modify/median.html

# Median

Modify › Blur

Shortcode `md`

Applies a median filter to remove spikes, speckles, and small noise while preserving edges better than blur.

## When to Use It

Use Median when the terrain shape is mostly working and you need a targeted reshape, filter, or remap pass.

## Visual Direction

![](https://docs.gaea.app/.data/assets/Median_VD.png)

## Properties

**Mode** (`Enum`)

- **Classic**
- **Advanced**
- **Advanced 2**

**Strength** (`Enum`)

- **X 1**
- **X 2**
- **X 4**
- **X 8**

**Horizontal** (`Int32`)

**Vertical** (`Int32`)

**Iterations** (`Int32`)

**Quality** (`Enum`)

- **Low**
- **Medium**
- **High**
