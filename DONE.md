# Done

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
