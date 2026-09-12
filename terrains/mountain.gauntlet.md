# Mountain node gauntlet

Goal: the Mountain node in `engine/src/nodes/landform_nodes.cpp` matches the default Gaea 2 Mountain node,
with the simple controls Gaea exposes (Height, Scale, Style presets Basic / Eroded / Old / Alpine / Strata).
Graph: `terrains/mountain_gauntlet.massifgraph`.

Target (from round 2): `terrains/reference/gaea_mountain_imposing.png` (user's Gaea 2 render, "Imposing" mountain).
Now (round 2): `terrains/mountain_gauntlet_preview.png` (hillshade of the built graph), `terrains/mountain_gauntlet.png` (16-bit heightmap, min-max normalised).
Earlier bar: `terrains/reference/gaea_mountain_default.png` (user's viewport render of the default node),
`terrains/reference/gaea_mountain_docs.png` (docs page render), `terrains/reference/gaea_mountain_styles.png`
(docs visual-direction image). Docs: https://docs.gaea.app/reference/nodes/terrain/mountain.html

Gaea parameter set (from docs): Scale, Height, Style {Basic, Eroded, Old, Alpine, Strata}, Bulk {Low, Medium, High},
Reduce Details, Seed, X Offset, Y Offset.

Measured every round: slope histogram (flat <2 deg, 2-5, 5-15, 15-30, 30-45, >45) over the tile; tangential/radial gradient ratio on the mountain; ridge-to-valley relief in metres vs the Height parameter; determinism hash across thread counts.

## Pieces

1. Parameter surface + massif silhouette (Height, Scale, Style, Bulk, Reduce Details, Seed, offsets; overall mass reads like the Gaea default)
2. Radial ridge network (spurs and gullies radiating from the summit, as in the Gaea default)
3. Style presets, one at a time: Basic, Eroded, Old, Alpine, Strata

## Log

| Round | Piece | Builder change | Critic verdict (blind A/B) | Biggest gap |
|-------|-------|----------------|----------------------------|-------------|
| 1 | 1 | Gaea parameter surface on Mountain (Scale, Height, Style, Bulk, Reduce details, Seed, X/Y offset) and a new massif silhouette: mass profile `(1 - u^2)^exponent` so the summit is pointed and the foot is tangent to the plane, reach bent by a direction-only lobe field, warp cut to a non-folding share, detail released closer to the summit. `terrains/mountain_gauntlet.massifgraph` at 1024 / 10 km. | GAEA | Mass hangs off one centre point: radially symmetric dome, zero gradient at apex, 3-lobe wobble in plan. Needs a crest skeleton (irregular branching ridge line with subsidiary highs, height as falloff from nearest ridge segment). Secondary: isotropic crinkle not spurs (tangential/radial diff 1.34x, want 3-5x), no footslope apron (1-5 deg band 0.49%), single mass scale, no shoulders. Flat 72.7%, on-mountain mean slope 27.5 deg. |
| 2 | 2 (structure, vs Imposing target) | Mass hung off a ridge tree (summit arête, 6-node main crest, two spur generations), ridge holds ground up by crest minus a two-scale fall so seams are V valleys, detail anchored to nearest ridge instead of a ring about the summit, graph reframed to fill the tile. Commit b1b9a26. | NO MATCH. Radial pleated starburst: 18-23 ridge crossings on a 3 km ring, 17-20 at 5 km (no coalescing, ~940 m periodic spacing), every crease a straight line from summit to frame edge. Footslope apron absent: 0 m on 3 of 4 edges, flat (<20 m) 0.89% of tile. Slopes uniformly steep: 15-30 32.3%, 30-45 40.5%, >45 16.2%, mean 32 deg, base band (0-200 m) mean 19 deg on 3.4% of area. Valleys at 4 km mean 182 m deep but too many and too narrow. Cliffs not localised to the summit. | Asymmetric, coalescing ridge/spur system (4-6 uneven major spurs off one dominant crest) leaving a real low-angle footslope apron before the frame. |

## Gaps (ranked, round 2 analysis against the Imposing target)

1. Structure. Ours is a centred, radially symmetric dome with a pinwheel of straight creases meeting at the apex. Target is a branching crest: one main ridge runs from the summit across the tile with subsidiary highs, spurs branch off it as a tree, and V-shaped valleys sit between spurs, deepening and widening downslope. Round 1 measured tangential/radial gradient ratio 1.34x (want 3-5x).
2. Surface detail. Ours is isotropic cellular crinkle at one scale everywhere ("whirls"). Target detail is aligned with the downslope flow direction: gullies, chutes, striated rock faces, nothing cell-shaped.
3. Extent. Ours: 73.9% of the tile flat (<2 deg), mountain foot well inside the tile. Target: the mountain fills the tile, foot reaches every edge, no plane visible.
4. Slope distribution. Ours: 15-30 deg 10.2%, 30-45 deg 7.6%, >45 deg 2.0%, 2-5 deg 1.5%. Target has steep rocky upper faces with cliff bands, mid slopes, and a broad gentle footslope with fans and a braided valley floor at the bottom edge.
5. Style presets (Basic, Eroded, Old, Alpine, Strata) produce no visible difference. Target's upper faces show strata banding.

## Next builder brief

Task: the Mountain node in `engine/src/nodes/landform_nodes.cpp`, exercised by `terrains/mountain_gauntlet.massifgraph`, judged at 1024 on a 10 km tile. Target render: `terrains/reference/gaea_mountain_imposing.png`. Read it with the Read tool before you start, and read the current build `terrains/mountain_gauntlet_preview.png` to see where you start from.

The one gap to close this round is the ridge network's symmetry and the missing footslope. Today the mountain reads as a radial pleated starburst: a ring 3 km from the summit crosses 18-23 ridges, a ring at 5 km crosses 17-20, so spurs never coalesce, they sit at a near-constant ~940 m spacing, and every crease is a straight line from the summit pixel to the frame edge. The target has one dominant summit crest along one axis with roughly 4-6 unevenly spaced, unevenly sized major spurs off it; ridgelines curve, vary in width and prominence, and fewer, larger valleys appear as you move outward because tributaries merge. The target also keeps a low-angle skirt of gentle ground between the massif's outline and the frame; today that apron measures 0 m on three of four tile edges, ground below 20 m covers 0.89% of the tile, and the 0-200 m base band averages 19 deg on 3.4% of the area where a toe slope reads at 5-15 deg. Slope bands today: <2 deg 0.7%, 2-5 1.1%, 5-15 9.2%, 15-30 32.3%, 30-45 40.5%, >45 16.2%, mean 32 deg; over half the tile is steeper than 30 deg and the target's vegetated lower flanks need far more moderate ground than that. Valleys at 4 km from the summit average 182 m below the neighbouring crest, which is plausible per valley; the problem is their count and spacing, not their depth.

Keep: the registration name `Mountain`; the current parameter surface (scale, height, style, bulk, reduceDetails, seed, centreEast, centreNorth, radius, baseAltitude, peakSharpness, irregularity, detailStrength, featureSize, spurElongation); the pointed summit and tangent foot; the ridge-tree mass model from round 2 where ground is held up by the nearest ridge and the seam between ridges is a V that deepens downslope; the detail anchored to the nearest ridge rather than a ring about the summit; the tests added in round 2 in `engine/tests/test_landform_nodes.cpp`. Style presets do not need to differ yet.

Rules: obey CLAUDE.md. All terrain math in the C++ engine. Deterministic: same graph, seed and resolution give bit-identical output across runs and thread counts; the determinism test must keep covering the node (Catch2 in `engine/tests/`). The engine and frontend suites must pass except for two known pre-existing failures: `engine/tests/test_fluvial.cpp` ("a stream cuts deeper the more ground drains into it"), which is unrelated to Mountain and must not be touched, and `engine/tests/test_landform_nodes.cpp` ("most of a range's flank is valley and the ridge lines are the exception"), which covers MountainRange, not Mountain; leave it failing unless your Mountain change happens to fix it, and never weaken it. `task` is go-task. Rebuild the addon before building the graph (see `Taskfile.yml`). Commit with a message that says the commit was made by an AI, no Co-Authored-By trailer. Do not push. Use `bun cli/src/massif.ts nodes` and `bun cli/src/massif.ts describe Mountain` to see the node registry; build with `bun cli/src/massif.ts build terrains/mountain_gauntlet.massifgraph --resolution 1024`, which writes `terrains/mountain_gauntlet.png` (16-bit heightmap) and `terrains/mountain_gauntlet_preview.png` (hillshade). Do not open the app or take screenshots. Do not edit anything under `terrains/reference/` or `terrains/mountain.gauntlet.md`.

Deliverable, as your final message: the paths of the two exported PNGs at 1024, the commit hash, and a five-line receipt of what changed in the node. No self-assessment of how it looks.
