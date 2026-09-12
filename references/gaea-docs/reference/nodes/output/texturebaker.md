Source: https://docs.gaea.app/reference/nodes/output/texturebaker.html

# TextureBaker

Output › Utilities

Shortcode `txb`

Bakes derived texture maps from the terrain for export and downstream use.

Drop a connection on the node center and you will get an option to add a new port.

## When to Use It

Use TextureBaker when you need bakes derived texture maps from the terrain for export and downstream use.

## Properties

**Generate Normals** (`Boolean`)  
Enables the generation of tangent-space normal maps for the mesh. When checked, MeshWarp will output normal maps that align with the mesh geometry at each LOD, suitable for advanced shading and detail.

**Information** (`Byte`)

**LOD Info** (`String`)  
Shows Level of Detail (LOD) information for the mesh and all connected maps. Each input map will have an LOD generated to match the mesh's LODs as set in the Mesher node.

**Add Input** (`Command`)  
Adds an additional input port to connect masks, color maps, or other data layers. Each input will be processed and fitted to the mesh at all LODs.
