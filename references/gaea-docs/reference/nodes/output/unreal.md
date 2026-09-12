Source: https://docs.gaea.app/reference/nodes/output/unreal.html

# Unreal

Output › Assets

Shortcode `ue`

Exports terrain data in Unreal-friendly resolutions, formats, and naming conventions.

Drop a connection on the node center and you will get an option to add a new port.

## When to Use It

Use Unreal when you need terrain data in Unreal-friendly resolutions, formats, and naming conventions.

## Properties

**Automatic Size** (`Boolean`)  
Automatically determines the optimal export resolution for Unreal Engine based on the input terrain and project settings.

**Target Size** (`Enum`)  
Specifies the desired export resolution for Unreal Engine heightmaps and textures. Choose from standard Unreal-compatible sizes for best results.

- **X 8129** — Exports at 8129 x 8129 pixels, suitable for very large landscapes in Unreal Engine.
- **X 4033** — Exports at 4033 x 4033 pixels, commonly used for large terrain tiles.
- **X 2017** — Exports at 2017 x 2017 pixels, ideal for medium-sized terrain tiles.
- **X 1009** — Exports at 1009 x 1009 pixels, suitable for smaller terrain tiles or test scenes.
- **X 505** — Exports at 505 x 505 pixels, used for small terrain tiles or prototyping.
- **X 253**
- **X 127**

**Dont Resize Textures** (`Boolean`)  
Prevents automatic resizing of textures during export, preserving the original resolution of input textures.

**LOD Info** (`String`)  
Provides Level of Detail (LOD) information for exported terrain, enabling optimized rendering and streaming in Unreal Engine.

**Format** (`Enum`)  
Selects the file format for exported terrain data. Choose the format that best matches your Unreal Engine workflow.

- **PNG** — Exports terrain data as PNG images, suitable for color maps and masks.
- **R 16** — Exports terrain data as 16-bit RAW files, recommended for heightmaps to preserve elevation detail.
- **RAW** — Exports terrain data as RAW files, compatible with Unreal Engine for heightmaps and other data.

**Unreal Friendly Naming** (`Boolean`)  
Applies Unreal Engine-compatible naming conventions to exported files for easier import and organization.

**Map 1** (`Enum`)

- **Height**
- **Mask**
- **Color**
- **Weight**

**Map 2** (`Enum`)

- **Height**
- **Mask**
- **Color**
- **Weight**

**Map 3** (`Enum`)

- **Height**
- **Mask**
- **Color**
- **Weight**

**Map 4** (`Enum`)

- **Height**
- **Mask**
- **Color**
- **Weight**

**Map 5** (`Enum`)

- **Height**
- **Mask**
- **Color**
- **Weight**

**Map 6** (`Enum`)

- **Height**
- **Mask**
- **Color**
- **Weight**

**Map 7** (`Enum`)

- **Height**
- **Mask**
- **Color**
- **Weight**

**Map 8** (`Enum`)

- **Height**
- **Mask**
- **Color**
- **Weight**

**Map 9** (`Enum`)

- **Height**
- **Mask**
- **Color**
- **Weight**

**Map 10** (`Enum`)

- **Height**
- **Mask**
- **Color**
- **Weight**

**Information 2** (`Byte`)  
Additional metadata or notes related to the Unreal Engine export process.

**Information 1** (`Byte`)  
General information or notes about the Unreal Engine export settings.

**Add Input** (`Command`)  
Adds an additional input map layer for export, allowing for more complex terrain data to be sent to Unreal Engine.
