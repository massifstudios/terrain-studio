# Terrain Studio — Architecture

Locked technical decisions. Updated deliberately when a decision changes,
never as a side effect of unrelated work. `PROMPT.md` holds the build brief;
this file holds what has been decided and must not drift.

## 1. Repository layout

```
engine/           C++20 core: src/, include/, tests/, own CMake build
engine/sdk/       public plugin C header, the registration boundary
bridge/           N-API addon wrapping engine/, built with cmake-js
app/              Electron + Svelte 5: main/, preload/, engine/, renderer/
plugins/example/  example plugin: ABI regression test and author template (T9)
Taskfile.yml      dev, build, test, package
```

The engine builds and tests standalone — no Electron, no Node, no bridge.
`engine/tests` is its own CTest binary; the bridge configures the engine with
`TERRAIN_ENGINE_BUILD_TESTS=OFF`.

## 2. Process and bridge model

```
Electron main ── utilityProcess.fork ──► engine host (Node) ── dlopen ──► engine.node (N-API → C++)
      │                                        ▲
      └──► renderer (Svelte) ──── MessagePort ─┘
```

- The engine runs in a `utilityProcess`. A native crash kills that process
  only; the UI stays up and reports it.
- **The renderer talks to the engine host directly over a `MessagePort`.**
  Main mints a `MessageChannelMain`, hands `port1` to the engine host and
  `port2` to the renderer's `WebContents`. Result buffers therefore never
  round-trip through the main process.
- `MessagePortMain.postMessage` accepts only `MessagePortMain` values in its
  transfer list, so a `Float32Array` crosses as a structured clone: one
  binary copy per hop, never JSON. The addon likewise copies its result into
  a fresh `ArrayBuffer` rather than wrapping an external one, because
  Electron builds Node-API without external buffer support. Two copies of a
  10240² field is 800 MB of memcpy on a build, which is cheap next to the
  evaluation that produced it.
- `MessagePort` objects cannot cross `contextBridge`. The preload script
  re-posts the port into the page with `window.postMessage(..., [port])` on
  the `engine:port` channel; the page adopts it from `event.ports[0]`.
  Everything else the preload exposes (engine status, port request) goes
  through `contextBridge` as plain functions.
- Wire format is the envelope in `app/src/shared/engine-protocol.ts`:
  `{ requestId, command, payload }` in, `{ requestId, status, ... }` out.
  Heightfields travel as transferables *beside* the envelope, never inside
  it — no JSON for grid data, ever.
- Request/response correlation lives in `app/src/shared/request-ledger.ts`,
  deliberately transport-free so it is unit-testable and so a crashed engine
  can fail everything in flight instead of hanging it.

### Native addon and packaging

- The addon is built with `cmake-js` against Node-API version 8, the highest
  both current Node and current Electron implement. One binary serves the
  Node test runner and the Electron utilityProcess; no `--runtime=electron`
  rebuild.
- The main process resolves the addon path and passes it to the host in
  `TERRAIN_ENGINE_ADDON`. Unpackaged: `bridge/build/Release/engine.node`.
  Packaged: `process.resourcesPath/engine.node`, copied there by
  electron-builder `extraResources` so it sits outside the asar and can be
  `dlopen`ed.
- electron-vite emits ESM (`.mjs`) for main, preload, and the engine host;
  entry paths in `package.json` and in code carry that extension.

## 3. Determinism contract

Binding on the engine core and on every plugin. It is stated in full at
the top of `engine/sdk/terrain_node_sdk.h`, where a plugin author will read
it.

- Same graph + seed + resolution ⇒ **bit-identical output**, across runs and
  across thread counts.
- Per-node seeds derive from the graph seed combined with a stable node id,
  so adding a node never shifts another node's random sequence.
- Fixed iteration counts. Never a convergence test, never a time budget.
- No wall-clock time, no unseeded randomness, no iteration over unordered
  containers in the engine.
- Where parallel workers scatter into shared cells (droplet erosion,
  scatter), accumulation uses **integer atomics on a fixed-point encoding**,
  never float atomics.
- Erosion at 512 does not match erosion at 8192. That is accepted and
  documented, not fought: parameters are physical (meters) so results scale
  sensibly, but iteration over a different cell count is a different solve.
- Every stochastic node lands together with a determinism test — output hash
  across two runs, and at 1 thread vs N threads.

## 4. Format commitments

- **`.massifdem` is owned by the game Massif.** Byte layout is defined by
  `../massif/Tools/massif_dem.py`: `MASSIFDM` magic, version 1, 192-byte
  header, uint16 samples quantised over the window's own altitude band,
  row-major, row 0 north. Terrain Studio never changes it unilaterally; a
  round-trip test against a vendored fixture enforces compatibility, and the
  test does not depend on the game checkout.
- **What round-tripping guarantees.** Read-then-write reproduces the sample
  block byte for byte: every quantised level survives storage as float32.
  The header's `minimum` and `altitudeStep` do not, and cannot — they are
  recomputed from float32 samples, so they land within a float32 ULP of the
  float64 values the source writer stored. Decoded altitudes therefore agree
  to a ULP, not bit-exactly. Preserving the source band instead would lie
  the moment a graph changed the terrain, so it is recomputed on every write.
- The hillshade preview matches `massif_dem.py`'s `write_preview`: 315°
  azimuth, 45° altitude, `np.gradient` semantics (central differences
  inside, one-sided at the edges), the altitude ramp, and truncation rather
  than rounding to 8 bits. The fixture reference is generated from the
  *dequantised* heights, since that is what the engine actually holds.
- Format readers and writers are the **only** place project-specific
  knowledge may live. The graph, the nodes, and the UI see the heightfield
  type and nothing else.
- PNG support is a small in-tree codec (`formats/png_image`) over zlib:
  non-interlaced grayscale, RGB and RGBA at 8 or 16 bits. Palette and
  interlace are refused with a message rather than guessed at.

## 5. The graph engine

### Registry

Node types are registered, never enumerated. `engine/sdk/terrain_node_sdk.h`
is the C ABI: descriptor structs, an evaluation context as a function-pointer
table, and a registration call. **Built-ins go through exactly that call.**
The engine is its own first plugin client, so the ABI cannot rot unnoticed —
a change that would break a plugin breaks the built-ins first.

The registry copies everything a descriptor points at, so a plugin may free
its own storage the moment registration returns. `NodeRegistry::toJson()` is
what the palette, the property panel, the DSL and the serializer read; none
of them knows any node type by name.

### One resolution per graph

Every field in a graph is a square grid of `settings.resolution` cells, and
cell size is `worldSizeMeters / resolution`. `ImportDEM` resamples whatever
it reads onto that grid. This is what lets the context API stay small — a
node never asks how big its neighbours are — and it is why erosion at 512
does not match erosion at 8192.

### Cache key

```
hash(typeName, typeVersion, isFinalBuild,
     resolution, worldSizeMeters, seed, georeference,
     each parameter name and value in descriptor order,
     for each input port: the upstream node's key and output port index)
```

Hashing is FNV-1a in `terrain/hash.h`, not `std::hash`, whose results are
not stable across standard-library versions. `-0.0` is folded to `0.0` so the
two zeros cannot key different cache lines. Node version is in the key, so
bumping it retires every stale result for that type.

Dirty propagation falls out of this: editing a parameter changes that node's
key and therefore every downstream key, and nothing else. The evaluator
reports which nodes actually ran, which is what the cache tests assert on.

### Seed derivation

`deriveNodeSeed(graphSeed, nodeId)` mixes the graph seed with a stable hash of
the node id through splitmix64. Adding a node never shifts another node's
sequence. The same generator is defined inline in the SDK header so a plugin
and a built-in draw identical streams from identical seeds.

### Scheduling

Nodes are grouped into waves by dependency depth; a wave runs on as many
threads as asked for and the next wave starts when it drains. Every task
writes only into its own output buffers and its own result slot, so the
outcome cannot depend on how the wave was divided — that is what the
one-thread-versus-many determinism test pins down.

Per-node timings use a steady clock and are reported to the UI only. No
engine decision may read them; nothing about evaluation depends on how long
anything took.

A node that wants its own parallelism calls `runLanes` on the context API
with a **lane count it chooses itself**. The lane count is a property of the
node and the resolution, never of the machine: lane 3 of 64 always covers the
same rows. The engine decides only how many threads to spend on those lanes,
and gives a node the thread budget left over once the wave has been spread
across the pool. This is why the thread count cannot reach a result — it
never divides the work, it only consumes an already-fixed division.

### Side effects

`Export` and every other writer must check `isFinalBuild` before touching the
disk. Preview evaluation runs constantly while a parameter is being dragged.

## 6. The preview area

- **Shading is presentation, not terrain math.** The viewports take the grid
  the engine produced and shade it on the GPU; no height value is ever
  computed in TypeScript, and the cursor readout is a nearest-cell lookup into
  the engine's own samples, never an interpolation.
- Both views use the same hillshade the engine writes beside an exported DEM:
  sun at 315°/45° over the altitude ramp, plus a small ambient term so a slope
  facing away from the sun still reads as terrain. Preview and exported file
  therefore agree.
- The 3D view displaces a `PlaneGeometry` whose vertex order is exactly the
  engine's row-major order after rotating to the ground plane. World axes are
  +X east, +Y up, −Z north.
- The 2D view is a single quad carrying the field as an `R32F` data texture;
  hillshade comes from central differences in the fragment shader over the
  same cell spacing. It pans and zooms independently of the 3D camera.
- The cursor is shared as a **grid cell**, not a world position, so the two
  views mirror the same sample exactly.
- Renderer setup lives in `onMount`, never in `$effect`. An effect that
  happens to read anything it also writes re-creates the WebGL context on
  every change; pan and zoom are therefore kept outside the reactive graph and
  mirrored into a write-only `$state` object for the crosshair to read.
- Colours come from design tokens even inside WebGL: `readColorToken` resolves
  the CSS custom property for the canvas clear colour.

## 7. The node editor

- **The graph store is the only state.** `graph-store.svelte.ts` holds nodes,
  edges, settings and selection; the editor components render it and emit
  mutations, and none of them keeps graph state of its own.
- **The rules are pure functions.** Legal connections, cycle refusal, id
  allocation, deletion and parameter edits live in
  `app/src/shared/graph-operations.ts` — plain TypeScript over plain arrays,
  so they are unit-tested without a component tree. The store only makes them
  reactive.
- Nothing in the UI names a node type. The palette, the property panel and the
  connection type-check all read the registry JSON the engine reports.
- **Thumbnails are shaded by the engine**, not the UI: every evaluation
  returns one small RGBA image per node in the subgraph, produced from the
  cached result. The editor blits them. This keeps the "no terrain math in the
  UI" invariant honest even for a 64-pixel picture, and makes a node body look
  like the exported preview.
- Editing debounces into an evaluation at preview resolution. A newer request
  cancels the engine and drops any late answer by generation number, so
  dragging a slider never shows a stale frame.
- Perceptual ramp for masks (viridis, nine stops) is duplicated in
  `engine/src/thumbnail.cpp` and in the viewport GLSL. Change one, change the
  other; the comment in each says so.
- `.massifgraph` is written by the TypeScript side and sorted — nodes by id,
  edges by target, parameter keys alphabetically — so moving a node in the
  editor does not reorder the file. The engine never parses it: the bridge
  builds a `terrain::Graph` from the live object instead.

## 8. The node library

- **Generators sample in world meters**, from the graph's north-west corner —
  never from the georeference. Anchoring to the georeference would slide a
  whole landscape the moment an import window moved, which is not what a seed
  is for. Sampling in meters is also what makes a generator resolution
  independent: a higher resolution adds detail rather than a new landscape.
- **A generator's `seed` parameter is a variation dial**, mixed on top of the
  per-node seed the graph already derived. It varies one node and nothing
  else.
- **Filters normalise against the field's own range** where the control is
  artistic rather than physical — `Terrace` steps and `Curve` remaps both work
  on [0, 1] and are mapped back afterwards. Physical controls (`Clamp`,
  `HeightMask`, blur radius) stay in meters. The dividing line: if a number
  means a place on the ground, it is in meters.
- **`CurvatureMask` scales against the field's own strongest bend.** Curvature
  is in 1/m and its useful magnitude changes completely between a floodplain
  and an arête, so a fixed threshold would be a number nobody could guess.
- **The transfer curve is text**, `"x,y;x,y;..."`, and monotone cubic
  (Fritsch-Carlson) at both ends. Monotone rather than natural spline because
  an overshoot would invent altitude nobody placed. The interpolation is
  duplicated in `engine/src/curve.cpp` and
  `app/src/shared/transfer-curve.ts` — the engine applies it, the editor draws
  it, and the comment in each names the other.
- The SDK header grows during v1 by appending only: new enum values, new
  fields at the end of a struct. `TerrainParameterType_Curve` and `runLanes`
  arrived that way. It freezes at ABI version 1 in T9, and from then on a
  change means a new version.

## 9. Erosion

The hardest determinism problem in the tool, and the one the fixed-point rule
in §3 exists for.

- **Scattering goes through `FixedPointGrid`.** Thermal slumping writes into
  its neighbours and droplets land wherever they land, so many workers add to
  the same cell at once. Float addition is not associative; integer addition
  is. One unit is 10 µm, which is finer than a single droplet's bite and
  leaves 20 km of headroom in an `int32`.
- **Rain arrives in 32 batches.** Droplets inside a batch read a frozen
  surface and write only into the delta grid, which is what makes the run
  reproducible — none of them can see which thread went first. The batch
  count divides the *rainfall*, not the droplet count, so the feedback between
  batches is identical however finely the rain was sampled. That is what makes
  the droplet count a quality dial: doubling it halves each droplet's share of
  the rain and converges on the same terrain instead of eroding twice as much.
- **The droplet solver works in cell units, not meters.** One step sideways is
  one unit, so a height difference of one unit is 45°. Mixing the two would
  make every parameter depend on the ground resolution. The surface is scaled
  on the way in and the change is scaled back on the way out — and the change
  is read back as `deposit - wear` rather than from the working copy, so the
  absolute altitude never round-trips through the conversion and no rain at
  all returns the input untouched rather than merely close.
- **A droplet's footprint has a floor of three cells**, and it erodes and
  deposits over the same one. This is not a tuning constant. A droplet that
  takes from a disc but gives back to a single cell concentrates what erosion
  spread, and builds a tower in every hollow it reaches; and since the radius
  is asked for in meters, a coarse graph would otherwise request a fraction of
  a cell and get exactly that. Both failures are pinned by tests.
- **Droplet speed is capped.** Carrying capacity scales with speed, and an
  uncapped droplet accelerates for its whole life, so a single one could
  eventually drop a mountain's worth of sediment into one hollow.
- **`Deposit` moves only the sediment layer**, tracked separately from the
  rock. Slumping the rock as well would make it a second thermal erosion
  rather than a fill.
- **`FlowMask` is sequential by nature**: cells are visited highest first, and
  a cell passes on what reached it. Ties break on index so the ordering is a
  total order and not something the sort implementation decides. Water leaves
  towards every lower neighbour at once — a single-direction rule draws
  channels one cell wide along the grid axes.
- Erosion secondary outputs (`Wear`, `Deposit`, `Flow`) are masks normalised
  against their own strongest cell. The meters are already in the surface
  output; a mask is a weight.

## 10. Colour and points

- **A node output is a `NodeOutput`, not a heightfield.** Exactly one of
  `field`, `color` and `points` is set, and which one is decided by the port's
  declared type — never by inspecting the value. The evaluator allocates
  storage from the descriptor, so a node cannot produce a kind it did not
  declare.
- **Colour is three floats per cell and no gamma is applied anywhere.** A stop
  written `#808080` leaves the writer as 128. Guessing at a transfer function
  would make an exported map disagree with the swatch the artist picked, and
  there is no second place to undo the guess.
- **A point set is canonicalised by id after the node returns.** Ids are
  seed-derived and unique, so the order is the same however the node emitted
  them. This is what lets a scatter be parallel later without the ABI growing
  a footgun — emission order simply does not reach the result. Ids come from
  the background cell rather than a counter, so a rejected candidate does not
  renumber everything after it.
- **Points cross the bridge as one flat `Float32Array` of five values each,
  with the ids beside them in a `BigUint64Array`.** A forest is millions of
  instances, and a million small objects is a million allocations.
- **Colour and points are drawn over the nearest surface upstream**, the same
  rule masks already follow: none of the three has ground of its own, and
  reading one out of context tells you nothing. The colour drape replaces the
  altitude ramp *before* lighting, so the terrain keeps its relief and the map
  keeps its colour.
- Colour textures are expanded from RGB to RGBA in the renderer, not in the
  engine, where the fourth channel would be a plane of ones crossing the
  bridge for nothing.
- **The colour ramp and the transfer curve are both text**, and both are
  duplicated between the engine and the editor — `color_ramp.cpp` beside
  `app/src/shared/color-ramp.ts`, `curve.cpp` beside `transfer-curve.ts`. The
  engine applies them; the editor draws them. The comment in each names the
  other.
- **`isFinalBuild` is part of the cache key.** A build is a different
  evaluation from a preview even when the numbers match, because it is the one
  allowed to write files. Without it, building straight after a preview serves
  the exporters from cache and puts nothing on disk.

## 11. The DSL

The code view is the same graph as text. The grammar in full:

```
program   = { statement }
statement = name "=" typeName "(" [ arguments ] ")"
arguments = argument { "," argument }
argument  = input | parameter
input     = name [ "." outputPort ] | "_"
parameter = name ":" value
value     = number | string | "true" | "false" | enumOptionName
```

`#` runs to the end of the line. Whitespace, including newlines, is
insignificant — a statement may span lines, because `)` followed by
`name =` is unambiguous.

```
dem    = ImportDEM(path: "diffusion.massifdem")
slope  = SlopeMask(dem, minimumSlope: 25, maximumSlope: 55)
eroded = HydraulicErosion(dem, slope, density: 2, capacity: 1.5)
trees  = GrowthMask(eroded, eroded.Flow)
forest = Scatter(trees, eroded, spacing: 14)
out    = ExportPoints(forest, format: json)
```

- **The graph is the source of truth; the text is printed and parsed against
  the registry.** No type name, port name, parameter name or parameter type is
  written down in the DSL implementation, so a plugin's node reads and writes
  like a built-in with no change here.
- **A node must be defined before it is named**, which makes a cycle
  unwritable rather than merely rejected — there is no second cycle check in
  the parser. Printing is therefore topological.
- **Inputs are positional and come before named parameters**, and `_` leaves
  one unconnected. Naming inputs as well would put port names and parameter
  names in one namespace, where a collision would be silent. A trailing run of
  unconnected inputs is simply left off.
- **Only parameters that differ from the registry default are printed**, and
  parsing fills the rest back in. The text stays readable, and a changed
  default cannot silently alter a graph that was saved with the old one.
- **An unknown type name is refused, and the message asks about plugins.** The
  DSL is hand-written, so a typo must not quietly become a placeholder node; a
  project with a missing plugin still prints, but will not apply until the
  plugin is back.
- **The text is authoritative only while it has focus.** Every keystroke that
  parses is applied to the graph immediately; leaving the box prints the graph
  again, which is where the text is tidied. Reprinting under the cursor would
  fight the typing.
- **Node positions are not in the text.** A node the editor already knows keeps
  its position; new nodes are laid out by dependency depth into columns.

## 12. Plugins

`PLUGINS.md` is the author-facing page. What is locked:

- **ABI version 1 is frozen.** Nothing in `engine/sdk/terrain_node_sdk.h`
  moves: no field is reordered or removed, no signature altered, no enum value
  reused. A later revision may only append, and it arrives as version 2, which
  the engine will accept alongside version 1. A plugin compiled against
  version 1 keeps working without being rebuilt.
- **The engine reads a plugin's ABI version before anything else** and refuses
  outright any it does not know, with a message naming both numbers. Never a
  crash, never a silent skip. The message reaches the UI and the log.
- **A loaded library is never unloaded.** The registry holds function pointers
  into it for as long as the process lives, and there is no moment at which
  closing one would be safe. A rescan is therefore additive: it picks up
  libraries that have appeared and leaves everything already loaded alone.
  Replacing a plugin means restarting the engine process.
- **Only libraries directly in a plugin directory are loaded**, never ones in
  subdirectories, which is what lets a plugin's source live beside its built
  form.
- **The main process decides where the plugin directories are** — a per-user
  one and one beside the app — and passes them to the engine host in
  `TERRAIN_PLUGIN_PATH`. Neither the engine nor the host knows Electron's path
  conventions, and the renderer has no say in it at all.
- **Plugins register between evaluations**, under the same lock, so no node
  can appear in the registry while a graph is being walked.
- **A missing plugin becomes a placeholder node, never a lost one.** The node
  keeps its parameters and its edges, takes as many ports as its existing
  edges use, shows as *not installed*, and refuses to evaluate. Saving the
  project writes everything back untouched. The code view is deliberately
  stricter: it refuses an unknown type outright, because text is typed by hand
  and a typo must not quietly become a placeholder.
- **The example plugin is the ABI's regression test.** `plugins/example` is
  built by the engine's own test build, exactly as an author would build it —
  from the SDK header alone, in C, linking nothing — and the suite loads it,
  evaluates it, and checks it is deterministic across thread counts. A second
  throwaway plugin declaring a future ABI version pins the refusal path.

## 13. Build and recovery

- **A build is every node in the `Output` category, evaluated one at a time.**
  The category is the registry's own grouping, not a list of type names, so a
  plugin's writer is built exactly like a built-in one. Exporters share almost
  all of their upstream work, and the cache means the second one of a pair
  costs milliseconds — so there is nothing to gain from running them at once,
  and a clear reason not to: two full-resolution branches at 8192 would double
  the peak memory.
- **The build resolution replaces the preview one for that run only.** It is a
  session preference; the project file carries the graph's own resolution and
  nothing about how you last built it.
- **A build stops at the first failure** rather than writing half a set of
  files. Cancelling is the same path: the engine is told to cancel, the
  current node returns, and the loop ends.
- **Previews are paused for the duration.** A preview at 512 running beside a
  build would only evict the cached results the build is filling in. The last
  preview request is kept and runs when the build lets go.
- **Only a build writes files**, which the engine decides from `isFinalBuild`
  in the evaluation options — never the UI, and never a node deciding for
  itself.
- **A crashed engine is relaunched by the main process**, up to four times in
  a row; a fifth crash leaves the status at `unavailable` rather than looping
  forever, which is what a plugin faulting on load would otherwise do. A
  successful launch resets the count.
- **Every renderer that had a port is handed a fresh one when the new engine
  reports ready.** The old port died with the process. The renderer adopts the
  new one and asks the engine to describe itself again, which is how the
  registry and the plugin list come back. A window that never asked for a port
  is left alone.
- **Opening an elevation model sets the world size and the georeference**, and
  only then, because rereading the header on every evaluation would keep
  overwriting a world size the user had changed. A loaded project brings its
  own settings and does not count as newly opened.
- **The source node is recognised from its descriptor, not its name**: no
  inputs, and a `path` parameter. An exporter carries a path too, which is why
  having nothing feeding it is half the rule and not an afterthought. A
  plugin's own reader counts as a source with no change here.
- **In-flight requests fail rather than hang** when the engine goes: the
  request ledger rejects everything outstanding, and both the preview runner
  and the build runner treat a rejection as "nothing to draw" rather than
  letting it escape as an unhandled rejection.

## 14. Code style

`CLAUDE.md` is authoritative. Engine C++: `PascalCase` types, `camelCase`
functions and variables, `snake_case` file names. TypeScript/Svelte:
`camelCase` values, `PascalCase` types and components.
