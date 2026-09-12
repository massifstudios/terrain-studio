Source: https://docs.gaea.app/developers/extensibility/

# Extensibility

Extensibility in Gaea covers the tools that let you turn a graph or project into something reusable, controllable, or pipeline-friendly. Start here when you want to package graph logic, expose artist-facing controls, drive builds through variables, or evaluate future custom-node workflows.

For automation-only workflows, such as command-line builds and bridge execution, start with [Automation](https://docs.gaea.app/developers/automation/index.html) instead. Use this section when the structure of the graph itself needs to become more reusable or programmable.

## Choose a Path

| Goal | Start with |
| --- | --- |
| Package graph logic into reusable nodes | [Macros](https://docs.gaea.app/developers/extensibility/macros/index.html) |
| Expose controls that can drive one or more node properties | [Variables](https://docs.gaea.app/developers/extensibility/scripting-and-expressions/variables.html) |
| Transform or constrain incoming property values | [Expressions](https://docs.gaea.app/developers/extensibility/scripting-and-expressions/expressions.html) |
| Build procedural shapes or calculations inside the graph | [Math Node](https://docs.gaea.app/developers/extensibility/scripting-and-expressions/math-node.html) |
| Review available expression and Math node functions | [Allowed Functions](https://docs.gaea.app/developers/extensibility/scripting-and-expressions/functions.html) |
| Understand future custom-node and plugin development | [TOR Platform SDK](https://docs.gaea.app/developers/extensibility/sdk/index.html) |

## Macros

Use [Macros](https://docs.gaea.app/developers/extensibility/macros/index.html) when you want to turn a graph into a reusable node. A macro can expose selected parameters, provide optional inputs and outputs, and let teams share terrain logic without copying the full source graph into every project.

Macros are the best first step for reusable artist-facing tools because they work inside Gaea's existing graph workflow. Read [Building Macros](https://docs.gaea.app/developers/extensibility/macros/building-macros.html) for the authoring process, then [Best Practices](https://docs.gaea.app/developers/extensibility/macros/best-practices.html) before sharing macros with a team.

## Scripting and Expressions

Use [Overview](https://docs.gaea.app/developers/extensibility/scripting-and-expressions/index.html) when you need a project to respond to exposed values or computed logic.

- [Variables](https://docs.gaea.app/developers/extensibility/scripting-and-expressions/variables.html) create reusable values that can be bound to node properties and controlled through the UI or automation.
- [Expressions](https://docs.gaea.app/developers/extensibility/scripting-and-expressions/expressions.html) modify a bound value at the property level, which is useful for scaling, clamping, or deriving one control from another.
- [Math Node](https://docs.gaea.app/developers/extensibility/scripting-and-expressions/math-node.html) lets you write procedural calculations directly inside the graph.

These features are often the bridge between artist controls and automated builds. A project can expose variables for a pipeline, use expressions to keep controls stable, and use the Math node for custom procedural behavior.

## SDK

[TOR Platform SDK](https://docs.gaea.app/developers/extensibility/sdk/index.html) is the future-facing developer path for custom nodes and plugins on Gaea's TOR Platform. Treat it as planning context unless the page specifically marks a feature as currently available.

If you are building production workflows today, start with [Macros](https://docs.gaea.app/developers/extensibility/macros/index.html) and [Overview](https://docs.gaea.app/developers/extensibility/scripting-and-expressions/index.html) before moving into SDK material.

## In this section

## In This Section

- [Developers](https://docs.gaea.app/developers/)
- [Automation](https://docs.gaea.app/developers/automation/)
