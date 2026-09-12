Source: https://docs.gaea.app/reference/nodes/modify/match.html

# Match

Modify › Adjust

Shortcode

Matches the height range or histogram of one terrain to another.

## Official Video

For a video breakdown of this node, see [the official Match video](https://docs.gaea.app/videos/official/nodes/yt-match.html).

The Match node is a low-level lets you provide a Reference input that is used to adapt/match the heights of the main Input.

This is useful when you need terrains to be a precise height, but lose some heights after a particular effect, erosion, or other processes.

See [Match or Correct Heights](https://docs.gaea.app/guides/scenarios/helpful-info/match-height.html) for a practical example of how Match can be utilized.

## When to Use It

Use Match when one terrain needs to inherit the height range or elevation character of a reference input.

## Visual Direction

![](https://docs.gaea.app/.data/assets/Match_VD.png)

## Properties

**Mode** (`Enum`)  
Specifies the method used to match the height of the current terrain to a reference terrain. Determines how the height adjustment is performed.

- **Height** — Directly matches the height values of the current terrain to the reference terrain, aligning elevation point-for-point.
- **Histogram** — Matches the height distribution (histogram) of the current terrain to that of the reference terrain, preserving relative features while adjusting overall elevation statistics.
