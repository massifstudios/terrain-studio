Source: https://docs.gaea.app/using/advanced-topics/technical-information/file-formats.html

# File Formats

Gaea supports all modern (and some legacy) file formats for both flat files and meshes.

## For Import

### **Bitmap Formats**

| Format | 32-bit | 16-bit | 8-bit |
| --- | --- | --- | --- |
| OpenEXR | true | false | false |
| TIFF | true | true | true |
| PNG (64 / 16 / 8) | true | true | true |
| RAW (Float / Half / UShort) | true | true | false |
| JPG | false | false | false |
| Gaea RAW | true | false | false |

Raw can be saved as .raw, .r16, and .r32 as required as different applications use different combinations of format and extensions.

### 3D Formats

- Wavefront OBJ (.obj)
- Autodesk Filmbox (.fbx)
- Collada (.dae)
- WebGL (.glTF/.glb)
- Point Cloud (.xyz)

## **For Import**

Gaea supports the following formats for importing data:

- .exr
- .tif/.tiff
- .webp/.jpeg
- .webp
- .svg
- .psd
- .hdr
- .pfm
- .r32
- .raw
- .bmp

Gaea can also import 3D objects with the Object node.

## Gaea's Project Formats

Gaea saves files in the formats mentioned below.

### **.terrain**

Gaea terrain projects are saved as a `.terrain` file. All Gaea editions can read this format.

See @#advanced-automation for manipulating this file format.

## Precision

Gaea stores and processes its heightfields in 32-bit floating points, which is compatible with all professional CGI applications.

### **32-bit**

For practical purposes, exporting as either OpenEXR or TIFF will give you the best results and maximum compatibility with other applications. If you are using a custom pipeline, using R32 or PFM formats may be of more use. See the section below for those formats.

If you are saving output from Gaea to bring back to Gaea either in the same or different file, we recommend using the `.r32` or `.graw` formats for maximum fidelity and efficiency.

### **16-bit**

While displacement/heightfield information requires 32-bit precision for accuracy; color maps, masks, and other secondary data which may have fewer levels of complexity can make use of 16-bit formats, such as PNG. This can help save on disk space as larger worlds will require a lot of storage space. Storing in 16-bit also helps performance.

In fact, if your terrain does not contain many smooth details, you can even export your main displacement as 16-bit.

Game engines, such as Unity, can only import RAW 16-bit (ushort) format terrains.

### **8-bit**

In some cases, for example with black and white masks, you may not need a high level of precision at all. You can use 8-bit PNG or TIFF output which can increase performance and save disk space.

**Custom Workflows Precision**

If you are using Gaea in a custom workflow, such as automation, you may require the data to be as simple and efficiently readable as possible.

### Technical Details

32-bit float (.r32) and 16-bit ushort (.raw) are the simplest formats you can use. It is a simple binary array of `float` (IEEE 754) or `unsigned short`. The file has no header and can be read directly as a binary stream. The files will use Little Endian.

The size of the heightfield should be square root of the byte length divided by the size of the type (4 bytes for `float`, 2 bytes for `ushort`).

The `.r32` format will store values between `0.0f` and `1.0f`. While the `.raw` format will store values between `0` and `65535`.

Both formats are recommended for heightfield (grayscale) data only.
