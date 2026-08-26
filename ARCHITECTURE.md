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
  round-trip through the main process, and transferable `ArrayBuffer`s stay
  zero-copy.
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

### Side effects

`Export` and every other writer must check `isFinalBuild` before touching the
disk. Preview evaluation runs constantly while a parameter is being dragged.

## 6. Pending decisions

Recorded here when they land, not before:

- DSL grammar (T8).
- Plugin dynamic loading; the SDK header freezes at ABI version 1 (T9).

## 7. Code style

`CLAUDE.md` is authoritative. Engine C++: `PascalCase` types, `camelCase`
functions and variables, `snake_case` file names. TypeScript/Svelte:
`camelCase` values, `PascalCase` types and components.
