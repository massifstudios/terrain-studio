---
name: massif
description: Author, inspect and build Terrain Studio graphs from the terminal with the `massif` command — writing terrain node graphs as DSL text, checking them, and running full-resolution builds without the Electron app. Use whenever the task involves a `.massifgraph` file, terrain nodes, erosion or heightfield export in this repository.
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

`nodes` prints each type as the call you would write:
`HydraulicErosion(In, Mask) -> Out, Wear, Deposit, Flow`. Inputs are inside the
parentheses in the order they must be given; outputs come after the arrow.

A type name that `nodes` does not list does not exist. Do not invent one, and do
not assume a node from another terrain tool has an equivalent here.

## The DSL

A graph is a list of statements, one node each:

```
program   = { statement }
statement = name "=" typeName "(" [ arguments ] ")"
arguments = argument { "," argument }
argument  = input | parameter
input     = name [ "." outputPort ] | "_"
parameter = name ":" value
value     = number | string | "true" | "false" | enumOptionName
```

```
dem    = ImportDEM(path: "valley.massifdem")
slope  = SlopeMask(dem, minimumSlope: 25, maximumSlope: 55)
eroded = HydraulicErosion(dem, slope, density: 2, capacity: 1.5)
out    = Export(eroded, path: "refined.massifdem")
```

Rules that bite if you forget them:

- **A node must be defined before it is named.** That is what makes a cycle
  impossible to write rather than merely rejected.
- **Inputs are positional and come first**, then named parameters. `_` leaves an
  input unconnected; a trailing run of `_` can be left off entirely.
- `name.PortName` picks a named output; a bare `name` means the first one.
- `#` runs to the end of the line. Newlines mean nothing, so a statement may
  span lines.
- Only parameters you write are set; the rest come from the registry's defaults.
  Printing a graph back leaves out everything still at its default.

## Editing a graph

```
bun cli/src/massif.ts print <graph>                 # the graph as DSL
bun cli/src/massif.ts apply <graph> --from <file>   # replace it from a DSL file
... | bun cli/src/massif.ts apply <graph>           # or from stdin
bun cli/src/massif.ts validate <graph>              # types, edges and ports
```

`apply` replaces the **whole** graph. To change one line, `print` it first, edit
the text, and apply the result — otherwise everything you left out disappears.

`apply` keeps the position of every node the file already had and lays out new
ones by dependency depth, so applying a graph does not shuffle the editor. It
also keeps the settings; those have their own command:

```
bun cli/src/massif.ts set <graph> --resolution 1024 --seed 7 --world-size 20000
bun cli/src/massif.ts set <graph> --preview-node eroded
```

A graph file that does not exist yet is created by the first `apply`.

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

1. `nodes`, then `describe` the few types you intend to use.
2. `print` the graph if one already exists.
3. Write the whole DSL and `apply` it.
4. `validate`, then `build` at a low `--resolution` first.
5. Only then build at full resolution — an 8192 erosion is minutes, not seconds.
