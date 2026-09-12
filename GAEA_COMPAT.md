# Gaea Node Compatibility

Reference inventory of every node in QuadSpinner Gaea 2, mapped against what
Terrain Studio has today, plus a plan for closing the gap.

Source: the Gaea 2 documentation repository (`QuadSpinner/Gaea2-Docs`,
`source/reference/nodes/`), 183 node pages across 9 families. Titles and
one-line summaries below are condensed from that reference; property lists are
not published in the docs source (the app injects them), so parameter sets here
are inferred from behaviour and must be designed on their own merits.

## What this document is

A capability checklist. Gaea is the most complete terrain node set in
existence, so it is a good map of the problem space. It is not a specification
to clone.

Rules that hold while working through this list:

- **Capability parity, not name parity.** Where Gaea's name is descriptive
  (`Slope`, `Blur`, `Terraces`) we may land on the same name by convergence.
  Where it is branded or vague (`Wizard`, `Rugged`, `SuperColor`, `Shatter`)
  we name for what the node does. Never copy marketing names, preset lists, or
  palette data.
- **No engine-side game knowledge.** Gaea ships `Unity` and `Unreal` export
  nodes. Terrain Studio gets one general tiled exporter plus format writers;
  engine-specific conventions live in writers, same as `.massifdem`.
- **Determinism is non-negotiable.** Several Gaea nodes are explicitly
  convergence-driven or wall-clock bounded. Our equivalents take a fixed
  iteration count and a derived seed. Anything stochastic ships with its
  determinism test in the same change.
- **Registration only.** Every node below must land as one C++ registration
  against `engine/sdk/terrain_node_sdk.h`. If a node cannot be expressed
  through the SDK, that is an SDK gap — recorded in §4, fixed additively with
  a version bump, never a silent signature change.

## Legend

| Mark | Meaning |
| --- | --- |
| `have` | Terrain Studio has an equivalent node covering the core behaviour |
| `partial` | Related node exists but the capability is materially narrower |
| `gap` | Not implemented; in scope |
| `host` | Not a node — needs evaluator, graph model, or app support first (§4) |
| `skip` | Deliberately out of scope; rationale given |

---

## 1. What Terrain Studio has today

56 registered node types, by the category each one registers under:

**Input** — `ImportDEM`, `Constant`, `Gradient`, `RadialGradient`, `Shape`
**Generate** — `Perlin`, `RidgedNoise`, `Voronoi`, `Pattern`, `Noise`
**Terrain** — `Mountain`, `MountainRange`
**Filter** — `Blur`, `Sharpen`, `Clamp`, `Terrace`, `Curve`, `Warp`, `Flip`,
`Repeat`, `Pixelate`, `Edge`, `Autolevel`, `Equalize`, `Match`, `SoftClip`
**Combine** — `Add`, `Subtract`, `Multiply`, `Min`, `Max`, `Lerp`, `Compare`
**Mask** — `HeightMask`, `SlopeMask`, `AspectMask`, `CurvatureMask`, `FlowMask`
**Erode** — `HydraulicErosion`, `ThermalErosion`, `Deposit`
**Vegetation** — `GrowthMask`, `Scatter`
**Texture** — `ColorRamp`, `MixColor`, `SplatWeights`, `Gamma`, `HSL`,
`RGBSplit`, `RGBMerge`, `ColorThreshold`
**Output** — `Export`, `ExportPNG16`, `ExportColor`, `ExportSplat`,
`ExportPoints`

Coverage against Gaea: **56 of 183 covered or partially covered (~31%)**. The
load-bearing machinery — noise, erosion, flow accumulation, masking, colour,
export — was already there; W1 added the breadth around it.

---

## 2. Node inventory

### 2.1 Terrain (14) — large-scale landform primitives

| Gaea node | What it does | Status | Notes |
| --- | --- | --- | --- |
| Mountain | General-purpose mountain primitive | have | `Mountain`: domain-warped radial mass with ridged crags |
| MountainRange | Full range, shaped for large-scale erosion | have | `MountainRange`: jittered summits on a bearing, joined by a saddled ridge |
| MountainSide | Single slope / mountainside form | gap | Cheap once Mountain exists |
| Ridge | Rugged ridgeline with fractures and shearing | partial | `RidgedNoise` covers the noise, not the landform |
| Rugged | Strong rocky surface preserving overall shape | gap | Detail overlay, not a base shape |
| Plates | Perlin core with flat plate formations between | gap | `Voronoi` plate mode is a starting point |
| Slump | Smooth hills with collapsed areas | gap | Reads as thermal collapse over low-frequency fBm |
| Uplift | Tectonic-style mass raising | gap | Broad-scale, low octave count |
| Canyon | Drainage-based river canyon | gap | Needs flow router + carve profile |
| Crater | Single crater shapes | gap | Analytic profile: rim, ejecta, floor |
| CraterField | Landscape of craters | gap | Crater + deterministic scatter |
| Volcano | Cone with central crater | gap | Analytic; shares Crater profile code |
| Island | Landmass from a drawn shape | gap | Needs distance field + shoreline falloff |
| DuneSea | Wind-shaped dune fields | gap | Directional; shares machinery with Sand |

### 2.2 Primitive (23) — base shapes, gradients, noise

| Gaea node | What it does | Status | Notes |
| --- | --- | --- | --- |
| Perlin | Geo-tuned Perlin fBm | have | |
| Noise | Single-pixel noise onto terrain | have | `Noise`; resolution-dependent by nature, and documented as such |
| MultiFractal | Multi-noise with wide variation | partial | `RidgedNoise` is one variant; want the family |
| Cellular | Cell-based patterns | partial | `Voronoi` covers cells/walls/plates |
| Cellular3D | 3D cellular noise | gap | Sample a 3D lattice at height-driven z |
| Voronoi | Geo-tuned Voronoi | have | |
| Gabor | Oriented band-limited pattern noise | gap | Deterministic kernel sum |
| DriftNoise | Overlapping "shelf" noise | gap | Domain-drifted fBm |
| LineNoise | Distortable line sets for layered ridges | gap | Geometry generator |
| DotNoise | Dotted / speckled noise | gap | Shares scatter machinery |
| Cracks | Large cracked patterns | gap | Voronoi edge distance + width falloff |
| Pattern | Repeatable geometric patterns | have | `Pattern`: checker, stripes, grid, bricks, dots |
| WaveShine | Wave-like rippling patterns | gap | Analytic |
| Constant | Uniform value | have | |
| LinearGradient | Straight gradient | have | `Gradient` |
| RadialGradient | Circular gradient | have | `RadialGradient` |
| Cone | Conical gradient | have | `RadialGradient`, linear profile |
| Hemisphere | Hemispherical gradient | have | `RadialGradient`, dome profile |
| Shape | Basic geometric shapes | have | `Shape`: disc, square, ring, regular polygon |
| Draw | Hand-drawn mountain ranges | host | Needs a canvas parameter type (§4) |
| File | Load image/data as terrain | have | `ImportDEM` / `File` |
| Object | Load mesh/object data | host | Needs mesh input (§4) |
| TileInput | Read tile sets for tiled builds | host | Needs tiled build (§4) |

### 2.3 Simulate (25) — physical processes

| Gaea node | What it does | Status | Notes |
| --- | --- | --- | --- |
| Erosion | Core hydraulic erosion | have | `HydraulicErosion` |
| Erosion2 | Newer erosion algorithm | partial | Ours is droplet-based; a pipe/grid solver is the second algorithm |
| EasyErosion | Simplified erosion workflow | skip | Preset wrapper, not a distinct algorithm |
| Wizard / Wizard2 | Curated erosion wrappers | skip | Same — belongs in presets, not the registry |
| Thermal | Thermal erosion, talus and debris | have | `ThermalErosion` |
| Thermal2 | Evolved thermal erosion | partial | Multi-material talus is the delta |
| Crumble | Collapse driven by existing crevices | gap | Curvature-gated thermal variant |
| Scree | Loose rock accumulation on slopes | gap | Thermal deposit with size sorting |
| Debris | Loose debris across the surface | gap | Scatter + deposit |
| Sediments | Thick generic sediment layer | gap | Deposit-only pass |
| Dusting | Thin snow sprinkle | gap | Cheap variant of Snow |
| Snow | Snow fall, melt, adhesion, flow | gap | Needs slope + aspect + optional sun input |
| Snowfield | Broad settled snow coverage | gap | Shares Snow's solver |
| Glacier | Flowing glaciers on peaks | gap | Viscous flow; expensive |
| IceFloe | Flat ice plates inside water bodies | gap | Needs water mask from Lake/Sea |
| Rivers | River networks on any terrain | gap | Flow accumulation + channel carve + banks |
| Lake | Rainfall-driven physically accurate lakes | gap | Priority-flood depression filling |
| Sea | Water surface and coastal erosion | gap | Sea level + wave-driven coastal wear |
| Anastomosis | Interconnected downcutting channels | gap | Flow-network variant |
| HydroFix | Repairs broken flows | gap | Low-level; carve monotone descent |
| Lichtenberg | Branching fracture patterns | gap | Deterministic DLA / invasion percolation |
| Hillify | Smooths terrain into hill forms | gap | Multi-scale smoothing |
| Trees | Forests inside the heightfield | partial | `Vegetation` + `Scatter` cover the mask; canopy displacement is missing |
| Shrubs | Low vegetation growth | partial | Same |

### 2.4 Surface (21) — detail and strata

| Gaea node | What it does | Status | Notes |
| --- | --- | --- | --- |
| Terraces | Stepped terraces | have | `Terrace` |
| FractalTerraces | Fractal terracing / stratification | gap | Terrace with per-band jitter |
| Stratify | Non-linear broken rock layers | gap | Strata engine, highest value in family |
| Steps | Hard stepped breaks | gap | Quantiser |
| Sandstone | Layered sandstone terracing | gap | Strata + erosion gating |
| Shear | Rock shearing exposing strata | gap | Strata + lateral offset |
| Contours | Map-like contour bands | gap | Isoline extraction |
| Stones | Scattered stone forms | gap | Scatter + stamp |
| RockNoise | Rock fields with size variation | gap | Scatter + stamp |
| Rockscape | Broad rocky formations | gap | Layered noise |
| Outcrops | Rocky outcrops with coverage control | gap | Masked stamp field |
| Craggy | Craggy broken surface | gap | High-frequency ridged overlay |
| Shatter | Simple shapes into fractured terrain | gap | Voronoi fracture + displacement |
| Distress | Superficial wear and damage | gap | Masked high-frequency noise |
| Roughen | Fine surface breakup | gap | Small |
| GroundTexture | Superficial realism detail | gap | Small |
| Pockmarks | Scattered pits and dents | gap | Scatter + negative stamp |
| Bulbous | Inflates shapes volumetrically | gap | Morphological dilation with falloff |
| Bomber | Stamps an input across the surface | gap | Core stamping node — unlocks the family |
| Sand | Medium and small sand patterns | gap | Directional ripple |
| Grid | Regular grid pattern | have | `Pattern`, grid mode |

### 2.5 Modify (41) — filters and transforms

| Gaea node | What it does | Status | Notes |
| --- | --- | --- | --- |
| Blur | Softens the terrain | have | |
| VariableBlur | Blur amount varies by guide input | gap | Summed-area or mip pyramid |
| SlopeBlur | Directional blur along guide slopes | gap | High value; cheap once flow gradients exist |
| SlopeWarp | Directional warp along guide slopes | gap | Shares SlopeBlur code |
| Denoise | General noise removal | gap | Bilateral filter |
| Median | Median filter, edge preserving | gap | Fixed-radius histogram median |
| Sharpen | Enhances edges and small structures | have | `Sharpen`: unsharp mask over the existing blur |
| Aperture | Expands or compacts every feature | gap | Morphological open/close |
| Dilate | Dilate pixels | gap | Morphology |
| Deflate | Removes bulk, leaves fine detail | gap | High-pass |
| Shaper | Bulks terrain up or down | gap | Non-linear gain on low frequencies |
| ThermalShaper | Coalesces shapes, thermally consistent | gap | Thermal-gated Shaper |
| Recurve | Curvature-based expander | gap | |
| Curve | Adjusts the height curve | have | |
| Adjust | Modifier-stack in node form | partial | `Filter` overlaps; brightness/contrast/gamma stack |
| Clamp | Squash, clip, or extend heights | have | |
| Clip | Cuts against a threshold | have | `Clamp`; a separate node would duplicate it |
| SoftClip | Soft intensity reduction above a threshold | have | `SoftClip`: exponential shoulder, either side |
| Threshold | Eliminates values above/below | partial | `Clamp` + `HeightMask` |
| Extend | Stretches elevation beyond current limits | have | `Autolevel`'s explicit floor and ceiling |
| Autolevel | Auto-levels the terrain | have | `Autolevel`, with percentile ends |
| Equalize | Balances the histogram | have | `Equalize` |
| Match | Matches heights to a reference input | have | `Match` |
| GraphicEQ | Multi-band filter across scales | gap | Needs a Laplacian pyramid |
| Filter | Profile-based value reshaping | have | |
| Heal | Reconstructs low-res or 8-bit data | gap | Detail synthesis; hard to do well |
| BlobRemover | Removes small disconnected blobs | gap | Connected components + area threshold |
| Distance | Distance field from input features | gap | Exact EDT; unlocks several nodes |
| Warp | Organic warping | have | |
| DirectionalWarp | Warp driven by a guide input | gap | |
| Swirl | Single vortex warp | gap | Analytic |
| Whorl | Many vortex distortions | gap | Swirl × deterministic scatter |
| Fold | Slants, breaks, and folds | gap | Early-graph shape node |
| Origami | Diagonal multifold pattern | gap | Stylised |
| Transform | Move, rotate, scale | gap | Needs a sampling/edge policy decision |
| Transform3D | Full 3D transform including rotation | gap | Only meaningful with a mesh view |
| Transpose | Applies a reference terrain's character | gap | Frequency transplant |
| Flip | Horizontal / vertical flip | have | `Flip` |
| TriplanarDisplacement | Triplanar-projected displacement | gap | Needs normals |
| Pixelate | Blocky downsample | have | `Pixelate`, block size in ground units |
| Meshify | Faceted mesh-like surface | gap | Planar segmentation |

### 2.6 Derive (14) — data maps

| Gaea node | What it does | Status | Notes |
| --- | --- | --- | --- |
| Height | Elevation selection mask | have | `HeightMask` |
| Slope | Slope-angle selection mask | have | `SlopeMask` |
| Angle | Facing-direction selection mask | have | `AspectMask` |
| Curvature | Convexity mask | have | `CurvatureMask` |
| Peaks | Peak and ridgeline mask | partial | Inverse curvature at a chosen scale |
| FlowMap | Modern flow map | have | `FlowMask` |
| FlowMapClassic | Legacy flow behaviour | skip | Back-compat for Gaea only |
| Normals | Normal map | gap | Colour output; needed by several nodes |
| Occlusion | Sediment/crevice-favouring occlusion | gap | Multi-direction horizon scan |
| Soil | Soil settling mask | partial | `GrowthMask` overlaps |
| RockMap | Rock coverage mask | gap | Slope + curvature + noise composite |
| TextureBase | Material distribution base for colour | partial | `Texture` overlaps |
| Texturizer | Preset-driven texture masks | skip | Preset wrapper |
| ColorThreshold | Threshold on colour input | have | `ColorThreshold`, on luminance or one channel |

### 2.7 Colorize (13)

| Gaea node | What it does | Status | Notes |
| --- | --- | --- | --- |
| CLUTer | Build colour gradients for mapping | have | `ColorRamp`: stops plus enhance, reverse, bias and an HSL grade |
| SatMap | Satellite-style palettes | have | `SatMap`: strips built from public Sentinel-2 imagery by elevation, with clip, bias, enhance, roughness and grade |
| Tint | Two-colour blend across a range | have | `MixColor` / `ColorRamp` |
| Splat | RGBA channels into one heightfield | have | `SplatWeights` inverse |
| RGBSplit | Colour to three heightfields | have | `RGBSplit` |
| RGBMerge | Three heightfields to colour | have | `RGBMerge`; an absent channel reads as 0 |
| HSL | Hue/saturation/lightness adjust | have | `HSL` |
| Gamma | Gamma adjust | have | `Gamma` |
| Weathering | Brightens protrusions, darkens crevices | gap | Curvature-driven colour |
| ColorErosion | Erosion-style colour transport | gap | Reuses erosion transport on colour |
| WaterColor | Shallow/deep water tinting | gap | Needs a water depth map |
| SuperColor | Composite procedural colourisation | skip | Bundle of the above |
| Synth | Bitmap to CLUT | gap | Palette extraction from an image |

### 2.8 Output (12)

| Gaea node | What it does | Status | Notes |
| --- | --- | --- | --- |
| Export | Export terrain, masks, textures | have | `Export`, `ExportColor`, `ExportPNG16`, `ExportSplat` |
| PointCloud | Export as PLY / XYZ / CSV | have | `ExportPoints` |
| Unity | Unity-friendly export | skip | Belongs in a format writer, not a node |
| Unreal | Unreal-friendly export | skip | Same |
| Mesher | Save nodes as meshes with LODs | host | Needs mesh output (§4) |
| TextureBaker | Bake derived maps for export | gap | Batch wrapper over exporters |
| AO | Ambient occlusion map | gap | Shares Occlusion's horizon scan |
| Shade | Shaded relief render | gap | Hillshade |
| LightX | Full lit render, sun/shadow/ambient | gap | Shade + cast shadows |
| Sunlight | Sun exposure integral over a period | gap | Deterministic sun path; drives Snow and Vegetation |
| Cartography | Topographic map overlay | gap | Contours + colour |
| Halftone | Halftone-style render | gap | Stylised |

### 2.9 Utility (20)

| Gaea node | What it does | Status | Notes |
| --- | --- | --- | --- |
| Combine | Blend two inputs by operation | have | |
| Mask | Post-hoc masking of an effect | have | |
| Mixer | Layer-based colour mixing with masking | partial | `MixColor` is single-pair |
| Layers | Stacked blend operations | gap | Variadic input count |
| Compare | Diff two heightfields | have | `Compare`: unsigned, optionally normalised, reads as a mask |
| DataExtractor | Extract boundaries, differences, modifier maps | gap | Grab bag; split into real nodes |
| Edge | Fades terrain edges to nothing | have | `Edge`, to any value rather than only to zero |
| Seamless | Makes any input tileable | gap | Mirror/offset blend |
| Repeat | Tiles a terrain | have | `Repeat`, with mirroring so the seams close |
| Accumulator | Merges many masks without clutter | gap | Variadic max/add |
| Chokepoint | Reconnection helper, no memory cost | skip | Our evaluator caches by content hash |
| Gate | Forces ancestors to bake | skip | Same |
| Reseed | Re-randomises upstream seeds | host | Seeds derive from graph seed + node id by design |
| Switch | Boolean choice between two inputs | host | Needs scalar ports (§4) |
| Route | Integer-driven port routing | host | Same |
| Var | Read/write named graph variables | host | Needs graph variables (§4) |
| Math | Expression evaluation | host | Needs an expression evaluator (§4) |
| LoopBegin / LoopEnd | Loop circuits | host | Needs bounded loop support (§4) |
| MacroPort | Macro input/output ports | host | Needs macros (§4) |

---

## 3. Where the gap actually is

Of 183 Gaea nodes: **56 covered or partial**, **13 skipped by policy**,
**12 blocked on host features**, **102 implementable as registrations today**.

Of those 102, roughly 15 are still near-trivial and about 15 are genuinely hard
(Rivers, Lake, Glacier, Sea, Heal, Stratify, Erosion2, Snow, Lichtenberg,
Meshify). W1 took most of the cheap breadth; what is left is mostly machinery
first, nodes second.

---

## 4. Host and SDK gaps

These gate whole clusters and must be decided before the nodes that need them.
The SDK is a stable versioned C ABI: every change here is **additive with a
version bump**, never a signature change.

| # | Gap | Unblocks | Shape of the change |
| --- | --- | --- | --- |
| H1 | Scalar ports (int / bool / float as graph edges) | Switch, Route, driven parameters | New `TerrainPortType_Scalar`; evaluator carries a scalar result alongside fields |
| H2 | Graph variables | Var, parameter automation | `api->variableNumber(context, name)`; variables are part of the cache key |
| H3 | Expression evaluator | Math, expression-valued parameters | Pure, deterministic, no floating-point mode switches; lives in the engine |
| H4 | Bounded loops | LoopBegin/LoopEnd, Repeat count | Fixed trip count only — an unbounded loop breaks determinism |
| H5 | Macros / subgraphs | MacroPort, graph reuse | Graph model + serializer work, no SDK change if expanded before evaluation |
| H6 | Tiled builds | TileInput, large-area export | `api->tileIndex`/`tileCount`/`overlapCells`; nodes with global reach (flow, erosion) need an overlap contract |
| H7 | Mesh ports and writers | Mesher, Object, Transform3D | New `TerrainPortType_Mesh` + OBJ/glTF writers |
| H8 | Canvas / spline parameter type | Draw, Island | New parameter type plus a UI editor; the registry must carry it so no frontend node edit is needed |
| H9 | Variadic input ports | Layers, Accumulator, Mixer | Port descriptor gains a repeat count; evaluator and serializer follow |

H1–H3 are one coherent piece of work (values flowing through the graph) and
should land together. H6 is the largest single item and the one most likely to
force changes elsewhere — do it before the node count grows.

---

## 5. Shared machinery

Most of the 127 are cheap **once the right primitive exists**. Build these
first; they are engine-internal helpers, not nodes.

| # | Machinery | Nodes it unlocks |
| --- | --- | --- |
| M1 | Extended noise library — cellular 2D/3D, Gabor, drift, line, multifractal family | 10 primitives |
| M2 | Analytic shape rasteriser — cone, hemisphere, radial, disc, polygon, ring | 6 primitives — **landed**, `engine/src/nodes/shape_support.h` |
| M3 | Deterministic stamp compositor — instance list, per-instance seed, blend modes | Bomber, Stones, RockNoise, Pockmarks, CraterField, Outcrops, Debris, DotNoise, Whorl |
| M4 | Exact Euclidean distance transform (Felzenszwalb) | Distance, BlobRemover, Island, Sand, Scree, coastal falloff |
| M5 | Histogram toolkit — CDF, equalise, match, normalise | Autolevel, Equalize, Match, Adjust, Extend — **landed**, `terrain/histogram.h` |
| M6 | Morphology — dilate, erode, open, close with a shaped structuring element | Dilate, Deflate, Aperture, Bulbous, BlobRemover |
| M7 | Guided-warp sampler — offset field, bilinear fetch, edge policy | DirectionalWarp, SlopeWarp, SlopeBlur, Swirl, Transform, Seamless, Repeat, Flip |
| M8 | Laplacian / mip pyramid | GraphicEQ, VariableBlur, Deflate, Sharpen, Hillify |
| M9 | Strata engine — band table, per-band jitter, dip angle | Stratify, FractalTerraces, Sandstone, Shear, Steps, Contours |
| M10 | Depression handling — priority flood, fill and breach | Lake, Rivers, HydroFix, Anastomosis, Sea |
| M11 | Horizon scan (multi-azimuth ray march) | Occlusion, AO, Shade, LightX, Sunlight |
| M12 | Colour utilities — channel split/merge, HSL, gamma, palette sampling | 8 colorize nodes — **landed** bar palette sampling, `terrain/color_space.h` |

M3, M4, and M7 have the best unlock-per-effort ratio. M10 and M11 are the two
that need real algorithmic care.

---

## 6. Implementation plan

Waves are ordered so each one is shippable on its own and leaves the graph
strictly more useful. Each wave: build the machinery, register the nodes, land
the tests in the same change.

### W1 — Cheap breadth — **landed**

Machinery: **M2**, **M5**, **M12**.

Landed as 20 registrations covering the 25 Gaea entries, plus `Mountain` and
`MountainRange` pulled forward from W4. No new SDK surface; the palette,
property panel and command line picked all of it up from the registry with no
frontend edit.

Registered: `RadialGradient`, `Shape`, `Pattern`, `Noise`, `Flip`, `Repeat`,
`Pixelate`, `Edge`, `Autolevel`, `Equalize`, `Match`, `SoftClip`, `Sharpen`,
`Compare`, `Gamma`, `HSL`, `RGBSplit`, `RGBMerge`, `ColorThreshold`, and the
two landforms.

Five Gaea entries did not become nodes of their own, because each would have
duplicated one that already exists:

- **Cone** and **Hemisphere** are `RadialGradient` profiles. The three differ
  only in the falloff curve.
- **Grid** is a `Pattern` mode, alongside checkers, stripes, bricks and dots.
- **Extend** is `Autolevel` with its floor and ceiling set explicitly, which is
  what that node's target parameters are.
- **Clip** is `Clamp`, and **Threshold** is `Clamp` feeding `HeightMask`.
  Neither adds a capability.

`Mountain` and `MountainRange` both draw an analytic mass and then domain-warp
the ground it is read from — a circle stays a circle however much detail is
laid over it, so the silhouette has to be what breaks. Ridged crags fade in by
how high the shape already stands. A range derives each summit's jitter from
its own index rather than from a running generator, so changing the peak count
leaves the summits that were already there in place.

### W2 — Filter depth (≈12 nodes)

Machinery: **M1**, **M6**, **M7**, **M8**.

Nodes: Cellular, Cellular3D, MultiFractal family, Gabor, DriftNoise, Denoise,
Median, Dilate, Deflate, Aperture, VariableBlur, SlopeBlur, SlopeWarp,
DirectionalWarp, GraphicEQ, Swirl.

SlopeBlur and SlopeWarp are the two Gaea artists reach for constantly; they
are cheap once M7 exists. GraphicEQ needs M8 and is worth the pyramid on its
own.

### W3 — Surface detail (≈15 nodes)

Machinery: **M3**, **M9**.

Nodes: Bomber, Stones, RockNoise, Pockmarks, Outcrops, DotNoise, Roughen,
GroundTexture, Distress, Craggy, Rockscape, Stratify, FractalTerraces,
Sandstone, Steps, Shear, Contours.

Determinism note: M3 scatter must use a per-instance seed derived from
`(graphSeed, nodeId, instanceIndex)` and composite through integer atomics on
a fixed-point encoding — never float atomics, never a thread-order-dependent
accumulation.

### W4 — Landform primitives (≈12 nodes)

Machinery: **M4**, plus W3's stamping.

Nodes: MountainSide, Ridge, Rugged, Plates, Slump, Uplift, Crater, CraterField,
Volcano, Distance, BlobRemover, Bulbous, Fold, Origami, Shatter, Sand, DuneSea.

Mountain and MountainRange were the two highest-value nodes in the entire list
— they are what makes a blank graph productive — so they landed with W1 rather
than waiting for this wave. MountainSide is cheap on top of `Mountain`.

### W5 — Water (≈8 nodes)

Machinery: **M10**.

Nodes: Lake, Rivers, Sea, Anastomosis, HydroFix, Canyon, Island, IceFloe,
WaterColor.

The hardest wave. Priority flood must use a deterministic tie-break (compare
by cell index when heights are equal) or lake boundaries shift between runs.
Rivers needs a fixed channel-width model, not a convergence loop.

### W6 — Derived data and render (≈10 nodes)

Machinery: **M11**.

Nodes: Normals, Occlusion, AO, Shade, LightX, Sunlight, Peaks, RockMap, Soil
refinement, TextureBase refinement, Cartography, Halftone, Weathering,
ColorErosion, Synth, TextureBaker, Meshify.

Sunlight's sun path must come from explicit date/latitude parameters — never
from the system clock.

### W7 — Simulation depth (≈12 nodes)

Nodes: Erosion2 (grid/pipe solver alongside the droplet solver), Thermal2
multi-material talus, Crumble, Scree, Debris, Sediments, Snow, Snowfield,
Dusting, Glacier, Hillify, Lichtenberg, Trees canopy, Shrubs.

Each stochastic node ships with an output-hash test across runs and across
thread counts, in the same commit.

### W8 — Host features (12 nodes, gated on §4)

H1–H3 → Switch, Route, Var, Math.
H4 → LoopBegin, LoopEnd, Repeat count.
H5 → MacroPort.
H6 → TileInput, tiled export.
H7 → Mesher, Object, Transform3D.
H8 → Draw, Island shape input.
H9 → Layers, Accumulator, Mixer.

This wave can start earlier — H1–H3 are independent of every other wave and
unblock parameter automation across the whole palette. Consider pulling it to
run alongside W2.

### Deliberately not implemented

FlowMapClassic (Gaea back-compat), EasyErosion / Wizard / Wizard2 / Texturizer
/ SuperColor (preset wrappers — belong in a preset library, not the registry),
Chokepoint / Gate (our evaluator already caches by content hash), Unity /
Unreal (engine conventions belong in format writers), Reseed (seeds derive
from graph seed and node id by design; a reseed node would break that
contract).

---

## 7. Determinism checklist for every node here

Applies to each node as it lands, not as a later pass:

1. No wall-clock time, no unseeded RNG, no convergence-driven trip counts.
2. Seeds derive from `(graphSeed, stableNodeId)` and, for per-instance work,
   the instance index.
3. Parallel scatter into shared cells uses integer atomics over a fixed-point
   encoding.
4. Reductions (histograms, flow accumulation, priority queues) use a
   deterministic order or an order-independent formulation, including
   tie-breaks.
5. Stochastic nodes ship an output-hash test across runs and thread counts.
6. Resolution independence: parameters in ground units, not cells, wherever
   the feature has a physical size.

## 8. Nearest next step

W1 is done, along with Mountain and MountainRange; coverage stands at ~31%.

Next is W8's **H1–H3** — scalar ports, graph variables, an expression
evaluator. They are one coherent piece of work, they are independent of every
other wave, and they unblock parameter automation across the whole palette,
which is worth more than any further node count. W2 can run alongside it: M7,
the guided-warp sampler, is the single highest unlock-per-effort item left, and
`SlopeBlur` and `SlopeWarp` fall straight out of it.
