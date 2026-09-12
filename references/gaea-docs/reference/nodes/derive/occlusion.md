Source: https://docs.gaea.app/reference/nodes/derive/occlusion.html

# Occlusion

Derive › Generative

Shortcode `ccl`

Occlusion behaves similarly to Ambient Occlusion, but favors sedimentary processes vs. lighting.

## Official Video

For a video breakdown of this node, see [the official Occlusion video](https://docs.gaea.app/videos/official/nodes/yt-occlusion.html).

---

The Occlusion node behaves similarly to Ambient Occlusion but favors sedimentary processes and rock crevices rather than lighting.

You can choose to focus on larger shapes such as gulleys and valleys over crevices by using a high power, or smaller crevices by using low values. This provides many opportunities to mask out areas for texturing separately.

In Gaea 1.3, the Invert and Strong options have been removed. Use the [Modifier Stack](https://docs.gaea.app/ui/interface/property-editor/modifier-stack.html) or an Fx node.

## When to Use It

Use Occlusion when you need a crevice-focused mask for recessed zones, sediment pockets, or darker material buildup.

## Properties

**Strength** (`Single`)  
Controls the intensity of the occlusion effect. Higher values produce deeper and more pronounced shadowing in crevices and low areas.

**Octaves** (`Int32`)  
Sets the number of detail layers used in the occlusion calculation. Increasing octaves adds finer detail and complexity to the occlusion shadows.
