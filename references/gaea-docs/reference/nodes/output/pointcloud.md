Source: https://docs.gaea.app/reference/nodes/output/pointcloud.html

# PointCloud

Output › Assets

Shortcode `pc`

Exports the terrain as a point cloud in formats such as PLY, XYZ, or CSV.

## When to Use It

Use PointCloud when you need the terrain as a point cloud in formats such as PLY, XYZ, or CSV.

## Properties

**Format** (`Enum`)  
Specifies the file format used for exporting the point cloud. Choose the format that best fits your workflow or target application.

- **PLY** — Exports the point cloud in the standard ASCII PLY (Polygon File Format) format, widely supported for 3D data exchange.
- **PLY BINARY** — Exports the point cloud in binary PLY format, offering smaller file sizes and faster read/write performance compared to ASCII.
- **XYZ** — Exports the point cloud as a simple XYZ text file, with each line representing a point's X, Y, and Z coordinates.
- **CSV** — Exports the point cloud as a CSV (Comma-Separated Values) file, suitable for use in spreadsheets or data analysis tools.

**Scale** (`Enum`)  
Sets the unit or normalization method for the exported point cloud coordinates. Determines how the point positions are scaled in the output file.

- **Normalized** — Exports coordinates normalized to a 0–1 range, useful for workflows that require unitless or relative positions.
- **Meter** — Exports coordinates in meters, matching real-world scale for compatibility with GIS and engineering applications.
- **Kilometer** — Exports coordinates in kilometers, suitable for large-scale terrain or geospatial datasets.

**Center Pivot** (`Boolean`)  
If enabled, centers the point cloud around the origin by offsetting all points so that the dataset's centroid is at (0,0,0). Useful for easier manipulation in 3D applications.

**Ignore Zero Values** (`Boolean`)  
When enabled, points with zero or null values are excluded from the export. This helps remove empty data such as the normal low floor of the terrain from the output file.
