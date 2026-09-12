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

## Gaps (ranked, round 2 analysis against the Imposing target)

1. Structure. Ours is a centred, radially symmetric dome with a pinwheel of straight creases meeting at the apex. Target is a branching crest: one main ridge runs from the summit across the tile with subsidiary highs, spurs branch off it as a tree, and V-shaped valleys sit between spurs, deepening and widening downslope. Round 1 measured tangential/radial gradient ratio 1.34x (want 3-5x).
2. Surface detail. Ours is isotropic cellular crinkle at one scale everywhere ("whirls"). Target detail is aligned with the downslope flow direction: gullies, chutes, striated rock faces, nothing cell-shaped.
3. Extent. Ours: 73.9% of the tile flat (<2 deg), mountain foot well inside the tile. Target: the mountain fills the tile, foot reaches every edge, no plane visible.
4. Slope distribution. Ours: 15-30 deg 10.2%, 30-45 deg 7.6%, >45 deg 2.0%, 2-5 deg 1.5%. Target has steep rocky upper faces with cliff bands, mid slopes, and a broad gentle footslope with fans and a braided valley floor at the bottom edge.
5. Style presets (Basic, Eroded, Old, Alpine, Strata) produce no visible difference. Target's upper faces show strata banding.

## Next builder brief

Task: the Mountain node in `engine/src/nodes/landform_nodes.cpp`, exercised by `terrains/mountain_gauntlet.massifgraph`, judged at 1024 on a 10 km tile. Target render: `terrains/reference/gaea_mountain_imposing.png`. Read it with the Read tool before you start.

The one gap to close this round is the landform's structure. Today the Mountain reads as a centred, radially symmetric dome whose surface is isotropic cellular crinkle at one scale, with a pinwheel of straight creases meeting at the apex. The target reads as a branching crest: one main ridge running from the summit across the tile with subsidiary highs along it, spurs branching off that ridge as a tree, and V-shaped valleys between the spurs that deepen and widen downslope. Surface detail on the target follows the downslope direction (gullies, chutes) rather than forming cells. Measurements on the current build: tangential-to-radial gradient ratio 1.34x on the mountain (a spur-and-gully mountain sits at 3-5x); flat (<2 deg) 73.9% of the tile; 2-5 deg 1.5%; 15-30 deg 10.2%; 30-45 deg 7.6%; >45 deg 2.0%. The target's mountain fills the tile with its foot reaching every edge, so frame the graph the way the target is framed.

Keep: the registration name `Mountain`; the current parameter surface (scale, height, style, bulk, reduceDetails, seed, centreEast, centreNorth, radius, baseAltitude, peakSharpness, irregularity, detailStrength, featureSize, spurElongation); the pointed summit and tangent foot of the round 1 silhouette. Style presets do not need to differ yet.

Rules: obey CLAUDE.md. All terrain math in the C++ engine. Deterministic: same graph, seed and resolution give bit-identical output across runs and thread counts; a determinism test must cover the node. `task test` must be green. Rebuild the addon before building the graph (see `Taskfile.yml`). Commit with a message that says the commit was made by an AI, no Co-Authored-By trailer. Do not push. Use `bun cli/src/massif.ts nodes` and `bun cli/src/massif.ts describe Mountain` to see the node registry; build with `bun cli/src/massif.ts build terrains/mountain_gauntlet.massifgraph --resolution 1024`, which writes `terrains/mountain_gauntlet.png` (16-bit heightmap) and `terrains/mountain_gauntlet_preview.png` (hillshade). Do not open the app or take screenshots.

Deliverable: the two exported PNGs at 1024, the commit hash, and a five-line receipt of what changed in the node. No self-assessment of how it looks.
