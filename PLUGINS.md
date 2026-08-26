# Writing a Terrain Studio plugin

A plugin is a shared library that adds node types. Everything downstream of
the registry — the palette, the property panel, the code view, the project
file, thumbnails, previews — picks a plugin's node up with no changes at all,
because none of them knows any node type by name. The engine's own nodes
register through exactly the same call, so if the built-ins work, plugins do.

## What you need

One header: `engine/sdk/terrain_node_sdk.h`. Nothing is linked, and there is
no library to find. Read the header — the determinism contract at the top of
it is binding, and the engine cannot enforce it for you.

## The two symbols

Every plugin exports exactly two functions, with C linkage:

```c
uint32_t terrainPluginAbiVersion(void);
int32_t  terrainRegisterPlugin(const TerrainRegistryApi* api, TerrainRegistry* registry);
```

Return `TERRAIN_SDK_ABI_VERSION` from the first. The engine reads it before
anything else and refuses the plugin outright, with a message naming both
version numbers, if it does not know that version. It never loads a plugin it
cannot vouch for and never skips one in silence.

From the second, describe your nodes and hand each one to
`api->registerNode`. Return 1 when you are willing to be loaded, 0 to refuse
yourself — a plugin that finds its own prerequisites missing should say so
here rather than register a node that cannot evaluate.

## A node

A node is a descriptor plus one function:

```c
void evaluate(const TerrainContextApi* api, TerrainEvaluationContext* context);
```

The context is a table of function pointers and is the whole of what a node
can reach. Through it you get the grid size and cell size in meters, the
graph's georeference, read-only views of the inputs, writable outputs, the
parameter values, a seed, progress reporting, a cancellation poll, and the
engine's parallel-for. There is no other engine internal in scope, by design.

Declare ports with a type — `Field`, `Color` or `Points` — and connections
type-check against it. A field port also carries a semantic tag, heightfield
or mask; the storage is identical either way and only the viewport reads it,
to decide whether to draw a surface or drape a mask over one.

Declare parameters with a type, a default, a range and a unit. Use meters
wherever the quantity is physical. Everything you declare appears in the
property panel with the right control, and in the code view under the name
you gave it — you write no UI.

## Determinism

The same graph, seed and resolution must produce bit-identical output, on
every run and at every thread count. This is not a quality goal; it is what
lets a preview at 512 and a build at 8192 be the same terrain, and what lets
a project be shared. The rules, in full:

- **Randomness comes only from the seed the context hands you**, through the
  generator inlined in the SDK header. Never `rand()`, never a thread id,
  never an address, never a clock.
- **Iteration counts are fixed.** Never iterate until a residual converges,
  and never budget work by elapsed time — both make the answer depend on the
  machine.
- **Parallel work goes through `api->runLanes`.** You choose the lane count;
  the engine chooses how many threads to spend on those lanes. A lane always
  covers the same work, so the thread count cannot reach the result. Write
  only into storage no other lane writes.
- **Where lanes must accumulate into shared cells**, use integer atomics on a
  fixed-point encoding. Float addition is not associative and float atomics
  reorder, so a sum of floats depends on the order the threads arrived in.
- **Carry no state between evaluations.** A node is a pure function of its
  inputs, its parameters and its seed.

The example plugin's `Speckle` shows all of this in about a hundred lines:
its per-cell value is mixed from the seed and the *cell index*, never the
lane index, so the rows may be divided any way at all.

## Building

The example is the template:

```
cmake -S plugins/example -B plugins/example/build
cmake --build plugins/example/build
```

The result is a `.so`, `.dylib` or `.dll` with no prefix. Point
`TERRAIN_SDK_DIR` at the SDK header if you build out of tree.

## Installing

Two directories are scanned at startup, and again whenever you press
**Plugins** in the top bar:

- your own: `<user data>/plugins`
- the project's: `plugins/` beside the application

Libraries sitting **directly** in one of those are loaded; subdirectories are
ignored, which is what lets a plugin's source live beside its built form. The
top bar reports how many loaded and, in red, anything that was refused and
why.

A rescan is additive. A library already loaded stays loaded and is not opened
again — the registry holds function pointers into it, and there is no moment
at which closing it would be safe. To replace a plugin, restart the app.

## When a plugin is missing

A project that names a node type nothing provides still opens. The node
becomes a placeholder: it keeps its settings and its connections, shows in
red as *not installed*, and refuses to evaluate. Saving the project writes
everything back untouched. Opening a project never costs you data.

The code view is stricter — it refuses to apply an edit while a placeholder
is in the graph, and says which type it could not find. Text is typed by
hand, so an unknown name there is far more likely to be a typo than a missing
plugin, and quietly turning one into a placeholder would hide it.

## The version rule

ABI version 1 is frozen. Nothing in the header moves: no field is reordered
or removed, no signature is altered, no enum value is reused. A later revision
may only append — new enum values, new function pointers at the end of
`TerrainContextApi` — and it arrives as version 2, which the engine will
accept alongside version 1. A plugin compiled against version 1 keeps working
without being rebuilt.

## Crashes

A plugin runs in the engine process, which is a `utilityProcess` separate from
the window. A plugin that crashes takes the engine down and not the UI; the
app reports it and restarts the engine. That containment is the only one there
is — the ABI cannot sandbox you, so test your node.
