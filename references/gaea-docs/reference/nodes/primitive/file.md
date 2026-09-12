Source: https://docs.gaea.app/reference/nodes/primitive/file.html

# File

Primitive › Assets

Shortcode `f`

File loads any supported image or data file as a terrain, and supports all major file formats. You can also transform the input.

Node must be baked when executing Hybrid or Tiled builds.

The File node loads any supported image or data file as a terrain. It
supports all major file formats, and you can load either a grayscale
image as a heightfield, or a RGB color image as a color map.

## When to Use It

Use File when you need loads any supported image or data file as a terrain.

## A note about scale

In most cases, you can use the `Raw` Scale setting. This will interpret
the data within a normal 8, 16, or 32 bit data range. If your uses
excessive scale values that go below zero, you may need to use the
`Normalized` or `Mapped` mode. If you wish to handle scale manually, you
should turn on `Clamp` in [Modifier Stack](https://docs.gaea.app/ui/interface/property-editor/modifier-stack.html) to ensure data does not go beyond
anticipated ranges which may cause other nodes to break.

The `Mapped` mode is mainly used for communicating with other
applications in automation scenarios where the incoming and outgoing
data must retain a specific number range.

## Properties

**File** (`String`)  
Specifies the file to load as input for the node. This can be a heightmap, mask, or other supported data file.

**Relative Path** (`Boolean`)  
Defines the file path relative to the project or asset directory, ensuring portability and correct asset referencing.

**Is RGB** (`Boolean`)  
Indicates whether the loaded file should be interpreted as an RGB image. Enable this for color maps or multi-channel data.

**Enforce Linear Gamma** (`Boolean`)  
Forces the loaded terrain or image data to use linear gamma correction, ensuring accurate color and height interpretation.

**Allow Unclamped** (`Boolean`)  
Allows values outside the standard 0–1 range to be loaded, preserving high dynamic range or special data in the file.

**Crop to Square** (`Boolean`)  
Automatically crops the loaded file to a square aspect ratio, which is often required for terrain processing.

**Information Houdini** (`Byte`)  
Displays or stores metadata related to Houdini integration, such as file attributes or import status.

**Never Cache** (`Boolean`)  
Prevents the loaded file from being cached, ensuring it is always read directly from disk.

**Prepare for Bridge** (`Command`)  
Prepares the loaded file for use with the Bridge system, enabling compatibility with external tools or workflows.

**Purge Cache** (`Command`)  
Clears the cached version of the loaded file, forcing a reload from disk on the next access.
