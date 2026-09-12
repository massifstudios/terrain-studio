Source: https://docs.gaea.app/reference/nodes/output/unity.html

# Unity

Output › Assets

Shortcode `un`

Exports terrain data in Unity-friendly resolutions and formats.

Drop a connection on the node center and you will get an option to add a new port.

## When to Use It

Use Unity when you need terrain data in Unity-friendly resolutions and formats.

## Properties

**Target Size** (`Enum`)  
Specifies the export resolution for Unity terrain heightmaps and textures. Select from standard Unity-compatible sizes to ensure proper import and terrain fidelity.

- **X 8193** — Exports at 8193 x 8193 pixels, suitable for very large Unity terrains requiring maximum detail.
- **X 4097** — Exports at 4097 x 4097 pixels, commonly used for large terrain tiles in Unity.
- **X 2049** — Exports at 2049 x 2049 pixels, ideal for medium-sized Unity terrain tiles.
- **X 1025** — Exports at 1025 x 1025 pixels, suitable for smaller terrain tiles or test scenes in Unity.
- **X 513** — Exports at 513 x 513 pixels, used for small terrain tiles or prototyping in Unity.

**Add Input** (`Command`)  
Adds an additional input map layer for export, allowing for more complex terrain data to be sent to Unity.
