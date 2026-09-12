Source: https://docs.gaea.app/reference/nodes/utility/macroport.html

# MacroPort

Utility › Routing

Shortcode

Defines an input or output port inside a macro for passing height, mask, or color data.

## When to Use It

Use MacroPort when the graph needs routing, layering, or control logic instead of a new terrain feature.

## Properties

**Direction** (`Enum`)  
Specifies whether the macro port is used for input or output connections. Determines the flow of data into or out of the macro node.

- **Input** — Designates the port as an input, allowing data to be received by the macro node.
- **Output** — Designates the port as an output, allowing data to be sent from the macro node.

**Display** (`Enum`)  
Defines the type of data visualized or represented by the macro port. This helps users identify the nature of the connection in the node graph.

- **Heightfield** — Represents terrain elevation data, typically used for heightmaps.
- **Mask** — Represents mask data, used for isolating or selecting specific regions in the terrain.
- **Color** — Represents color data, used for visual effects or texture information.

**Is Optional** (`Boolean`)

**Fallback** (`Enum`)  
When the given input fails or is intended to short-circuit, return the selected map.

- **Null** — Return a null value. May cause node to be disrupted, but needed for Switch and other mechanisms.
- **Black** — A black (0) value map.
- **White** — A white (1) value map.
- **Grey** — A grey (0.5) value map.
