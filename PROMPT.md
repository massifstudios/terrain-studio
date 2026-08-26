# Terrain Studio — Build Prompt

You are building **Terrain Studio**: a general-purpose desktop
terrain-generation application in the spirit of QuadSpinner Gaea — import a
DEM or generate one, refine it through a node graph, preview it in 3D, export
it. It is a Neoworks tool, not tied to any one project.

Its first customer is the game **Massif** (an Unreal Engine 5 mountain-resort
building game at `../massif/`), which supplies the most concrete requirements,
the `.massifdem` format, and the acceptance test. Read
`../massif/ARCHITECTURE.md` §5 before writing any code for those contracts —
but keep the tool general: nothing Massif-specific belongs in the engine core,
the graph model, or the UI. Game-specific knowledge lives entirely in format
readers/writers.

Work milestone by milestone (§9). At the end of each milestone the app must
build, run, and pass its tests. Do not build ahead of the current milestone.

## 1. Why this tool exists

The core loop of the tool, for any user:

1. **Import** a DEM (`.massifdem`, 16-bit PNG, raw float32) or generate one
   from noise.
2. **Refine** it through a node graph: erosion, detail synthesis, masks,
   combinators.
3. **Derive targets** from the refined ground: texture weight maps (splat
   layers), color maps, vegetation density maps and scattered instance
   points.
4. **Preview** everything interactively in 3D.
5. **Export** heightfield and targets in the formats the target pipeline
   reads.

The workload that shapes v1 comes from Massif. That game's terrain macro
shape is **imported, not generated** — a real 20 × 20 km laserscan window of
Tirol, or a window sampled from a terrain diffusion model (InfiniteDiffusion,
30 m/px), both arriving as `.massifdem` files — and its `ARCHITECTURE.md`
§5.1.1 leaves one step open: **geological refinement**, erosion over the
imported surface to smooth scanner artefacts, cut detail below the source
resolution, and age the ground. This tool is where that step happens.

The diffusion path is the demanding workload: 30 m/px input must gain
believable sub-30 m detail (hydraulic and thermal erosion, ridge noise
constrained by the macro shape) so that a generated massif and a surveyed one
are indistinguishable downstream. A tool that does that well is a good
terrain tool in general — none of it is game-specific.

## 2. Stack (decided — do not substitute)

| Layer | Choice |
|---|---|
| Shell | Electron (current stable), Vite, TypeScript, bun as package manager |
| UI | Svelte 5, `@neoworks-dev/ui` design system (linked via `bun link`, see §3.6) |
| 3D viewport | Three.js, WebGL2 |
| Node editor | Custom-built (see §3.5) — no graph-editor library |
| Engine | C++20 static library, no Electron/Node dependencies |
| Bridge | N-API addon built with `cmake-js`, loaded in an Electron `utilityProcess` |
| C++ tests | Catch2 (or doctest — pick one, use it everywhere) |
| TS tests | `bun test` |
| Task runner | `Taskfile.yml` (the game uses Task; mirror it: `task dev`, `task build`, `task test`) |

## 3. Architecture

### 3.1 Process model

```
Electron main ── spawns ──► utilityProcess (Node) ── loads ──► engine.node (N-API → C++ core)
      │                            ▲
      └────► renderer (Svelte UI) ─┘  MessagePort: commands, progress, transferable Float32Arrays
```

- **All terrain math lives in the C++ core.** The UI never computes a height
  value. If a feature seems to need terrain math in TypeScript, the engine API
  is missing something — extend the engine.
- The engine runs in a `utilityProcess` so a native crash cannot take the UI
  down; the renderer shows "engine crashed, restarting" and relaunches it.
- Heightmaps cross the boundary as **transferable `ArrayBuffer`s** (zero-copy
  where possible), never as JSON.
- Evaluation is asynchronous with **progress reporting** (per node: name,
  fraction complete) and **cancellation** — starting a new preview evaluation
  cancels the previous one.

### 3.2 C++ core

A standalone library (`engine/`) with its own CMake build and test binary,
compilable and testable without Electron.

**Heightfield.** The working type is a row-major `float32` grid with world
size in meters and an optional georeference (EPSG code, easting/northing of
the north-west corner — carried through from import to export, untouched).
Row 0 is north, matching `.massifdem`.

**Node registry.** Nodes are registered, not enumerated — one registration
adds a node to the palette, the property panel, the DSL, and the serializer
with no further edits (this mirrors the game's channel registry,
`../massif/ARCHITECTURE.md` §5.6.2). A node declares:

- name, category, description
- input ports and output ports, typed. Port types: **field** (heightfield
  and mask are the same float32 grid, semantic tag only), **color** (RGB
  grid), **points** (a flat list of instances: position, rotation, scale,
  seed-derived id). Connections type-check against these.
- parameters, each with: name, type (float / int / bool / enum / seed /
  color ramp), default, min/max, unit (meters where physical), description

The registry is queryable from TypeScript as JSON; the property panel and node
palette are generated from it. Adding a node must require zero frontend code.
The registry is also the plugin boundary (§3.7): built-in nodes register
through the same descriptor structures and the same registration call that
plugins use, so the plugin ABI cannot rot — the engine is its own first
plugin client.

**Graph evaluation.** Directed acyclic graph (cycles are a validation error —
unlike the game's bake there are no relaxation groups here; keep it simple).
Topological sort; per-node result cache keyed by a hash of (node type, node
version, parameter values, input hashes, resolution, seed). Editing a
parameter dirties that node and its transitive dependents, nothing else.
Evaluation runs on a thread pool; independent branches run in parallel.

**Determinism.** Same contract as the game (`../massif/ARCHITECTURE.md` §4):

- Every stochastic node takes the graph seed combined with a per-node stable
  id — adding a node never shifts another's random sequence.
- Fixed iteration counts, never convergence tests.
- Same graph + seed + resolution ⇒ **bit-identical output**, across runs and
  across thread counts. Where parallel workers scatter into shared cells
  (droplet erosion), accumulate through integer atomics on a fixed-point
  encoding, not float atomics.
- No wall-clock time, no unseeded randomness anywhere in the engine.
- A determinism test (hash after full evaluation of a reference graph, run
  twice, and at 1 thread vs N threads) is part of the suite from the first
  stochastic node onward.

**Resolution.** The graph evaluates at a chosen square resolution: preview
(512 or 1024) for interactive editing, build (up to 10240) for export.
Parameters are in physical units (meters) wherever possible so results scale
sensibly across resolutions. Erosion at 512 will not match erosion at 8192 —
that is accepted (Gaea has the same property); document it, do not fight it.

### 3.3 File formats

Formats are readers/writers at the edge of the engine — the graph and every
node see only the heightfield type. This is where per-project knowledge
lives, and the only place it may live; adding a format touches no node and no
UI beyond the file dialogs.

**`.massifdem`** — the contract with Massif. Byte layout is defined by
`../massif/Tools/massif_dem.py` (`MASSIFDM` magic, version 1, 192-byte header,
uint16 samples quantised over the window's own altitude band, row-major, row 0
north). Implement read and write in C++ and enforce compatibility with a test
that round-trips against a file produced by the Python reference (vendor a
small generated fixture; do not depend on the game checkout at test time).
Export also writes the hillshade preview PNG beside the file, matching
`massif_dem.py`'s `write_preview` (315° sun azimuth, 45° altitude, altitude
ramp).

**16-bit PNG** import/export — interop with other terrain tools.

**Raw float32** import (width/height/spacing prompted) — escape hatch.

**Splat maps** — up to four masks packed into RGBA channels of an 8-bit PNG
(Unreal/Unity landscape-layer convention), one file per group of four.
Single masks also export as 16-bit PNG.

**Color maps** — 8-bit RGB PNG.

**Point sets** — CSV (`x_meters,y_meters,altitude_meters,rotation_degrees,
scale,id`) and a JSON variant; consumers rasterize or instance as they
please.

**`.massifgraph`** — the project file. JSON: format version, graph seed,
world size, georeference, nodes (type, id, position, parameters), edges,
selected preview node. Human-diffable; stable key order.

GeoTIFF/GDAL: out of scope for v1 — Massif's own fetch tools already produce
`.massifdem`, and general GeoTIFF support is a later format-module addition,
not an architectural question.

### 3.4 Node set (v1)

| Category | Nodes |
|---|---|
| Input | `ImportDEM` (file path parameter), `Constant`, `Gradient` |
| Generate | `Perlin`, `RidgedNoise`, `Voronoi` |
| Erode | `ThermalErosion` (talus-angle material slump), `HydraulicErosion` (droplet-based, deterministic per §3.2; secondary outputs: wear, deposit, flow), `Deposit` (sediment fill of concavities) |
| Filter | `Blur`, `Terrace`, `Curve` (remap by editable curve), `Clamp`, `Warp` (domain distortion by another field) |
| Mask | `SlopeMask`, `HeightMask`, `CurvatureMask`, `FlowMask` (flow accumulation), `AspectMask` (facing direction) |
| Combine | `Add`, `Subtract`, `Multiply`, `Lerp` (with mask input), `Min`, `Max` |
| Texture | `ColorRamp` (field → color via editable ramp, the SatMap role), `MixColor` (blend two color inputs by a mask), `SplatWeights` (normalize up to four masks so they sum to 1 per cell) |
| Vegetation | `GrowthMask` (convenience: altitude band × slope limit × moisture proxy from flow, one node instead of a five-node subgraph), `Scatter` (density field → deterministic seeded point set, Poisson-disc spacing in meters, per-point rotation/scale jitter) |
| Output | `Export` (.massifdem + preview PNG), `ExportPNG16`, `ExportSplat` (RGBA-packed weights), `ExportColor`, `ExportPoints` |

Every filter and erosion node takes an optional mask input that scales its
effect per cell. Erosion nodes are the heart of the tool — invest the effort
there; correctness references are the standard literature (Musgrave thermal,
droplet hydraulic à la Hans Beyer's paper / Gaea's behavior).

### 3.5 Frontend layout

Gaea's arrangement, which the user has specified:

- **Center: preview area.** The preview always shows the **selected node's
  output** — selecting a different node switches the preview to that node's
  result (from cache if clean, evaluating it if dirty). What is drawn
  depends on the selected output's type:
  - *field (heightfield):* heightmap mesh (grid at preview resolution,
    vertex-displaced, recomputed normals) with a hillshade material and an
    adjustable sun direction.
  - *field (mask):* the mask draped over the nearest upstream heightfield
    as a color overlay, so a mask is always read in terrain context.
  - *color:* the color map draped over the nearest upstream heightfield.
  - *points:* instanced markers standing on the nearest upstream
    heightfield.

  The area holds **two views side by side: 3D and 2D**. The 3D view is
  Three.js with orbit/pan/zoom. The 2D view is a top-down orthographic
  render of the same selected output (hillshade for heightfields, flat for
  masks/color, dots for points) with its own pan/zoom. The 2D view can be
  toggled on and off from the viewport toolbar; when off, 3D takes the full
  width. Both views show the value under the cursor (world position,
  altitude, mask value) in a corner readout, and cursor position is
  mirrored between them as a crosshair.
- **Bottom panel: graph editor / code view, switchable.**
  - *Graph view:* a custom node editor — do not pull in a graph-editor
    library. Structure: one pannable/zoomable surface (CSS transform on a
    world container; wheel zooms about the cursor, middle-drag or
    space-drag pans), nodes as absolutely positioned Svelte components on
    it, edges as cubic béziers in an SVG layer beneath the nodes. Graph
    state (nodes, positions, edges, selection) lives in one Svelte store;
    the editor renders it and emits mutations — no state inside the editor
    components themselves.

    Feature scope, deliberately small: drag nodes; drag from a port to
    connect, with the pending edge following the cursor and invalid targets
    (type mismatch, would-create-cycle) visibly rejected; click to select,
    shift-click and box-drag for multi-select; Delete removes selection;
    node palette from the registry (searchable, opened with right-click or
    Tab) inserts at cursor. A node's dirty/cached/evaluating state and last
    execution time are visible on the node itself. Click a node to select
    it: the preview area shows its output, the property panel its
    parameters. Every evaluated node also carries a **small preview
    thumbnail** on the node body (hillshade for heightfields, grayscale for
    masks, the map itself for color, density dots for points), rendered
    from the cached result at thumbnail resolution and greyed while dirty —
    the graph should read at a glance like Gaea's does.
    No minimap, no grouping/comment boxes, no edge rerouting handles in v1.
  - *Code view:* the same graph as a small declarative textual form, e.g.
    ```
    dem    = ImportDEM(path: "oetztal.massifdem")
    eroded = HydraulicErosion(dem, droplets: 2_000_000, capacity: 4.0)
    out    = Export(eroded, path: "refined.massifdem")
    ```
    Editing either representation updates the other; the graph is the single
    source of truth and the text is parsed/printed against the registry. Node
    positions are preserved across a text edit (new nodes auto-layout).
- **Right panel: properties.** Generated from the selected node's parameter
  descriptors: sliders with numeric entry for ranged floats/ints, checkboxes,
  dropdowns for enums, a reroll button for seeds. Unit suffixes. Edits debounce
  into re-evaluation at preview resolution.
- **Top bar:** open/save `.massifgraph`, seed, world size, preview resolution
  selector, build resolution selector, Build button (full-resolution evaluate +
  export with a progress bar), engine status indicator.

### 3.6 Visual design

This is a Neoworks tool: the design comes from the shared `@neoworks-dev/ui`
design system, located at
`/home/moritz/Documents/neoworks/neoworks.dev/packages/ui` and linked into
the app via `bun link @neoworks-dev/ui`. Build the chrome — panels, toolbars,
property controls, dialogs — from its components and tokens; before creating
a new component, check the package for an existing one that fits, and follow
its guidelines when one is genuinely missing. No color literals in
components; everything goes through the design system's tokens.

Never edit anything under `node_modules/`; the copy of the package there is
the real linked source. If the link is missing, restore it with
`bun link @neoworks-dev/ui` — never install from a registry or vendor
components as a workaround.

The game's `../massif/DESIGN.md` remains relevant only for **data** display
conventions that the design system does not cover — perceptual color ramps
for heightfield/mask overlays and legends. Chrome and data stay separate:
UI state colors never leak into terrain color ramps and vice versa.

### 3.7 Plugins

The program is extensible with **native plugins that add custom nodes**, and
everything downstream of the registry — palette, property panel, DSL,
serializer, thumbnails, previews — picks a plugin node up with zero changes,
because it only ever speaks to the registry.

- A plugin is a shared library (`.so`/`.dll`/`.dylib`) in a `plugins/`
  directory (one per-user, one project-local), loaded by the engine process
  at startup and on explicit rescan.
- The interface is a **stable, versioned C ABI**: one public C header (the
  plugin SDK). A plugin exports an ABI-version function and a registration
  function that receives the registry and declares its nodes with the same
  descriptor structures built-ins use. An ABI mismatch refuses the plugin
  with a clear message in the UI — never a crash, never a silent skip.
- At evaluation, a plugin node receives a context as a function-pointer
  table (C-ABI safe): input/output buffer access, parameter reads, a
  seeded random stream derived exactly as for built-ins, progress
  reporting, and a cancellation poll. No other engine internals are
  reachable.
- **The determinism contract (§3.2) binds plugins** and is documented in
  the SDK header: seeds only from the provided stream, fixed iteration
  counts, fixed-point atomics for parallel scatter. The engine cannot
  enforce all of it, so the contract is stated where a plugin author will
  read it.
- Import and export are nodes, so **new file formats arrive as plugins**
  too — a GeoTIFF plugin needs no core change.
- A `.massifgraph` referencing a node whose plugin is absent loads with a
  placeholder node that preserves the parameters and connections and
  refuses evaluation, so opening a project never destroys data.
- A plugin crash is an engine-process crash; the existing `utilityProcess`
  recovery (§3.1) is the containment.
- The repo carries an example plugin (`plugins/example/`, one small node)
  built and loaded by the test suite — it is the ABI's regression test and
  the plugin author's template.

## 4. What not to build

- No GPU compute for the engine (WebGPU, CUDA, compute shaders). CPU first;
  the architecture keeps the door open (pure nodes, no hidden state) but v1 is
  CPU-only.
- No tiled/infinite terrain, no streaming. One finite grid in memory —
  10240² float32 is 400 MB; fine.
- Texturing stops at weight maps and color maps; no PBR material authoring,
  no texture synthesis, no painting tools. Vegetation stops at density
  fields and instance points; no mesh placement preview beyond markers, no
  species modelling. Consumers own their materials and their foliage
  systems.
- No export formats beyond §3.3.
- No auto-update, telemetry, or installer polish. Local dev tool.
- No scripted nodes (JS/Lua/WASM node authoring). Extensibility is the
  native plugin ABI (§3.7); a scripting layer is a possible later addition
  on top of it, not part of v1.

## 5. Testing

- **Engine (primary surface):** Catch2 suite run by `task test`.
  - `.massifdem` round-trip and byte-layout fixture test (§3.3).
  - Determinism hashes (§3.2).
  - Node correctness on analytic inputs — cone, ridge, tilted plane, saddle —
    asserting qualitative invariants (thermal erosion respects the talus
    angle; flow accumulates downslope; blur preserves mean). This mirrors the
    game's analytic-source test strategy.
  - Cache correctness: editing one parameter re-evaluates only dependents.
- **Frontend:** `bun test` for pure logic only — DSL parse/print round-trip,
  `.massifgraph` serialization, registry-to-UI schema mapping. No E2E/
  screenshot testing.
- Every milestone ends green.

## 6. Repository layout

```
engine/           C++ core: src/, include/, tests/, CMakeLists.txt
engine/sdk/       the public plugin C header (§3.7)
bridge/           N-API addon wrapping engine/, cmake-js build
app/              Electron + Svelte: main/, renderer/, preload
plugins/example/  example plugin: ABI regression test and author template
Taskfile.yml      dev, build, test, package
```

## 7. Documentation

Maintain `ARCHITECTURE.md` in this repo in the same spirit as the game's:
locked decisions, updated deliberately when a decision changes, never as a
side effect. Record at minimum: the process/bridge model, the determinism
contract, the cache key, the `.massifdem` compatibility commitment, and the
DSL grammar once it exists.

## 8. Ground rules

- Read `CLAUDE.md` in this repo and follow it — code style, tests, git
  workflow.
- Never modify anything under `../massif/`. It is context, not workspace. If
  the `.massifdem` format seems to need a change, stop and raise it — the
  format is owned by the game.
- When a UI decision is not covered here, resolve it against the
  `@neoworks-dev/ui` design system's guidelines rather than inventing a new
  one.
- Keep the tool general (§1). If a change would put Massif-specific knowledge
  anywhere but a format reader/writer, stop and restructure.

## 9. Milestones

**T0 — Skeleton.** Electron + Vite + Svelte app opens a window with the
four-panel layout stubbed. `engine/` builds as a library with a trivial Catch2
test. `bridge/` builds an addon exposing `engineVersion()`; the utilityProcess
loads it and the UI displays the version. `task dev`, `task build`,
`task test` all work. This milestone proves the entire toolchain; do not move
on until packaging of the native addon inside Electron works on Linux.

**T1 — Heightfield & format.** Heightfield type; `.massifdem` read/write with
the fixture round-trip test; 16-bit PNG and raw float32 import; hillshade
preview PNG writer.

**T2 — Graph engine.** Registry, parameter descriptors, DAG evaluation, cache,
dirty propagation, seed derivation. The registration path is designed
against the plugin ABI from the start (§3.7) — built-ins go through it —
but dynamic loading itself waits for T9. Nodes: `ImportDEM`, `Constant`,
`Blur`, `Add`, `Export`. Evaluation of a hardcoded test graph from the CLI
test binary. Determinism and cache tests.

**T3 — Preview area.** Bridge grows: load graph, evaluate, stream result
buffer, progress, cancellation. 3D view renders a heightfield with
hillshade, orbit camera, sun control, cursor readout; 2D top-down view
beside it, toggleable, with mirrored cursor. Hardcoded graph: open a
`.massifdem`, see the mountain in both views.

**T4 — Graph editing.** The custom node editor (§3.5) bound to the registry;
property panel generated from descriptors; selection drives the preview
area, including the per-type display rules (mask drape, color drape, point
markers) and per-node thumbnails; edits re-evaluate at preview resolution
with visible per-node state. `.massifgraph` save/load. Top bar complete
except Build.

**T5 — Generators & filters.** `Perlin`, `RidgedNoise`, `Voronoi`, `Gradient`,
`Terrace`, `Curve`, `Clamp`, `Warp`, masks, remaining combiners. Analytic
correctness tests per node.

**T6 — Erosion.** `ThermalErosion`, `HydraulicErosion`, `Deposit`, `FlowMask`.
Deterministic under threading (fixed-point atomics). This is the milestone the
tool is judged by: the acceptance test is the diffusion DEM
(`../massif/Data/Terrain/diffusion.massifdem`, if present — otherwise any
30 m/px input) gaining convincing sub-resolution relief.

**T7 — Texturing & vegetation.** `ColorRamp`, `MixColor`, `SplatWeights`,
`AspectMask`, `GrowthMask`, `Scatter`, the erosion secondary outputs, and
the `ExportSplat` / `ExportColor` / `ExportPoints` writers. Scatter is
deterministic under the same contract as erosion. Preview drapes and point
markers complete.

**T8 — Code view.** DSL grammar, parser, printer, two-way sync with the graph,
round-trip tests.

**T9 — Plugins.** The SDK header frozen at ABI version 1; dynamic loading
from the plugin directories with version check and clear refusal;
missing-plugin placeholder nodes; the example plugin built and exercised by
the test suite; a plugin authoring page in the docs.

**T10 — Build & polish.** Full-resolution Build with progress and
cancellation, export end-to-end, engine crash recovery, design-token pass
over the whole UI against §3.6.

Acceptance for the whole project: open `oetztal.massifdem`, build a
refinement graph (thermal + hydraulic erosion masked by slope), preview
interactively at 1024, build at 8192, and export the heightfield plus a
splat map (rock/scree/soil/snow weights) and a forest point set from a
`GrowthMask` → `Scatter` chain. The exported `.massifdem` loads in the game
with `-MassifDem=` — same file contract, no game-side changes.
