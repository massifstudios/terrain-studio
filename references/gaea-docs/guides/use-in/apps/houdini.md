Source: https://docs.gaea.app/guides/use-in/apps/houdini.html

# Import Gaea terrain as a mesh (OBJ/FBX)

You may wish to try [Gaea2Houdini](https://docs.gaea.app/guides/use-in/bridges/gaea2houdini/index.html) to use Gaea directly inside Houdini.

1. **Create a Geometry container**
   - `/obj` → **Tab → Geometry** → dive inside.
2. **Import the mesh**
   - **OBJ**: add a **File SOP** and pick the `.obj`.
   - **FBX**: use **File → Import → Filmbox FBX…** (creates a network), then dive to the SOP level.
3. **Scale the normalized 0..1 mesh to meters**
   - Append **Match Size SOP**.
   - Turn **Scale to Fit** **ON** (this is the node’s scaling feature). ([?](https://www.sidefx.com/docs/houdini/nodes/sop/matchsize.html?utm_source=chatgpt.com "Match Size geometry node"))
   - Turn **Uniform Scale** **OFF** (so you can fit a non-cube: 5000×2500×5000). ([?](https://www.sidefx.com/docs/houdini/nodes/sop/matchsize.html?utm_source=chatgpt.com "Match Size geometry node"))
   - Set the target **Size** to:
     - **X = 5000**
     - **Y = 2500**
     - **Z = 5000**
4. **Fix orientation if needed**
   - Houdini terrains typically live on the ground plane; HeightField tools default to **ZX** as the "ground" alignment. ([?](https://www.sidefx.com/docs/houdini/nodes/sop/heightfield_file.html "HeightField File"))
   - If your imported mesh is "standing up" or using a different up-axis, add a **Transform SOP** and rotate until it sits flat.

---

# Import as a Heightfield

1. **Create a Geometry container**
   - `/obj` → **Tab → Geometry** → dive inside.
2. **Load the heightmap as a Heightfield**
   - Add **Heightfield File SOP**.
   - Set **File** to your heightmap (prefer EXR/32-bit+).
3. **Set terrain footprint to 5000 m**
   - In **Size**:
     - **Size Method = Size of Largest Axis**
     - **Size = 5000** (meters). ([?](https://www.sidefx.com/docs/houdini/nodes/sop/heightfield_file.html "HeightField File"))
4. **Set max altitude to 2500 m**
   - In **Height**:
     - **Height Scale = 2500** (this scales the height values from the image). ([?](https://www.sidefx.com/docs/houdini/nodes/sop/heightfield_file.html "HeightField File"))
       With your convention (black=0, white=ceiling), this makes white map to ~2500 m.
5. **(Optional) Confirm alignment**
   - HeightField File’s default **Orientation** is **ZX** (ground plane). ([?](https://www.sidefx.com/docs/houdini/nodes/sop/heightfield_file.html "HeightField File"))
