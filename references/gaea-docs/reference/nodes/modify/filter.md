Source: https://docs.gaea.app/reference/nodes/modify/filter.html

# Filter

Modify › Profile

Shortcode `fl`

Applies profile-based filtering to reshape terrain values and refine feature response.

The Filter node shapes the terrain's frequency content using an audio-inspired parametric filter. Useful for isolating or suppressing features at specific scales.

## When to Use It

Use Filter when you need a profile-based remap that changes how terrain values respond without fully rebuilding the form.

## Visual Direction

![](https://docs.gaea.app/.data/assets/Filter_VD.png)

## Properties

**Mode** (`Enum`)  
Selects the filter response curve used to shape terrain detail by frequency (scale).

- **Low Pass** — Keeps large-scale forms and smooths out high-frequency detail.
- **High Pass** — Keeps fine detail and removes broad, low-frequency forms.
- **Low Shelf** — Boosts or cuts low frequencies while leaving higher frequencies mostly unchanged.
- **High Shelf** — Boosts or cuts high frequencies while leaving lower frequencies mostly unchanged.
- **Band Pass** — Keeps a band of mid frequencies and attenuates both low and high frequencies outside the band.
- **Notch** — Removes a narrow band of frequencies while leaving the rest mostly unchanged.
- **Bell** — Boosts or cuts a centered frequency band with a controllable bandwidth.

**Frequency** (`Single`)  
The center/cutoff frequency of the filter (controls which detail scale is affected).

**Gain** (`Single`)  
Amount of boost or cut applied (used by shelf and bell modes).

**Q** (`Single`)  
Filter sharpness/bandwidth (higher Q = narrower, more selective band; lower Q = broader influence).

**Keep DC** (`Boolean`)  
Preserves the overall baseline/average height (DC offset) so the filter affects detail without shifting the mean elevation.
