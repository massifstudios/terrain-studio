# Done

## T5 — Generators, filters and masks

There is now something to build terrain *out of*, not just something to build
terrain *with*.

**What works**

- Three ways to make terrain from nothing: `Perlin` for rolling ground,
  `RidgedNoise` for sharp crests and rounded valleys, `Voronoi` for mesas,
  cracked plates or the seams between them. A `Gradient` gives a straight
  ramp across the world for tilting or masking by direction.
- Reshaping: `Terrace` cuts a field into level steps, `Curve` remaps it
  through a curve you draw by hand, `Clamp` holds it between a floor and a
  ceiling, and `Warp` pulls it sideways by the shape of another field to
  break up anything that looks too regular.
- Four ways to select part of a terrain: by steepness (`SlopeMask`), by
  altitude (`HeightMask`), by whether the ground is a hollow or a rise
  (`CurvatureMask`), and by which way a slope faces (`AspectMask`).
- All six ways of combining two fields: `Add`, `Subtract`, `Multiply`, `Min`,
  `Max`, and `Lerp` for blending between two of them.
- Every filter takes a mask, so an effect can be applied to only part of a
  terrain — steep ground only, or above the treeline only.
- A curve editor in the property panel: drag a point to move it, click empty
  space to add one, right-click to remove it.

**Where things are**

- `engine/src/nodes/` — one file per category; adding a node still touches
  nothing outside the engine.
- `app/src/renderer/src/lib/controls/CurveEditor.svelte` — the curve control.

**Useful to know**

- Generators are measured in meters, not pixels, so the same settings give the
  same landscape whether you are previewing at 512 or building at 8192 — the
  higher resolution simply adds finer detail.
- A generator's own seed varies just that node. Rolling it leaves the rest of
  the graph exactly as it was.
- Masks show as a colour ramp draped over the terrain rather than as a picture
  of their own.

## T4 — Building graphs

The tool is now usable: you can build a terrain graph by hand.

**What works**

- A node editor in the bottom panel. Right-click or press Tab for a searchable
  palette of every node the engine offers; drag nodes around; drag from an
  output to an input to connect them. A connection that would not work —
  wrong kind of data, or one that would loop back on itself — is refused while
  you drag, shown in red. Shift-click and box-drag select several nodes,
  Delete removes them, and the surface pans with the middle button or the
  space bar and zooms on the wheel.
- Every node carries a small picture of its own result, lit the same way the
  exported preview is, plus a dot for how fresh it is and how long it took.
- The right-hand panel builds itself from whichever node is selected —
  sliders, checkboxes, dropdowns, seed rolls and file pickers, with units. It
  knows nothing about any node in particular.
- Selecting a node shows its result in the preview. A mask is draped over the
  nearest terrain above it rather than shown on its own.
- Editing anything re-runs only what changed and updates the preview, with the
  older run cancelled.
- Projects save and open as `.massifgraph`. The file is sorted and readable,
  so two versions of a project actually diff.
- The top bar carries the project name, seed, world size, and the preview and
  build resolutions. Build itself is still to come.

**Where things are**

- `app/src/renderer/src/lib/graph/` — the node editor.
- `app/src/renderer/src/lib/properties/` — the generated property panel.
- `app/src/shared/graph-operations.ts` — the editing rules, tested on their
  own.
- `app/src/shared/massifgraph.ts` — the project file.

**Useful to know**

- The app opens with an empty graph. Add an `ImportDEM`, point it at a file,
  and go from there.
- Colour maps and scattered points get their own previews when those kinds of
  data exist, later on.

## T3 — The preview area

You can now see the terrain.

**What works**

- Opening an elevation model shows the mountain in two views side by side: an
  orbiting 3D view and a top-down map, each with its own pan and zoom. The 2D
  view can be switched off to give the 3D view the full width.
- The sun can be moved — azimuth and elevation — and both views relight
  immediately. The lighting matches the preview image the tool writes beside
  an exported DEM, so what you see is what you get.
- Move the pointer over either view and the corner readout shows the position
  and the altitude under it; the other view marks the same spot.
- The engine reports progress per node while it works and a new preview
  cancels the one before it, so dragging never queues up stale work.

**Where things are**

- `app/src/renderer/src/lib/preview/` — the two viewports and the shading
  they share.
- `bridge/` — now carries graph evaluation, progress and cancellation, not
  just a version string.

**Useful to know**

- The terrain shown at startup is a placeholder file the repo carries. Point
  `TERRAIN_STUDIO_DEMO_DEM` at another `.massifdem` to look at something else
  until the open dialog arrives.
- Nothing in the interface works out a height. It draws the numbers the
  engine hands it, and reads them back cell by cell for the readout.

## T2 — The node graph engine

Terrain Studio can now run a graph.

**What works**

- Nodes are registered, not listed anywhere. Adding one is a single
  registration and it shows up everywhere — palette, properties, and later
  the code view — with no other edits. Custom nodes from plugins will arrive
  through exactly the same door, which is why the built-in nodes already use
  it.
- Graphs are checked before they run: unknown node types, missing
  connections, mismatched port types and loops are all reported rather than
  crashing or hanging.
- Results are remembered. Changing one setting re-runs that node and the ones
  downstream of it, and leaves everything else alone.
- Independent branches run at the same time, and the result is identical
  whether one thread or eight did the work.
- The first five nodes: import a DEM, a flat constant, blur, add, and export.
- Export only writes files during a real build, never while you are dragging
  a slider.

**Where things are**

- `engine/sdk/terrain_node_sdk.h` — the plugin boundary, and where the
  determinism rules are written down for node authors.
- `engine/src/nodes/` — the built-in nodes, one file per category.
- `ARCHITECTURE.md` §5 — how caching, seeding and scheduling are decided.

**Useful to know**

- A graph runs at one square resolution: low while you work, high when you
  build. Erosion at low resolution will not match erosion at high resolution;
  that is expected and is how tools of this kind behave.

## T1 — Heightfield and file formats

The engine can now read and write real terrain.

**What works**

- Opening and saving `.massifdem` files, the format the game Massif uses.
  A file written here is byte-for-byte the same terrain as the one that went
  in, and it carries its survey coordinates through untouched.
- Saving the hillshade preview image beside a DEM, looking exactly like the
  ones the game's own tools produce.
- Importing 16-bit PNG heightmaps from other terrain tools, and exporting
  them back out.
- Importing headerless raw float32 grids, for tools that export nothing else.

**Where things are**

- `engine/include/terrain/formats/` — one reader/writer per format.
- `engine/tests/data/` — the reference DEM and preview the tests check
  against, plus the script that regenerates them from the game's own writer.

**Useful to know**

- Nothing about the game leaks past the format readers. Every other part of
  the engine only ever sees a plain grid of heights.

## T0 — Skeleton

The whole toolchain is proven end to end: a C++ engine, a native addon, and
an Electron window that talks to it.

**What works**

- The app opens a window with the four-panel layout stubbed out: top bar,
  preview area (3D and 2D panes, 2D toggleable), bottom graph/code panel,
  right-hand properties panel. All chrome comes from the Neoworks design
  system.
- The engine runs in its own process. The top bar shows its live status and
  the version it reports — that number travels from C++ through the native
  addon, across a direct port to the window.
- Building a package produces a runnable app folder with the engine binary
  bundled beside it.

**Where things are**

- `engine/` — the C++ core and its test suite.
- `bridge/` — the native addon that exposes the engine to the app.
- `app/` — the Electron shell and the Svelte interface.
- `ARCHITECTURE.md` — the decisions that are now locked.

**Useful to know**

- `task dev` runs the app, `task build` builds everything, `task test` runs
  both test suites, `task package` produces the distributable folder.
- The `task` runner is installed here as `go-task`; either name works if
  `task` is on the path.
