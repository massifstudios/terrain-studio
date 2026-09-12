# Done

## T10 — Build, and standing back up

The tool is finished: what you preview, you can now produce.

**What works**

- **Build.** One button takes the graph you have been previewing at 512 and
  runs it at the resolution you chose, writing every file the graph asks for
  — the DEM and its preview image, splat maps, colour maps, point sets. It
  shows which output it is on and what the engine is chewing on, and it can
  be cancelled at any point.
- A build stops at the first thing that goes wrong rather than leaving you
  with half a set of files, and says what it was.
- Previews hold still while a build runs, and pick up again by themselves
  afterwards. Nothing has to be clicked twice.
- Opening an elevation model now sets the project up around it: the ground it
  covers and the survey coordinates it was cut from. Those travel through the
  graph untouched and into every exported file, so a refined terrain and its
  forest land exactly where the original stood. Until now they did not, and an
  export came out in plain metres from zero.
- If the engine falls over, the app does not. It says so, starts a new one,
  and the graph, the palette and the plugins come back on their own. If it
  cannot start at all after a few tries it says that instead of flickering
  forever.

**Where things are**

- `app/src/renderer/src/lib/build-runner.svelte.ts` — what a build does.
- `app/src/main/engine-connection.ts` — supervising and relaunching the
  engine.

**Useful to know**

- A build runs every node in the **Output** group. If a graph has none, the
  build says so rather than appearing to succeed silently.
- Exporters that share their upstream work only pay for it once — a second
  export off the same terrain takes milliseconds.
- The build resolution is a setting of your session, not of the project. The
  file remembers the graph, not how you last built it.
- Every colour in the interface comes from the shared design system. The one
  exception is the window's own background before the page has painted, which
  Electron needs before any stylesheet exists.

## T9 — Plugins

Terrain Studio can now be extended without touching it. Someone else's node
appears in the palette, the properties panel and the code view exactly like
one of ours.

**What works**

- Shared libraries dropped into a plugin folder are loaded when the engine
  starts, and again whenever you press **Plugins** in the top bar. The bar
  says how many loaded, and shows in red anything that was turned away and
  why.
- A plugin built for a different version of the interface is refused with a
  message naming both versions. It never crashes the app and is never quietly
  ignored.
- Opening a project that uses a node you do not have no longer costs you
  anything: the node stays in the graph marked *not installed*, keeps every
  setting and every connection, and simply refuses to run. Save the project
  and it all comes back out untouched.
- The repo carries a working example plugin, `Speckle`, in about a hundred
  lines of C. It builds from one header, links nothing, and the test suite
  builds and runs it on every test run — so the interface cannot quietly rot.

**Where things are**

- `PLUGINS.md` — how to write one, start to finish.
- `plugins/example/` — the example, which is also the template.
- `engine/sdk/terrain_node_sdk.h` — the interface itself.

**Useful to know**

- Two folders are looked at: your own, under the app's user data, and
  `plugins/` beside the application. Libraries sitting directly in either are
  loaded; anything in a subfolder is ignored, so a plugin's source can live
  next to its built form.
- Rescanning only adds. A plugin already loaded stays as it is — replacing one
  means restarting the app.
- The interface is now fixed. Anything built against it today keeps working;
  future additions arrive as a new version and the old one keeps being
  accepted.
- Plugins are bound by the same reproducibility rules as everything else, and
  the rules are written where an author will read them.

## T8 — The graph as code

The bottom panel's second tab is no longer a placeholder. The same graph is
now readable and editable as the `.massifgraph` document itself.

**What works**

- The text in the panel is the file: the same JSON the project saves as, sorted
  the same way, settings and preview node included.

  ```json
  { "id": "eroded", "type": "HydraulicErosion", "x": 420, "y": 60,
    "parameters": { "density": 2, "capacity": 1.5, ... } }
  ```

- Editing the text edits the graph as you type. Anything that does not parse,
  or that names a node type or a port the engine does not have, is refused with
  what went wrong, and the graph is left alone until it reads cleanly.
- Positions and wiring are both in the text, so applying a document moves nodes
  as well as reconnecting them.
- There is no second language for a graph: what the app writes to disk is what
  the panel shows and what `massif apply` takes.

**Where things are**

- `app/src/shared/massifgraph.ts` — the reader and writer.
- `app/src/shared/graph-validation.ts` — checking a document against the registry.
- `app/src/renderer/src/lib/graph/CodeView.svelte` — the panel.
- `ARCHITECTURE.md` §11 — the format and the rules around it.

**Useful to know**

- Every parameter is written, whether or not it differs from the node's default,
  so a later change to a default cannot quietly alter a saved graph.
- Ports are indices into the lists `massif describe` prints, not names.
- While your cursor is in the box the text is left exactly as you typed it;
  clicking away tidies it up.
- The seed, world size and resolution are in the document as well as in the top
  bar — editing either one is the same edit.

## T7 — Texturing and vegetation

Terrain Studio no longer produces only shape. It now produces what covers the
shape as well.

**What works**

- `ColorRamp` turns any field into a colour map through a gradient you edit in
  the property panel, and `MixColor` blends two of them through a mask.
- `SplatWeights` stacks up to three ground layers over a base like paint —
  scree over soil, rock over scree, snow over everything — so each cell's four
  shares add to exactly one, with anything uncovered falling to the base layer
  rather than leaving a hole.
- `GrowthMask` answers "where would this grow?" in one node instead of five:
  an altitude band, a slope limit and moisture, together.
- `Scatter` places instances over the ground with a real minimum spacing in
  meters, each with its own rotation and scale. A forest, in other words.
- Four new ways out: 16-bit PNG heightmaps, RGBA splat maps, RGB colour maps,
  and point sets as either CSV or JSON. Like the DEM export, they write only
  during a build.
- The preview understands all of it. A colour map is painted onto the terrain
  keeping its own light, and scattered instances appear as markers in both the
  3D and the top-down view, standing on the ground they were placed on.

**Where things are**

- `engine/src/nodes/texture_nodes.cpp` and `vegetation_nodes.cpp` — the nodes.
- `engine/src/formats/texture_maps.cpp`, `point_files.cpp` — the new writers.
- `app/src/renderer/src/lib/controls/RampEditor.svelte` — the gradient editor.

**Useful to know**

- Colours are written out exactly as you picked them. A stop set to #808080
  leaves the tool as 128, 128, 128 — no gamma is guessed at anywhere.
- Point positions are written in the survey coordinates of whatever DEM they
  came from, so a forest lines up with the terrain it belongs to.
- A scatter is reproducible down to the individual tree: every instance has an
  id derived from the seed, so the same graph gives the same forest, and
  adding a tree somewhere never renumbers the rest.

## T6 — Erosion

The part the tool exists for. A coarse survey DEM now gains believable relief
at a scale the survey never recorded.

**What works**

- `HydraulicErosion` runs rain over the terrain: water finds its way downhill,
  cuts channels, carries material and lays it down again where it slows. It
  also reports where it wore the ground away, where it left sediment, and
  where the water ran — three extra outputs you can use as masks.
- `ThermalErosion` lets loose material slide until nothing is steeper than the
  angle it will hold, which is what gives scree slopes and rounded shoulders.
- `Deposit` releases sediment over the whole terrain and lets it settle, so it
  slides off anything too steep and gathers in the hollows. Only the sediment
  moves; the rock underneath stays where it is.
- `FlowMask` follows water across the whole terrain and marks where it
  gathers — the river network, without eroding anything.
- All of them take a mask, so erosion can be confined to the steep ground or
  kept out of a valley floor.

**Where things are**

- `engine/src/nodes/erosion_nodes.cpp` — the three erosion nodes.
- `engine/tests/test_erosion.cpp` — including the milestone check that a
  coarse DEM gains detail while staying the same mountain.

**Useful to know**

- Measured on the game's own `diffusion.massifdem` (30 m per pixel): previewed
  at 512, thermal plus hydraulic erosion through a slope mask multiplies the
  fine relief by about four while moving the summit by well under one percent.
  It takes about half a second.
- **The droplet count is a quality dial, not a strength dial.** Raising it
  makes the same result cleaner rather than eroding more, so you can preview
  cheaply and build finely without the terrain changing character. Erosion
  strength lives in Capacity and Droplet life.
- Erosion is the slow part. Half a second at 512, a few seconds at 1024, and
  minutes at build resolutions — the cost rises with both the resolution and
  the channel width, since a wider channel means more ground per droplet.
- Erosion at 512 will not match erosion at 8192. That is expected of tools of
  this kind: the same rain falling on a finer grid finds finer paths.

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
