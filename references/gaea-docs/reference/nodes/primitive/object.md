Source: https://docs.gaea.app/reference/nodes/primitive/object.html

# Object

Primitive › Assets

Shortcode `o`

Loads external object or mesh data for use inside the graph.

Node must be baked when executing Hybrid or Tiled builds.

## When to Use It

Use Object when you need loads external object or mesh data for use inside the graph.

## Visual Direction

## Properties

**Mesh** (`String`)  
Specifies the 3D mesh file to import. The mesh will be converted and rendered as a heightfield for use in terrain generation.

**Relative Path** (`Boolean`)  
Defines the file path to the mesh relative to the project or asset directory. Ensures portability and correct asset referencing.

**Drop to Floor** (`Boolean`)  
Automatically positions the object so its lowest point rests on the terrain base, preventing it from floating or being embedded below the surface.

**Offset X** (`Single`)  
Shifts the object horizontally along the X axis, allowing precise placement within the terrain.

**Offset Y** (`Single`)  
Shifts the object horizontally along the Y axis, allowing precise placement within the terrain.

**Offset Z** (`Single`)  
Moves the object vertically along the Z axis, raising or lowering it relative to the terrain surface.

**Scale** (`Single`)  
Group containing parameters for scaling the imported object.

**Pitch** (`Single`)  
Rotates the object around the X axis, tilting it forward or backward.

**Yaw** (`Single`)  
Rotates the object around the Y axis, turning it left or right.

**Roll** (`Single`)  
Rotates the object around the Z axis, spinning it clockwise or counterclockwise.

**Antialiasing** (`Boolean`)  
Enables or adjusts antialiasing for the heightfield rendering, reducing jagged edges and improving visual smoothness.
