Source: https://docs.gaea.app/reference/nodes/modify/transform3d.html

# Transform3D

Modify › Transform

Shortcode `t3`

Applies full 3D transformation to the terrain, including rotation and spatial adjustment.

## When to Use It

Use Transform3D when the terrain shape is mostly working and you need a targeted reshape, filter, or remap pass.

## Properties

**Drop to Floor** (`Boolean`)  
Automatically positions the object so its lowest point rests on the ground plane, preventing it from floating or being embedded below the surface.

**Offset X** (`Single`)  
Shifts the object horizontally along the X axis, allowing precise placement within the 3D space.

**Offset Y** (`Single`)  
Shifts the object horizontally along the Y axis, moving it forward or backward in the 3D scene.

**Offset Z** (`Single`)  
Moves the object vertically along the Z axis, raising or lowering its elevation relative to the ground.

**Scale** (`Single`)  
Group containing parameters for scaling the object or mesh, affecting its size and proportions in 3D space.

**Pitch** (`Single`)  
Rotates the object around the X axis, tilting it forward or backward.

**Yaw** (`Single`)  
Rotates the object around the Y axis, turning it left or right.

**Roll** (`Single`)  
Rotates the object around the Z axis, spinning it clockwise or counterclockwise.

**Antialiasing** (`Boolean`)  
Enables or adjusts antialiasing for the 3D transformation, reducing jagged edges and improving visual smoothness.
