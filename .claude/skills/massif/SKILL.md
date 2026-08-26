---
name: massif
description: Author, inspect and build Terrain Studio graphs from the terminal with the `massif` command — writing terrain node graphs as `.massifgraph` JSON, checking them against the registry, and running full-resolution builds without the Electron app. Use whenever the task involves a `.massifgraph` file, terrain nodes, erosion or heightfield export in this repository.
---

# Building terrain with `massif`

`massif` drives the Terrain Studio C++ engine with no window in the way. It
reads and writes the same `.massifgraph` file the app opens, so a graph written
here appears in the editor and a graph edited there is read back unchanged.

## Running it

From the repository root:

```
bun cli/src/massif.ts <command>
```

`cd cli && bun link` puts a plain `massif` on the path if you would rather use
that. `massif help` prints every command.

The engine binary must exist. If a command cannot load it, build it with
`go-task bridge:build` (the task runner is `go-task` on this machine, not
`task`).

## Never guess a node type

**There is no list of node types in this file, and there must not be one.**
Node types are registered by the engine and by plugins, so the registry is the
only truth about what exists. Always look before writing a graph:

```
bun cli/src/massif.ts nodes              # every type: signature and parameters
bun cli/src/massif.ts describe <Type>    # one type: ports, units, ranges, defaults
bun cli/src/massif.ts registry           # the same thing as JSON
```

`nodes` prints each type as a signature:
`HydraulicErosion(In, Mask) -> Out, Wear, Deposit, Flow`. Inputs are inside the
parentheses, outputs after the arrow, and both are in port-index order — which is
what `to_port` and `from_port` mean in the file.

A type name that `nodes` does not list does not exist. Do not invent one, and do
not assume a node from another terrain tool has an equivalent here.

## Editing a graph

```
bun cli/src/massif.ts print <graph>                 # the graph as JSON
bun cli/src/massif.ts apply <graph> --from <file>   # replace it from a JSON file
... | bun cli/src/massif.ts apply <graph>           # or from stdin
bun cli/src/massif.ts validate <graph>              # types, edges and ports
```

There is no DSL. The JSON `apply` reads is the whole `.massifgraph` document,
exactly as `print` writes it — settings, node positions and preview included.
Nothing is carried over from the file being replaced, so what you write is what
the editor opens. `apply` refuses a document with an unknown node type or a bad
edge before it writes anything, and creates the file if it is not there yet.

To change one thing, `print` the graph, edit that JSON, and apply it back.
Settings also have their own command, which leaves the rest of the file alone:

```
bun cli/src/massif.ts set <graph> --resolution 1024 --seed 7 --world-size 20000
bun cli/src/massif.ts set <graph> --preview-node eroded
```

`print` and `set` read and write the file without the engine, so they work even
where the addon has not been built. `apply` and `validate` need it, because the
registry is what says whether the document makes sense.

## The file on disk

A `.massifgraph` is plain JSON, tab-indented, one object per file:

```json
{
	"version": 1,
	"settings": {
		"resolution": 1024,
		"world_size_meters": 20000,
		"seed": 7,
		"georeference": {
			"coordinate_system": "",
			"easting_min_meters": 0,
			"northing_max_meters": 0
		}
	},
	"preview_node_id": "eroded",
	"nodes": [
		{ "id": "dem", "type": "ImportDEM", "x": 0, "y": 0,
		  "parameters": { "maximum_altitude": 4000, "path": "valley.massifdem" } }
	],
	"edges": [
		{ "from_node_id": "dem", "from_port": 0, "to_node_id": "eroded", "to_port": 0 }
	]
}
```

What the format guarantees, and what it costs you:

- **Every key is snake_case, parameter names included.** The registry reports
  `maximumAltitude`; the file says `maximum_altitude`. The conversion is
  mechanical and its own inverse, so a parameter name with an underscore already
  in it would not survive the round trip.
- **Ports are indices, not names.** `from_port` and `to_port` are positions in
  the node's output and input lists — the order `massif describe` prints them.
- **The order is canonical.** Nodes sort by id, edges by destination then
  source, parameter keys alphabetically. Moving a node in the editor does not
  reorder the file, so a diff shows only what changed. A node the editor made
  carries every parameter, default or not, so a later change to a default cannot
  quietly alter a saved graph.
- **`version` is checked before anything else**, and only `1` loads. A file
  that fails validation says which field, not that the whole thing is wrong.
- **Every field is required, positions included.** Writing a graph from nothing
  means choosing `x` and `y` yourself; roughly 240 apart across and 200 down,
  left to right in the order the data flows, reads the way the editor lays out.
- **Parameters you leave out stay out.** The engine evaluates a missing one at
  its registry default, so a hand-written node only needs the parameters it
  actually changes — and the file keeps it that way until the editor rewrites
  the node.

## Building

```
bun cli/src/massif.ts build <graph> [--resolution 8192] [--node <id>] [--threads N]
```

A build evaluates every node in the `Output` category, one at a time, and it is
the only run allowed to write files — an export node writes nothing during a
preview. `--resolution` applies to that run only and does not change the file.
The build stops at the first failure rather than writing half a set.

## What to expect from the engine

- **Determinism is a contract.** The same graph, seed and resolution give
  bit-identical output, across runs and across thread counts. `--threads` can
  never change a result, only how long it takes.
- **Erosion at 512 is not erosion at 8192.** Iterating over a different cell
  count is a different solve. Author at a low resolution for speed, but judge a
  result at the resolution you will build at.
- **A generator's `seed` parameter is a variation dial**, mixed on top of the
  per-node seed the graph already derives. Changing it varies one node and
  nothing else; the graph's own seed in `set --seed` moves everything.
- Parameters that mean a place on the ground are in **metres**. Artistic
  controls work on the field's own range instead. `describe` gives the unit.

## The app is watching

If Terrain Studio has the same file open it notices the write. With nothing
unsaved it simply reloads. With unsaved edits it asks the person at the keyboard
whether to reload or overwrite, and changes nothing until they answer — so a
`massif apply` while somebody is mid-edit will not silently win.

## Working style

1. `nodes`, then `describe` the few types you intend to use — the port order in
   the signature is what the edges' port indices refer to.
2. `print` the graph if one already exists, and edit that JSON.
3. Write the whole document and `apply` it.
4. `validate`, then `build` at a low `--resolution` first.
5. Only then build at full resolution — an 8192 erosion is minutes, not seconds.
