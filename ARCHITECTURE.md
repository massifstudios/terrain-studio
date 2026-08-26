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
hash(typeName, typeVersion,
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

## 9. Pending decisions

Recorded here when they land, not before:

- DSL grammar (T8).
- Plugin dynamic loading; the SDK header freezes at ABI version 1 (T9).

## 10. Code style

`CLAUDE.md` is authoritative. Engine C++: `PascalCase` types, `camelCase`
functions and variables, `snake_case` file names. TypeScript/Svelte:
`camelCase` values, `PascalCase` types and components.
