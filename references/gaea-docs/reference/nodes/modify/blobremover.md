Source: https://docs.gaea.app/reference/nodes/modify/blobremover.html

# BlobRemover

Modify › Utilities

Shortcode `blo`

BlobRemover can remove smaller fragments or blobs near larger shapes. This is very useful for cleanup.

## Official Video

For a video breakdown of this node, see [the official BlobRemover video](https://docs.gaea.app/videos/official/nodes/yt-blobremover.html).

A low-level node that helps isolate and remove small blobs or disconnected shapes.

![](https://docs.gaea.app/.data/assets/Gaea_-_Untitled_10-43-03-PM.webp)

## When to Use It

Use BlobRemover when masks or heightfields contain small stray islands that should be cleaned up without changing the main shapes.

## Properties

**Range** (`Single`)  
Controls the sensitivity of the blob detection. Higher values detect a wider range of blob sizes or intensities.

**Connectivity** (`Int32`)  
Specifies how connected blobs must be to be considered a single shape. Higher connectivity allows blobs to be more loosely attached to larger shapes.

**Enforce Quantization** (`Boolean`)  
If enabled, forces the use of quantization during blob detection, improving efficiency and consistency in identifying blobs.

**Quantization** (`Int32`)  
Sets the quantization level for blob detection, with values between 1 and 128. Lower values provide finer detail, while higher values group similar values together for faster and more efficient blob identification.
