# Mountain node gauntlet

Goal: the Mountain node in `engine/src/nodes/landform_nodes.cpp` matches the default Gaea 2 Mountain node,
with the simple controls Gaea exposes (Height, Scale, Style presets Basic / Eroded / Old / Alpine / Strata).
Graph: `terrains/mountain_gauntlet.massifgraph`.

Bar: `terrains/reference/gaea_mountain_default.png` (user's viewport render of the default node),
`terrains/reference/gaea_mountain_docs.png` (docs page render), `terrains/reference/gaea_mountain_styles.png`
(docs visual-direction image). Docs: https://docs.gaea.app/reference/nodes/terrain/mountain.html

Gaea parameter set (from docs): Scale, Height, Style {Basic, Eroded, Old, Alpine, Strata}, Bulk {Low, Medium, High},
Reduce Details, Seed, X Offset, Y Offset.

Measured every round: ridge-to-valley relief in metres vs the Height parameter; determinism hash across thread counts.

## Pieces

1. Parameter surface + massif silhouette (Height, Scale, Style, Bulk, Reduce Details, Seed, offsets; overall mass reads like the Gaea default)
2. Radial ridge network (spurs and gullies radiating from the summit, as in the Gaea default)
3. Style presets, one at a time: Basic, Eroded, Old, Alpine, Strata

## Log

| Round | Piece | Builder change | Critic verdict (blind A/B) | Biggest gap |
|-------|-------|----------------|----------------------------|-------------|
| 1 | 1 | Gaea parameter surface on Mountain (Scale, Height, Style, Bulk, Reduce details, Seed, X/Y offset) and a new massif silhouette: mass profile `(1 - u^2)^exponent` so the summit is pointed and the foot is tangent to the plane, reach bent by a direction-only lobe field, warp cut to a non-folding share, detail released closer to the summit. `terrains/mountain_gauntlet.massifgraph` at 1024 / 10 km. | | |
