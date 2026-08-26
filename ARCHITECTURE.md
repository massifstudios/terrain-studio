# Terrain Studio — Architecture

Locked technical decisions. Updated deliberately when a decision changes,
never as a side effect of unrelated work. `PROMPT.md` holds the build brief;
this file holds what has been decided and must not drift.

## 1. Repository layout

```
engine/           C++20 core: src/, include/, tests/, own CMake build
engine/sdk/       public plugin C header (lands at T9)
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

Binding on the engine core and on every plugin (stated in the SDK header
at T9).

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

## 5. Pending decisions

Recorded here when they land, not before:

- Node registry descriptors and the cache key (T2).
- DSL grammar (T8).
- Plugin C ABI, frozen at version 1 (T9).

## 6. Code style

`CLAUDE.md` is authoritative. Engine C++: `PascalCase` types, `camelCase`
functions and variables, `snake_case` file names. TypeScript/Svelte:
`camelCase` values, `PascalCase` types and components.
