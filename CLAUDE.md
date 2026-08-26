# Terrain Studio

Neoworks desktop terrain-generation tool (in the spirit of QuadSpinner Gaea).
General-purpose: it imports, refines, and exports heightfield terrain through
a node graph. The game Massif at `../massif/` is its first customer and the
source of the `.massifdem` format, not what the tool is built around.
Electron + Svelte 5 + TypeScript shell, Three.js viewport, C++20 engine
behind an N-API addon in a `utilityProcess`.

`PROMPT.md` holds the build brief; `ARCHITECTURE.md` (once it exists) holds
locked technical decisions. The invariants below override everything else in
the repo.

## Invariants

- **All terrain math lives in the C++ engine.** The UI never computes a
  height value. If a feature seems to need terrain math in TypeScript, the
  engine API is missing something — extend the engine instead.
- **Determinism.** Same graph + seed + resolution ⇒ bit-identical output,
  across runs and across thread counts. In engine code, never use wall-clock
  time, unseeded randomness, or convergence-driven iteration counts. Seeds
  derive per node from the graph seed and a stable node id; parallel scatter
  into shared cells uses integer atomics on a fixed-point encoding, never
  float atomics.
- **Keep the tool general.** Nothing Massif-specific leaks into the engine
  core, the graph model, or the UI — game-specific knowledge lives entirely
  in format readers/writers. A second game or a plain art pipeline must be
  servable without touching the core.
- **`.massifdem` is owned by the game.** Byte layout is defined by
  `../massif/Tools/massif_dem.py` and enforced by a round-trip test here.
  Never change the format unilaterally; if it seems to need a change, stop
  and raise it.
- **Never modify anything under `../massif/`.** It is context, not workspace.
- **Nodes are registered, not enumerated.** Adding a node is one C++
  registration; palette, property panel, code view and serializer pick it up
  from the registry. If adding a node requires a frontend edit, the registry is
  broken — fix that instead.
- **The registry is the plugin boundary.** Custom nodes load from shared
  libraries against a stable, versioned C ABI (the SDK header in
  `engine/sdk/`); built-ins register through the same path. Changing the
  SDK header is an ABI decision: additive only, version-bumped, never a
  silent signature change. Plugins are bound by the determinism contract.

## Code Style

Prioritize readability and maintainability over cleverness.

- TypeScript/Svelte: `camelCase` for variables, functions, and fields;
  `PascalCase` for types and components.
- C++: `PascalCase` for types, `camelCase` for functions and variables,
  `snake_case` file names. No Hungarian prefixes; this is not UE code.
- Preserve descriptive names. Do not shorten identifiers.
  - Good: `talusAngleDegrees`, `dropletCount`, `previewResolution`
  - Bad: `talus`, `nDrops`, `res`
- Prefer explicit control flow. Avoid ternaries, `??`, and compact
  conditionals; the only exception is when the explicit form would require
  duplicating a non-trivial expression — a preference for brevity alone is
  not an exception.
- Maximum 2 indentation levels inside a function body. Use guard clauses,
  early returns, or helper functions instead. Extracted helpers must be
  self-contained and meaningfully named — do not split a function into
  arbitrary fragments just to satisfy the indent limit.
- Keep functions short, with one clear responsibility each.
- Comments: only where the code is not readable when skimming. Comments must
  be technical and concise. Never write comments that restate the code, and
  never write historical comments — do not document old decisions, previous
  implementations, or what changed (no "previously...", "changed from...",
  "used to...").
- Do not change behavior, public APIs, data shapes, or side effects unless
  explicitly asked. If a required refactor (e.g. for the indent rule) would
  change behavior, stop and ask instead.
- Existing code is not authoritative just because it exists. If an existing
  pattern looks like a design mistake, do not copy it — ask, or verify
  against the framework's documented approach. The user is the authority.

This is a development project with no users. When editing code do not state
what something was changed from; git has the history.

## Style

Neoworks uses a shared design system defined in the `@neoworks-dev/ui`
package, located at
`/home/moritz/Documents/neoworks/neoworks.dev/packages/ui` and linked into
this project via `bun link`. The copy under `node_modules/@neoworks-dev/ui`
is the real package source, not a vendored dependency.

- Before creating a new component, check `@neoworks-dev/ui` for an existing
  one that fits. If you create a new one, follow the design system
  guidelines and briefly note which existing components you considered and
  why they didn't fit.
- Never edit anything under `node_modules/`. Changes to the design system
  happen in the ui package's own directory and repository, and only when
  explicitly asked.
- If `@neoworks-dev/ui` is missing or unresolved, restore it with
  `bun link @neoworks-dev/ui`. Do not install it from a registry, remove
  the dependency, or copy components into the app as a workaround. If
  linking fails, report it and stop.
- Use the design system's tokens for color, spacing, and type; no color
  literals in components.

Never judge how the app looks yourself — do not take or inspect screenshots.
When a change needs to be seen, say what you changed and ask the user to
look, naming what to look for.

## Tests

After completing all requested changes, and before committing:

1. Decide whether any change needs a new test. Behavior changes and bug fixes
   need one; pure refactors and comment/style edits do not. If you decide no
   new test is needed, say so in one sentence. Engine tests are Catch2 in
   `engine/tests/`; frontend logic tests use `bun test`, and cover pure
   logic only (serialization round-trip, graph validation, schema mapping) — no
   E2E or screenshot tests.
2. Run `task test` regardless of whether you wrote a new test. It must run
   both the engine suite and the frontend suite.
3. If tests fail, fix the code until they pass. Never delete, skip, weaken,
   or rewrite a test to make it pass unless the test itself is wrong — and if
   so, say so explicitly and explain why.

Determinism tests (output hash across runs and thread counts) accompany every
stochastic node as it lands, not later.

## Git

You may use git. Work in this order: change → test → commit → push.

- If the worktree is dirty before you start, commit the existing state
  separately with the message `wip: pre-existing changes`. Never mix
  pre-existing changes into your own commits.
- Commit at each logically complete step (a step that builds and passes
  tests), not one giant commit at the end.
- Commit messages: short, to-the-point title; add a body only when the
  change is not obvious from the title. If a change spans unrelated
  concerns, split it into multiple commits.
- Mention in the commit message that the commit was made by you (an AI)
  and not a human. Do not use a `Co-Authored-By` trailer.
- On a feature branch: commit only changes belonging to that branch's
  feature, then merge back into main. Merge only after verification
  passes — never merge red. Unrelated changes go on a separate branch.
- Push only after verification passes. Never force-push.
