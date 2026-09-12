Source: https://docs.gaea.app/reference/nodes/utility/var.html

# Var

Utility › Utilities

Shortcode `var`

Reads or modifies named graph variables for procedural control and automation.

## When to Use It

Use Var when the graph needs routing, layering, or control logic instead of a new terrain feature.

## Properties

**Edit Mode** (`Enum`)  
Specifies how the variable is used in the node. 'Consume' reads the value of an existing variable, while 'Modify' changes the value or applies an expression to the variable.

- **Consume** — Reads and uses the value of the specified variable without altering it.
- **Modify** — Applies an expression or new value to the variable, updating its value for subsequent nodes.

**Variable Name** (`String`)  
The name of the variable to consume or modify. This should match the variable defined elsewhere in the graph or macro.

**Expression** (`String`)  
An optional mathematical or logical expression to apply when modifying the variable. Used only in 'Modify' mode.

**Default Value** (`String`)  
The value to use if the specified variable is not set or available. Ensures the node has a fallback value.
