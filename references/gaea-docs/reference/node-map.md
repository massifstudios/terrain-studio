Source: https://docs.gaea.app/reference/node-map.html

# Node Map

### Primitive

| Family | Node | Shortcode |
| --- | --- | --- |
| Assets | [File](https://docs.gaea.app/reference/nodes/primitive/file.html)   *File loads any supported image or data file as a terrain, and supports all major file formats. You can also transform the input.* | `f` |
|  | [Object](https://docs.gaea.app/reference/nodes/primitive/object.html)   *Loads external object or mesh data for use inside the graph.* | `o` |
|  | [TileInput](https://docs.gaea.app/reference/nodes/primitive/tileinput.html)   *Reads a set of tiles for 1:1 input into a tiled build, or combines it into a single input.* | `til` |
| Basic | [Constant](https://docs.gaea.app/reference/nodes/primitive/constant.html)   *Constant creates a blank terrain of the height specified, or a flat color output.* | `k` |
|  | [Cracks](https://docs.gaea.app/reference/nodes/primitive/cracks.html)   *Cracks primitive creates large, cracked patterns on a flat base.* | `ck` |
|  | [Draw](https://docs.gaea.app/reference/nodes/primitive/draw.html)   *Draw draws entire mountain ranges in the shape you choose.* | `vm` |
|  | [Gabor](https://docs.gaea.app/reference/nodes/primitive/gabor.html)   *Gabor noise is one of the lesser used noises, but provides a wonderful pattern-friendly noise that can be used to simulate various aspects of nature.* | `g` |
|  | [LineNoise](https://docs.gaea.app/reference/nodes/primitive/linenoise.html)   *LineNoise is a geometry generator that creates sets of lines which can be distorted and used with other noises and fractals to produce layered ridges.* | `ln` |
|  | [Noise](https://docs.gaea.app/reference/nodes/primitive/noise.html)   *Noise applies controlled single-pixel noise onto the terrain.* | `kn` |
|  | [Pattern](https://docs.gaea.app/reference/nodes/primitive/pattern.html)   *Generates repeatable geometric or decorative patterns for masking and stylized terrain work.* | `` |
|  | [Perlin](https://docs.gaea.app/reference/nodes/primitive/perlin.html)   *Perlin is the most basic and essential noise. In Gaea, we've made a "geo-variant" by modifying the base Perlin shape to fit the needs of terrain design from the beginning.* | `p` |
|  | [Shape](https://docs.gaea.app/reference/nodes/primitive/shape.html)   *Generates basic geometric terrain shapes for use as primitives or masks.* | `s` |
|  | [Voronoi](https://docs.gaea.app/reference/nodes/primitive/voronoi.html)   *Voronoi is a staple of procedural design and creates sharp, geometric patterns. In Gaea, we've made a "geo-variant" by modifying the base Voronoi shape to fit the needs of terrain design from the beginning.* | `v` |
| Extended | [Cellular](https://docs.gaea.app/reference/nodes/primitive/cellular.html)   *Cellular creates cell-based patterns and noises that can be quite versatile as they're ever-present in nature.* | `ll` |
|  | [Cellular3D](https://docs.gaea.app/reference/nodes/primitive/cellular3d.html)   *Generates three-dimensional cellular noise useful for volumetric-looking patterns and complex terrain breakup.* | `c3` |
|  | [DotNoise](https://docs.gaea.app/reference/nodes/primitive/dotnoise.html)   *Generates dotted or speckled noise patterns for texturing and breakup.* | `do` |
|  | [DriftNoise](https://docs.gaea.app/reference/nodes/primitive/driftnoise.html)   *DriftNoise is a unique primitive that can create overlapping "shelves". It is a versatile primitive noise that can be used in various scenarios from cliffs to crack production.* | `dr` |
|  | [MultiFractal](https://docs.gaea.app/reference/nodes/primitive/multifractal.html)   *MultiFractal creates a multi-noise that can have a large level of variations. Often useful for texturing.* | `mf` |
|  | [WaveShine](https://docs.gaea.app/reference/nodes/primitive/waveshine.html)   *Generates wave-like reflective or rippling patterns useful for stylized surfaces and water-like forms.* | `ws` |
| Gradient | [Cone](https://docs.gaea.app/reference/nodes/primitive/cone.html)   *Generates a simple conical gradient that is useful as a primitive shape or mask.* | `con` |
|  | [Hemisphere](https://docs.gaea.app/reference/nodes/primitive/hemisphere.html)   *Generates a hemispherical gradient useful for domes, mounds, and radial falloffs.* | `do` |
|  | [LinearGradient](https://docs.gaea.app/reference/nodes/primitive/lineargradient.html)   *Generates a straight linear gradient across the terrain.* | `lg` |
|  | [RadialGradient](https://docs.gaea.app/reference/nodes/primitive/radialgradient.html)   *Generates a circular gradient radiating outward from a center point.* | `rg` |

### Terrain

| Family | Node | Shortcode |
| --- | --- | --- |
| Landscape | [Canyon](https://docs.gaea.app/reference/nodes/terrain/canyon.html)   *Canyon is a simple, fast, and detailed canyon primitive. It creates a drainage based river canyon.* | `ny` |
|  | [CraterField](https://docs.gaea.app/reference/nodes/terrain/craterfield.html)   *Creates a landscape full of craters.* | `cf` |
|  | [DuneSea](https://docs.gaea.app/reference/nodes/terrain/dunesea.html)   *Creates broad fields of dune-like terrain suitable for deserts and wind-shaped landscapes.* | `du` |
|  | [MountainRange](https://docs.gaea.app/reference/nodes/terrain/mountainrange.html)   *Range primitive quickly sets up an entire mountain terrain with shapes highly optimized for eroding large-scale landscapes.* | `mr` |
|  | [Plates](https://docs.gaea.app/reference/nodes/terrain/plates.html)   *Plates primitive is based on a Perlin core with flat plate-like formations created in between larger Perlin-esque shapes.* | `pl` |
|  | [Ridge](https://docs.gaea.app/reference/nodes/terrain/ridge.html)   *Ridge creates a rugged ridgeline with multiple fractures and rock shearing.* | `ri` |
|  | [Rugged](https://docs.gaea.app/reference/nodes/terrain/rugged.html)   *Rugged is a very powerful primitive node that creates a strong, rocky surface for any terrain while preserving overall shape.* | `gg` |
|  | [Slump](https://docs.gaea.app/reference/nodes/terrain/slump.html)   *Slump is a simple primitive that creates relative smooth hills with slumped (collapsed) areas. It is very useful for creating large landscapes.* | `um` |
|  | [Uplift](https://docs.gaea.app/reference/nodes/terrain/uplift.html)   *Creates tectonic-style uplift by raising and shaping large terrain masses.* | `` |
| Primitive | [Crater](https://docs.gaea.app/reference/nodes/terrain/crater.html)   *Crater primitive creates different types of crater shapes.* | `cra` |
|  | [Island](https://docs.gaea.app/reference/nodes/terrain/island.html)   *Island draws basic shapes from which entire landmasses can be generated.* | `is` |
|  | [Mountain](https://docs.gaea.app/reference/nodes/terrain/mountain.html)   *Mountain is one of Gaea's most versatile primitives, using a modulated Voronoi pattern and distortions to create realistic mountain shapes, ready for further modification and erosion.* | `m` |
|  | [MountainSide](https://docs.gaea.app/reference/nodes/terrain/mountainside.html)   *Generates a single mountain slope or mountainside formation.* | `ms` |
|  | [Volcano](https://docs.gaea.app/reference/nodes/terrain/volcano.html)   *Generates a volcanic landform with a central cone and crater structure.* | `vo` |

### Modify

| Family | Node | Shortcode |
| --- | --- | --- |
| Adjust | [Adjust](https://docs.gaea.app/reference/nodes/modify/adjust.html)   *The Fx node makes that process easier by combining all the common adjustments inside a single node and allowing you to toggle them on and off.* | `fx` |
|  | [Autolevel](https://docs.gaea.app/reference/nodes/modify/autolevel.html)   *Automatically remaps the input to use the full available height or mask range.* | `lv` |
|  | [Clamp](https://docs.gaea.app/reference/nodes/modify/clamp.html)   *Clamp controls the height of the terrain. You can squash the terrain, clip it at certain heights, or make it taller using the Extend operation.* | `cl` |
|  | [Clip](https://docs.gaea.app/reference/nodes/modify/clip.html)   *Cuts the terrain against a threshold, removing values above or below a specified level.* | `ci` |
|  | [Denoise](https://docs.gaea.app/reference/nodes/modify/denoise.html)   *Denoise is a general purpose noise removal filter that systematically cleans up random noise, spikes, and extremely sharp edges.* | `dn` |
|  | [Equalize](https://docs.gaea.app/reference/nodes/modify/equalize.html)   *Redistributes the input values to balance the histogram and reveal more detail across the range.* | `eq` |
|  | [Extend](https://docs.gaea.app/reference/nodes/modify/extend.html)   *Expands the terrain range beyond its current limits, effectively stretching elevation values.* | `xt` |
|  | [Flip](https://docs.gaea.app/reference/nodes/modify/flip.html)   *Flip flips the terrain horizontally, vertically, or both.* | `fp` |
|  | [Match](https://docs.gaea.app/reference/nodes/modify/match.html)   *Matches the height range or histogram of one terrain to another.* | `` |
|  | [Sharpen](https://docs.gaea.app/reference/nodes/modify/sharpen.html)   *Sharpen enhances terrain edges and small structures by making them more prominent.* | `sx` |
|  | [Threshold](https://docs.gaea.app/reference/nodes/modify/threshold.html)   *Abs can modify the terrain or mask it by eliminating values higher or lower than the exact value specified.* | `th` |
|  | [Transpose](https://docs.gaea.app/reference/nodes/modify/transpose.html)   *Adapts the terrain to the substructures of a given reference terrain.* | `tx` |
| Blur | [Blur](https://docs.gaea.app/reference/nodes/modify/blur.html)   *Blur diffuses sharp shapes and softens the terrain.* | `b` |
|  | [Median](https://docs.gaea.app/reference/nodes/modify/median.html)   *Applies a median filter to remove spikes, speckles, and small noise while preserving edges better than blur.* | `md` |
|  | [SlopeBlur](https://docs.gaea.app/reference/nodes/modify/slopeblur.html)   *SlopeBlur is a versatile, low-level node that adds directional blurring based on the slopes of a Guide terrain or mask. It is one of our deceptively simple nodes that can create a wide array of shapes.* | `sb` |
|  | [VariableBlur](https://docs.gaea.app/reference/nodes/modify/variableblur.html)   *Blurs the input by varying the blur amount across the surface using local data or a guide.* | `vb` |
| Effect | [Origami](https://docs.gaea.app/reference/nodes/modify/origami.html)   *Origami creates a diagonal multifold pattern, and can produce a highly stylized terrain.* | `or` |
|  | [Pixelate](https://docs.gaea.app/reference/nodes/modify/pixelate.html)   *Converts the input into a blocky, pixelated version.* | `px` |
|  | [Swirl](https://docs.gaea.app/reference/nodes/modify/swirl.html)   *Swirl creates a vortex warp on the terrain. Very useful for adding a fluid look.* | `swi` |
|  | [Whorl](https://docs.gaea.app/reference/nodes/modify/whorl.html)   *Whorl applies multiple vortex-like distortions across the terrain to create large-scale warping. Very useful for creating massive landscapes without a patterning in the noise and for creating slightly surreal or alien landscapes.* | `wh` |
| Profile | [Curve](https://docs.gaea.app/reference/nodes/modify/curve.html)   *Curve adjusts the height curve of a terrain.* | `cu` |
|  | [Deflate](https://docs.gaea.app/reference/nodes/modify/deflate.html)   *Compresses and pulls terrain features inward, reducing bulk and prominence.* | `df` |
|  | [Filter](https://docs.gaea.app/reference/nodes/modify/filter.html)   *Applies profile-based filtering to reshape terrain values and refine feature response.* | `fl` |
|  | [Fold](https://docs.gaea.app/reference/nodes/modify/fold.html)   *Fold simulates terrain folding by introducing slants, breaks, and folds to the terrain. It significantly changes the terrain shape and should be used early in the graph.* | `fo` |
|  | [GraphicEQ](https://docs.gaea.app/reference/nodes/modify/graphiceq.html)   *Reshapes terrain using multiple adjustable bands, similar to an equalizer for height profiles.* | `gq` |
|  | [Recurve](https://docs.gaea.app/reference/nodes/modify/recurve.html)   *Recurve is a curvature-based expander that inflates and/or deflates the terrain to create stronger slopes and larger, softer formations from a given noise or terrain.* | `rc` |
|  | [Shaper](https://docs.gaea.app/reference/nodes/modify/shaper.html)   *Shaper bulks up or bulks down a terrain, while optionally retaining the finer details.* | `sp` |
|  | [SoftClip](https://docs.gaea.app/reference/nodes/modify/softclip.html)   *SoftClip a low-level utility that lets you softly reduce the intensity of shapes above a specified threshold. It can be comparable to masking a shape with a Height node and then clamping that portion.* | `sc` |
|  | [ThermalShaper](https://docs.gaea.app/reference/nodes/modify/thermalshaper.html)   *ThermalShaper is a fast, basic simulation of the concavity and convexity that can be achieved from thermal erosion. The effect can be exaggerated for artistic effect.* | `ts` |
| Shape | [Meshify](https://docs.gaea.app/reference/nodes/modify/meshify.html)   *Converts terrain into a faceted, mesh-like surface with controllable vertex density and detail sensitivity.* | `ms` |
| Transform | [Aperture](https://docs.gaea.app/reference/nodes/modify/aperture.html)   *Aperture expands or compacts every feature on the terrain, similar to a camera's bokeh.* | `ap` |
|  | [Dilate](https://docs.gaea.app/reference/nodes/modify/dilate.html)   *Dilate the pixels of the heightfield or mask.* | `di` |
|  | [Distance](https://docs.gaea.app/reference/nodes/modify/distance.html)   *Generates a distance field from the input, measuring how far each point is from the nearest boundary or feature.* | `rt` |
|  | [Transform](https://docs.gaea.app/reference/nodes/modify/transform.html)   *Transform moves, rotates, and scales any terrain. A post-process effect, it can be implemented at any point in the graph.* | `t` |
|  | [Transform3D](https://docs.gaea.app/reference/nodes/modify/transform3d.html)   *Applies full 3D transformation to the terrain, including rotation and spatial adjustment.* | `t3` |
| Utilities | [BlobRemover](https://docs.gaea.app/reference/nodes/modify/blobremover.html)   *BlobRemover can remove smaller fragments or blobs near larger shapes. This is very useful for cleanup.* | `blo` |
|  | [Heal](https://docs.gaea.app/reference/nodes/modify/heal.html)   *Heal reconstructs damaged, low res, or 8-bit data with 16-bit, high resolution fidelity. It cleans up quantized images with extreme banding of any shape, and yields naturalistic terrain output.* | `hl` |
| Warp | [DirectionalWarp](https://docs.gaea.app/reference/nodes/modify/directionalwarp.html)   *DWarp or Directional Warp uses a secondary input to warp the first input in a uniform direction.* | `dw` |
|  | [SlopeWarp](https://docs.gaea.app/reference/nodes/modify/slopewarp.html)   *SlopeBlur is a versatile, low-level node that adds directional warping based on the slopes of a Guide terrain or mask. It behaves similarly to SlopeBlur.* | `sw` |
|  | [Warp](https://docs.gaea.app/reference/nodes/modify/warp.html)   *Warp can take the terrain and "warp" the shape creating more believable shapes or alter simple shapes into a more complex formation. It can help breakup masks too.* | `w` |

### Surface

| Family | Node | Shortcode |
| --- | --- | --- |
| Cartography | [Contours](https://docs.gaea.app/reference/nodes/surface/contours.html)   *Creates map-like contours for a terrain. Useful for making maps or for other overlays.* | `cn` |
|  | [Grid](https://docs.gaea.app/reference/nodes/surface/grid.html)   *Generates a regular grid pattern useful for cartography, masking, or stylized surfaces.* | `gr` |
| Rocky | [Craggy](https://docs.gaea.app/reference/nodes/surface/craggy.html)   *Turns a terrain's surface into a craggy, rocky, broken landscape.* | `cg` |
|  | [Outcrops](https://docs.gaea.app/reference/nodes/surface/outcrops.html)   *The Outcrops LookDev node creates rocky outcrops on any surface. You can control coverage, breakage, and surface details. Several Outcrops nodes can be layered for complex results.* | `oc` |
|  | [RockNoise](https://docs.gaea.app/reference/nodes/surface/rocknoise.html)   *RockNoise is a useful tool for generating fields of rocks with size variations and controllable density.* | `rn` |
|  | [Rockscape](https://docs.gaea.app/reference/nodes/surface/rockscape.html)   *Creates broad rocky surface formations and broken stone-like terrain detail.* | `rs` |
| Scatter | [Bomber](https://docs.gaea.app/reference/nodes/surface/bomber.html)   *Bomber takes an input heightfield and "stamps" or bombs it across the entire terrain surface using the randomization properties you set.* | `bo` |
|  | [Pockmarks](https://docs.gaea.app/reference/nodes/surface/pockmarks.html)   *Creates scattered pits, dents, and small impact-like depressions on the surface.* | `pck` |
|  | [Stones](https://docs.gaea.app/reference/nodes/surface/stones.html)   *Scatters stones or small rock forms across the surface.* | `on` |
| Structure | [Bulbous](https://docs.gaea.app/reference/nodes/surface/bulbous.html)   *Bulbous inflates existing shapes to make them more voluminous while respecting standard geological boundaries.* | `bu` |
|  | [Shatter](https://docs.gaea.app/reference/nodes/surface/shatter.html)   *Shatter transforms simple shapes into complex, highly eroded, and fractured terrains.* | `tt` |
|  | [Shear](https://docs.gaea.app/reference/nodes/surface/shear.html)   *Shear creates rock shearing and exposes strata.* | `sh` |
| Superficial | [Distress](https://docs.gaea.app/reference/nodes/surface/distress.html)   *Adds superficial wear, breakup, and irregular damage to a surface.* | `dst` |
|  | [GroundTexture](https://docs.gaea.app/reference/nodes/surface/groundtexture.html)   *GroundTexture adds superficial detail to the terrain to increase realism.* | `gt` |
|  | [Roughen](https://docs.gaea.app/reference/nodes/surface/roughen.html)   *Adds fine roughness and small-scale breakup to the terrain surface.* | `rog` |
|  | [Sand](https://docs.gaea.app/reference/nodes/surface/sand.html)   *Sand creates medium and small level sand patterns. It can also be used to simulate dunes.* | `sd` |
| Terrace | [FractalTerraces](https://docs.gaea.app/reference/nodes/surface/fractalterraces.html)   *FractalTerraces creates fractal terracing on the terrain. It is a powerful alternative to the Terrace node.* | `ft` |
|  | [Sandstone](https://docs.gaea.app/reference/nodes/surface/sandstone.html)   *Creates layered sandstone-like terracing and sedimentary rock structure.* | `ss` |
|  | [Steps](https://docs.gaea.app/reference/nodes/surface/steps.html)   *Creates stepped formations and hard terrace-like breaks in the terrain.* | `ste` |
|  | [Stratify](https://docs.gaea.app/reference/nodes/surface/stratify.html)   *Stratify creates broken strata or rock layers on the terrain in a non-linear fashion, producing a robust, realistic formation.* | `ct` |
|  | [Terraces](https://docs.gaea.app/reference/nodes/surface/terraces.html)   *Creates stepped terrace formations from the terrain.* | `st` |

### Simulate

| Family | Node | Shortcode |
| --- | --- | --- |
| Erosion | [Anastomosis](https://docs.gaea.app/reference/nodes/simulate/anastomosis.html)   *Anastomosis creates interconnected water flow based extractions, ranging from small pits to large river channels. It is useful for both terrains and textures.* | `an` |
|  | [Crumble](https://docs.gaea.app/reference/nodes/simulate/crumble.html)   *Crumble runs a different kind of simulation that feeds off of existing crevices, erosive information, and other subtle details to collapse the terrain. Just as the name implies, the terrain starts crumbling from the edges.* | `cr` |
|  | [EasyErosion](https://docs.gaea.app/reference/nodes/simulate/easyerosion.html)   *Provides a simplified erosion workflow for quickly adding natural erosion effects.* | `ee` |
|  | [Erosion](https://docs.gaea.app/reference/nodes/simulate/erosion.html)   *Erosion is the cornerstone of all terrains. It simulates hydraulic erosion and turns any shape into a realistic-looking terrain.* | `ce` |
|  | [Erosion2](https://docs.gaea.app/reference/nodes/simulate/erosion2.html)   *An advanced hydraulic erosion node that creates more detailed and realistic erosion results.* | `e2` |
|  | [Hillify](https://docs.gaea.app/reference/nodes/simulate/hillify.html)   *Softens and reshapes terrain into smoother, hill-like forms.* | `hi` |
|  | [HydroFix](https://docs.gaea.app/reference/nodes/simulate/hydrofix.html)   *HydroFix alters the terrain to create continuous drainage network friendly slopes. It can be used before Erosion or other flow related nodes to get better results.* | `hf` |
|  | [Lichtenberg](https://docs.gaea.app/reference/nodes/simulate/lichtenberg.html)   *Creates branching, lightning-like erosion or fracture patterns.* | `lct` |
|  | [Sediments](https://docs.gaea.app/reference/nodes/simulate/sediments.html)   *Sediments creates a thick layer of generic sedimentation on top of the terrain, which can be used for sand or snow.* | `sed` |
|  | [Thermal](https://docs.gaea.app/reference/nodes/simulate/thermal.html)   *Thermal simulates Thermal Erosion to create talus and debris.* | `te` |
|  | [Thermal2](https://docs.gaea.app/reference/nodes/simulate/thermal2.html)   *Thermal2 is an evolution of the previous node. It creates Thermal Erosion to create realistic talus and debris.* | `t2` |
|  | [Wizard](https://docs.gaea.app/reference/nodes/simulate/wizard.html)   *The Erosion Wizard is a lightweight wrapper for the Erosion node. It simplifies the settings through curated presets, and provides secondary passes for additional processing.* | `z1` |
|  | [Wizard2](https://docs.gaea.app/reference/nodes/simulate/wizard2.html)   *A simplified erosion wrapper that provides curated controls for shaping erosion depth, scale, and strength.* | `z2` |
| Scatter | [Debris](https://docs.gaea.app/reference/nodes/simulate/debris.html)   *Simulates loose debris across the terrain surface to create broken rocky accumulation.* | `db` |
|  | [Scree](https://docs.gaea.app/reference/nodes/simulate/scree.html)   *Creates accumulations of loose rock fragments scattered across slopes and terrain edges.* | `` |
| Snow | [Dusting](https://docs.gaea.app/reference/nodes/simulate/dusting.html)   *Applies a thin dusting of snow or sediment over the terrain surface.* | `ds` |
|  | [Glacier](https://docs.gaea.app/reference/nodes/simulate/glacier.html)   *Glacier creates flowing glaciers on mountain tops.* | `gl` |
|  | [IceFloe](https://docs.gaea.app/reference/nodes/simulate/icefloe.html)   *Icefloe creates flat plates of ice inside a water body.* | `if` |
|  | [Snow](https://docs.gaea.app/reference/nodes/simulate/snow.html)   *Snow simulates snow fall, melt, and settling like in nature, and controls how the snow adheres and flows along the terrain.* | `sn` |
|  | [Snowfield](https://docs.gaea.app/reference/nodes/simulate/snowfield.html)   *Creates broad settled snow coverage across the terrain surface.* | `sf` |
| Vegetation | [Shrubs](https://docs.gaea.app/reference/nodes/simulate/shrubs.html)   *Adds low vegetation or shrub-like surface growth across the terrain.* | `shr` |
|  | [Trees](https://docs.gaea.app/reference/nodes/simulate/trees.html)   *The Arboreal Node gives you the power of creating superficial forests and vegetation. These forests are part of the heightfield while being "detached" enough for various export scenarios.* | `tr` |
| Water | [Lake](https://docs.gaea.app/reference/nodes/simulate/lake.html)   *Lake simulates rainfall on the terrain and creates physically accurate lakes.* | `` |
|  | [Rivers](https://docs.gaea.app/reference/nodes/simulate/rivers.html)   *Rivers creates a realistic network of rivers with controllable headwaters.* | `rv` |
|  | [Sea](https://docs.gaea.app/reference/nodes/simulate/sea.html)   *Sea is a multi-function water surface generator and coastal erosion tool. Its primary purpose is to create a water surface, while Coastal Erosion creates beaches and cliffs.* | `sea` |

### Derive

| Family | Node | Shortcode |
| --- | --- | --- |
| Aspect | [Angle](https://docs.gaea.app/reference/nodes/derive/angle.html)   *Angle creates a selection mask covering all areas of the heightfield facing the specified range.* | `ang` |
|  | [ColorThreshold](https://docs.gaea.app/reference/nodes/derive/colorthreshold.html)   *Creates a greyscale mask from a specified color range.* | `cth` |
|  | [Curvature](https://docs.gaea.app/reference/nodes/derive/curvature.html)   *Curvature creates a mask where convex areas (protrusions, sharp edges, etc.) are selected. Very useful layer in texture creation, often as highlights on top of an existing texture map.* | `cv` |
|  | [Height](https://docs.gaea.app/reference/nodes/derive/height.html)   *Height creates a selection mask within the height range specified.* | `h` |
|  | [Normals](https://docs.gaea.app/reference/nodes/derive/normals.html)   *Generates a normal map from the terrain for shading and downstream workflows.* | `nm` |
|  | [Peaks](https://docs.gaea.app/reference/nodes/derive/peaks.html)   *Creates a mask that isolates the peaks and highest protruding parts of the terrain.* | `pk` |
|  | [Slope](https://docs.gaea.app/reference/nodes/derive/slope.html)   *Slope creates a selection mask within the slope range specified.* | `sl` |
| Generative | [FlowMap](https://docs.gaea.app/reference/nodes/derive/flowmap.html)   *Generates a modern flow map showing how water or material would travel across the terrain.* | `fl` |
|  | [FlowMapClassic](https://docs.gaea.app/reference/nodes/derive/flowmapclassic.html)   *Generates a classic-style flow map based on legacy flow behavior.* | `fl1` |
|  | [Occlusion](https://docs.gaea.app/reference/nodes/derive/occlusion.html)   *Occlusion behaves similarly to Ambient Occlusion, but favors sedimentary processes vs. lighting.* | `ccl` |
|  | [RockMap](https://docs.gaea.app/reference/nodes/derive/rockmap.html)   *RockMap creates a rock-like coverage mask that can be used to texture terrains on its own or combined with other Data Maps.* | `km` |
| Texture | [Soil](https://docs.gaea.app/reference/nodes/derive/soil.html)   *Soil Map creates a soil mask, with increased density in crevices or other areas where the slope allows soil to settle.* | `so` |
|  | [TextureBase](https://docs.gaea.app/reference/nodes/derive/texturebase.html)   *Texture uses multiple factors (such as chaos, slopes, flow zones, and other geological features) to create a texture mask for any color node to create a texture map. Can be used by itself, or combined with other data maps and selectors.* | `tb` |
|  | [Texturizer](https://docs.gaea.app/reference/nodes/derive/texturizer.html)   *Generates terrain texture masks using preset styles and controllable secondary effects.* | `tz` |

### Colorize

| Family | Node | Shortcode |
| --- | --- | --- |
| Adjust | [Gamma](https://docs.gaea.app/reference/nodes/colorize/gamma.html)   *Gamma applies Gamma adjustment to the heightfield, working exactly like Gamma in an image processing program.* | `ga` |
|  | [HSL](https://docs.gaea.app/reference/nodes/colorize/hsl.html)   *Adjusts the hue, saturation, and lightness of a color input.* | `hu` |
| Color | [CLUTer](https://docs.gaea.app/reference/nodes/colorize/cluter.html)   *CLUTer creates color gradients which can be mapped to texture masks to make color maps.* | `clt` |
|  | [SatMap](https://docs.gaea.app/reference/nodes/colorize/satmap.html)   *Generates terrain color from preset satellite-style palettes with controllable range and processing.* | `sa` |
|  | [SuperColor](https://docs.gaea.app/reference/nodes/colorize/supercolor.html)   *Applies procedural terrain colorization using texture, flow, peaks, and palette controls.* | `sc` |
|  | [Synth](https://docs.gaea.app/reference/nodes/colorize/synth.html)   *Synth takes a bitmap image (photo or artwork) and converts it to CLUT map that can be used with a texture mask to colorize your terrain.* | `sy` |
|  | [Tint](https://docs.gaea.app/reference/nodes/colorize/tint.html)   *QuickColor quickly maps a 2-color gradient to any mask. Useful for simple scenarios where CLUTer would be overkill.* | `ti` |
|  | [WaterColor](https://docs.gaea.app/reference/nodes/colorize/watercolor.html)   *Colorizes water bodies using palette-based shallow and deep water tones.* | `wc` |
| Erosion | [ColorErosion](https://docs.gaea.app/reference/nodes/colorize/colorerosion.html)   *Applies erosion-style transport and deposition to color data, creating more natural weathered color variation.* | `cx` |
| Surface | [Weathering](https://docs.gaea.app/reference/nodes/colorize/weathering.html)   *Creates weathered color or mask variation that simulates dirt, fading, and environmental wear.* | `we` |
| Utilities | [RGBMerge](https://docs.gaea.app/reference/nodes/colorize/rgbmerge.html)   *Combines three heightfields into a single RGB color output by packing them into separate channels.* | `rm` |
|  | [RGBSplit](https://docs.gaea.app/reference/nodes/colorize/rgbsplit.html)   *RGBSplit takes a color node and splits the Red/Green/Blue channels into heightfields. The counterpart to RGBMix.* | `rsp` |
|  | [Splat](https://docs.gaea.app/reference/nodes/colorize/splat.html)   *Splat is similar to RGBMix, taking heightfield nodes and encoding them to the RGB channels of a color map, and providing properties to control the inputs.* | `spl` |

### Output

| Family | Node | Shortcode |
| --- | --- | --- |
| Assets | [Export](https://docs.gaea.app/reference/nodes/output/export.html)   *Exports terrain, masks, or textures to external files.* | `ex` |
|  | [Mesher](https://docs.gaea.app/reference/nodes/output/mesher.html)   *Mesher can save specific nodes as meshes instead of heightfields, and can also generate LODs.* | `me` |
|  | [PointCloud](https://docs.gaea.app/reference/nodes/output/pointcloud.html)   *Exports the terrain as a point cloud in formats such as PLY, XYZ, or CSV.* | `pc` |
|  | [Unity](https://docs.gaea.app/reference/nodes/output/unity.html)   *Exports terrain data in Unity-friendly resolutions and formats.* | `un` |
|  | [Unreal](https://docs.gaea.app/reference/nodes/output/unreal.html)   *Exports terrain data in Unreal-friendly resolutions, formats, and naming conventions.* | `ue` |
| Render | [AO](https://docs.gaea.app/reference/nodes/output/ao.html)   *Creates an ambient occlusion map that darkens recessed areas and enhances terrain depth.* | `ao` |
|  | [Cartography](https://docs.gaea.app/reference/nodes/output/cartography.html)   *Cartography creates a topographical overlay for a map.* | `crt` |
|  | [Halftone](https://docs.gaea.app/reference/nodes/output/halftone.html)   *Converts the terrain or mask into a halftone-style rendered output.* | `` |
|  | [LightX](https://docs.gaea.app/reference/nodes/output/lightx.html)   *Creates a fully lit render using controllable sun, shadow, and ambient lighting.* | `lx` |
|  | [Shade](https://docs.gaea.app/reference/nodes/output/shade.html)   *Produces a shaded terrain render based on lighting direction and surface relief.* | `` |
|  | [Sunlight](https://docs.gaea.app/reference/nodes/output/sunlight.html)   *Sunlight is a Data Map that gives you the sunlight integral (sun light/shadow map) of any given period (up to 365 days). It can be used to drive snow melt, vegetation growth, and more.* | `si` |
| Utilities | [TextureBaker](https://docs.gaea.app/reference/nodes/output/texturebaker.html)   *Bakes derived texture maps from the terrain for export and downstream use.* | `txb` |

### Utility

| Family | Node | Shortcode |
| --- | --- | --- |
| Routing | [Chokepoint](https://docs.gaea.app/reference/nodes/utility/chokepoint.html)   *Chokepoint node is a special node whose sole purpose is to help avoid re-connecting many nodes, while adding no memory overhead.* | `cp` |
|  | [Combine](https://docs.gaea.app/reference/nodes/utility/combine.html)   *Combine is one of Gaea's most important nodes. You can combine two terrains (or any node output) using various mathematical operations often known as Blend Modes.* | `c` |
|  | [Compare](https://docs.gaea.app/reference/nodes/utility/compare.html)   *Compares two inputs and produces a result based on their relative values.* | `cm` |
|  | [Gate](https://docs.gaea.app/reference/nodes/utility/gate.html)   *Defines a baking boundary for the graph by forcing all ancestor nodes to be baked.* | `g8` |
|  | [Layers](https://docs.gaea.app/reference/nodes/utility/layers.html)   *Combines multiple layers using blend operations in a stacked workflow.* | `` |
|  | [LoopBegin](https://docs.gaea.app/reference/nodes/utility/loopbegin.html)   *Begins a Loop circuit.* | `lb` |
|  | [LoopEnd](https://docs.gaea.app/reference/nodes/utility/loopend.html)   *Ends a Loop circuit.* | `le` |
|  | [MacroPort](https://docs.gaea.app/reference/nodes/utility/macroport.html)   *Defines an input or output port inside a macro for passing height, mask, or color data.* | `` |
|  | [Mixer](https://docs.gaea.app/reference/nodes/utility/mixer.html)   *Mixer allows layer-based manipulation of color textures and contains in-built masking.* | `mx` |
|  | [Reseed](https://docs.gaea.app/reference/nodes/utility/reseed.html)   *Re-randomizes an upstream procedural node by overriding or refreshing its seed behavior.* | `` |
|  | [Route](https://docs.gaea.app/reference/nodes/utility/route.html)   *Route chooses the flow of the graph based on the Input/Output ports chosen, and relies on integer input for automation.* | `ro` |
|  | [Switch](https://docs.gaea.app/reference/nodes/utility/switch.html)   *Switch chooses between two different heightfield inputs and relies on a boolean (true/false) input.* | `swt` |
| Structure | [Edge](https://docs.gaea.app/reference/nodes/utility/edge.html)   *Zero Borders softly transitions the edges of a terrain into nothing, similar to a vignette effect in paint applications. Useful when using Bomber or other effects where a tall edge would be problematic.* | `ed` |
|  | [Repeat](https://docs.gaea.app/reference/nodes/utility/repeat.html)   *Repeat takes a shape and repeats it in a tiled pattern. It is often combined with the Seamless node.* | `rp` |
|  | [Seamless](https://docs.gaea.app/reference/nodes/utility/seamless.html)   *Seamless can transform any input terrain or color map into a tileable/repeating pattern.* | `sls` |
| Utilities | [Accumulator](https://docs.gaea.app/reference/nodes/utility/accumulator.html)   *Accumulates specific map types such as Water, Forest, or Snow from all available nodes.* | `cc` |
|  | [DataExtractor](https://docs.gaea.app/reference/nodes/utility/dataextractor.html)   *Extracts specific derived data such as boundaries, differences, or Modifier maps from an input.* | `de` |
|  | [Mask](https://docs.gaea.app/reference/nodes/utility/mask.html)   *Mask draws a mask. If the Input port is connected to another node, a 2D representation of that terrain is shown in the background as a guide. This is optional, as the Input port only provides a drawing guide and does not affect the actual mask.* | `ma` |
|  | [Math](https://docs.gaea.app/reference/nodes/utility/math.html)   *Evaluates mathematical expressions to generate or modify terrain data procedurally.* | `mx` |
|  | [Var](https://docs.gaea.app/reference/nodes/utility/var.html)   *Reads or modifies named graph variables for procedural control and automation.* | `var` |
