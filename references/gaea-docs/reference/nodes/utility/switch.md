Source: https://docs.gaea.app/reference/nodes/utility/switch.html

# Switch

Utility › Routing

Shortcode `swt`

Switch chooses between two different heightfield inputs and relies on a boolean (true/false) input.

Route lets you choose between two different heightfield inputs. This is an automation node that relies on a boolean (true/false) input.

## When to Use It

Use Switch when you need to toggle between two inputs quickly with a simple true-or-false control.

## Properties

**Switch Route** (`Boolean`)  
Selects which input route is active. Use this to dynamically switch between different input sources or branches in your graph.

**Discard Alt in Final Build** (`Boolean`)  
If enabled, alternate (inactive) input routes are discarded during the final build, reducing memory usage and export size.
