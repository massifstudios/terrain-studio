---
name: gauntlet-loop
description: Turns a terrain goal into one short, paste-ready "gauntlet loop" prompt - a prompt that makes an agent set a concrete quality bar (a named terrain render or real DEM), split a graph or engine change into small judgeable pieces, run one builder and a separate harsh critic on each, compare the built export blind against the bar, and loop until it wins. Works for .massifgraph authoring, new engine nodes, erosion tuning, colouring and export work. Triggers on "/gauntlet-loop", "gauntlet loop", "gauntlet this", "make a gauntlet prompt", "loop until it beats X".
---

# Gauntlet Loop

The user gives a terrain goal. You give back ONE short prompt they can paste into a fresh agent session in this repository.

You are not doing the work. You are writing the prompt that makes another agent grind on a graph or an engine node until its built output beats a real reference.

## Flow

1. **Read the goal.** One line restatement in your head, not on screen. Decide whether it is a graph goal (a `.massifgraph` and its exports) or an engine goal (a node in `engine/src/nodes/`, then a graph that shows it).
2. **Set the bar.** If the user supplied a reference, use it. If not, offer **2 or 3 candidate bars**, one line each, and stop. Wait for their pick. Do not write the prompt yet.
3. **Write the prompt.** One block, paste-ready, no preamble, no headings inside it, no narration after it.
4. **Offer to run it.** One flat line under the prompt: "I can run this here." Not a question.

If they say run it, you become the lead agent and follow the prompt you just wrote.

## The bar is the whole trick

Everything else in a gauntlet loop is scaffolding. The loop only produces quality if the thing it compares against is real.

A bar has to pass three tests:

- **Named.** A specific thing, not a category. "The Copernicus GLO-30 tile over the Bernese Oberland" works. "Realistic alpine terrain" does not.
- **Fetchable.** The critic can actually get it - download the DEM tile, open the published render, clone the repo and run its test suite. If the agent cannot obtain it, it will hallucinate the comparison.
- **Comparable.** The bar and our export can sit side by side at the same world size and resolution and a judge can pick one. If you cannot imagine the A/B, it is not a bar.

Bars by goal type:

| Goal                             | Bar that works                                                                                               |
| -------------------------------- | ------------------------------------------------------------------------------------------------------------ |
| Whole graph (a range, a valley)  | A real DEM tile of a named massif at matching world size, rendered as a 16-bit heightmap and a hillshade      |
| Landform primitive node          | A real DEM of the landform it imitates (a named cirque, a named volcano), cropped to the same extent           |
| Erosion or weathering node       | A named open-source implementation's output on the same input, plus its published test or benchmark          |
| Colour, SatMap, texturing        | A named satellite tile (Sentinel-2 true colour over the same massif) at the same resolution                   |
| Export, format, DEM round-trip   | The owning tool's reference file (`../massif/Tools/massif_dem.py` output), judged on byte equality            |
| Performance                      | The erosion baseline in memory (seconds at 1024 and 2048) or a named engine's published timing                |

When you propose bars, prefer the hardest one the agent can genuinely reach. A bar that is too easy makes the loop exit on round one.

If the goal has a measurable half, name it alongside the reference. Terrain nearly always has one: slope histogram, hypsometric curve, drainage density, ridge-to-valley relief in metres, determinism hash across thread counts, build seconds at a resolution. Taste plus a number beats taste alone.

## What the critic judges

The critic judges **exported files**, never the app. `bun cli/src/massif.ts build <graph> --resolution N` writes the `ExportPNG16` heightmap and the `ExportColor` image the graph declares. Those files, next to the bar, are the A/B. The critic must not open Terrain Studio or take screenshots of it - `CLAUDE.md` forbids judging the app's own look, and a viewport screenshot is not a comparable artifact anyway.

Author at a low resolution for speed, judge at the resolution the user will build at. Erosion at 512 is a different solve from erosion at 2048, so a round is not won until the full-resolution export wins.

## Prompt template

Adapt the wording every time. Fill the brackets, keep it short, keep the last line.

```
Build [GOAL] in this repository, as [a .massifgraph under terrains/ | an engine node plus a graph that shows it].

The bar is [BAR]. Get the real thing first - [download the tile | clone the repo | run the reference tool] - and compare against it directly, not against a description of it. [Measure MEASURABLE HALF on both.]

Break this into the smallest pieces that can be improved and judged on their own from a built export. Spawn exactly one builder subagent on Opus 5 to do the work, one piece at a time. It authors with the massif command - nodes, describe, print, apply, validate, build - and obeys CLAUDE.md: terrain math in the engine, deterministic, tests green before every commit. For each piece, run a separate critic with fresh context on [CRITIC MODEL]. The critic builds the graph at [RESOLUTION], puts the exported heightmap and colour image next to the bar blind with the labels stripped, says which one is better, and names the single biggest remaining gap in terrain terms. Then it goes back to the builder.

The critic should be a harsh critic. Praise is not useful. If ours does not win, it keeps going.

/loop on each piece until the critic picks ours blind. Do not stop before that. Commit each round the critic advances.

Keep a progress file at terrains/[NAME].gauntlet.md updating as the work evolves so I can watch it.

Only one subagent works on the task at any time. Do not fan out.
```

Rules for what you fill in:

- The builder is always a single Opus 5 subagent. Never more than one builder.
- `[CRITIC MODEL]` is Opus 5 when the judgement is visual or geomorphological (does the ridge network read as alpine, is the erosion carving where water would run, does the colour sit like the satellite tile). Sonnet 5 when the measurable half dominates (byte equality, determinism hash, build seconds, a benchmark from the reference repo).
- `[RESOLUTION]` is the resolution the user will ship at. Default to the graph's own `settings.resolution` if they did not say.
- Bake the bar in as a concrete, fetchable thing. Tile id, dataset name, repo URL, file path.
- Add a build-time or resolution ceiling line **only if the user named one**. No default cap.
- Add tool names only if the goal needs them beyond `massif` (a DEM downloader, a Python plotting environment for the measurable half).
- Everything else stays out. No node chain, no parameter values, no erosion pass counts, no round count. The builder decides those after reading `massif nodes`, and it decides better than a spec written before the work started.

## Length and voice

Short. Around 150 to 200 words. If the prompt needs a heading to stay readable, it is too long.

Plain sentences. No bullet lists inside the prompt. It should read like someone telling an agent what a real mountain looks like and refusing to accept less.

## Portability

`/loop` is a Claude Code feature. It reruns the prompt on an interval or lets the model pace itself.

For any other agent, swap the `/loop` line for: "Keep looping until the critic picks ours." The single-builder line, the critic model line and the massif command carry over unchanged.

## Two filled examples

**Graph goal.** User: "the range graph should look like a real alpine massif, not a noise field."

Bars offered: A) Copernicus GLO-30 over the Bernese Oberland at 16 km B) swissALTI3D over the Mont Blanc massif at 16 km C) USGS 3DEP over the Teton Range at 16 km. User picks A.

```
Build terrains/gaea_style_range.massifgraph into a range that reads as a real alpine massif at 16 km world size.

The bar is the Copernicus GLO-30 tile over the Bernese Oberland, cropped to 16 km around the Jungfrau. Download it, resample it to 2048, export it as a 16-bit heightmap and a hillshade, and compare against those directly, not against a description of the Alps. Measure the slope histogram and ridge-to-valley relief in metres on both.

Break this into the smallest pieces that can be improved and judged on their own from a built export - the spine, the transverse ridges, the erosion, the valley floors, the colour. Spawn exactly one builder subagent on Opus 5 to do the work, one piece at a time. It authors with the massif command and obeys CLAUDE.md. For each piece, run a separate critic with fresh context on Opus 5. The critic builds the graph at 2048, puts the exported heightmap and colour image next to the tile blind with the labels stripped, says which one is the real mountain, and names the single biggest remaining gap in terrain terms. Then it goes back to the builder.

The critic should be a harsh critic. Praise is not useful. If ours does not win, it keeps going.

/loop on each piece until the critic picks ours blind. Do not stop before that. Commit each round the critic advances.

Keep a progress file at terrains/gaea_style_range.gauntlet.md updating as the work evolves so I can watch it.

Only one subagent works on the task at any time. Do not fan out.
```

**Engine goal.** User: "ThermalErosion should make proper scree slopes, right now it just blurs."

Bars offered: A) the Copernicus GLO-30 tile over a named scree field with its measured talus angle B) a named open-source terrain library's thermal erosion run on the same input plus its test suite C) both. User picks C.

```
Rework the ThermalErosion node in engine/src/nodes/ so it settles real scree, and add a graph under terrains/ that shows it on the existing Mountain primitive.

The bar is two things. The Copernicus GLO-30 tile over the Grosser Aletsch scree fields, cropped to 4 km, for what real talus looks like, and the named open-source library's thermal erosion run on our own Mountain export, for what a correct solver produces. Fetch both and compare against them directly. Measure the slope histogram on all three - real scree settles near its talus angle and the histogram should show it.

Break this into the smallest pieces that can be judged on their own from a built export - the talus angle rule, the material transport, the mask input, the determinism across thread counts. Spawn exactly one builder subagent on Opus 5 to do the work, one piece at a time. It keeps the node's registration and parameters stable, extends the Catch2 determinism test with every change, authors the graph with the massif command, and obeys CLAUDE.md. For each piece, run a separate critic with fresh context on Sonnet 5. The critic builds the graph at 1024, puts our heightmap next to the two references blind with the labels stripped, says which one is better, and names the single biggest remaining gap. Then it goes back to the builder.

The critic should be a harsh critic. Praise is not useful. If ours does not win, it keeps going.

/loop on each piece until the critic picks ours blind. Do not stop before that. Commit each round the critic advances.

Keep a progress file at terrains/thermal_erosion.gauntlet.md updating as the work evolves so I can watch it.

Only one subagent works on the task at any time. Do not fan out.
```

## What breaks a gauntlet loop

- **A vague bar.** "Looks like Gaea" or "realistic mountains" - the critic invents a comparison and approves everything. Most common failure by far.
- **Judging the app instead of the export.** A viewport screenshot has lighting and camera baked in and cannot sit next to a DEM. Only built files count.
- **Judging at the wrong resolution.** A win at 512 says nothing about 2048. The critic builds at the shipping resolution.
- **The builder judging its own work.** The critic must be a separate agent with fresh context. It should not know how hard the builder tried.
- **More than one builder.** Parallel builders fight over the same graph file and the critic cannot tell whose gap it is naming. One Opus 5 builder, sequential pieces.
- **A soft critic.** Say "harsh" in the prompt and give it a binary job: which one is the real mountain, A or B. Scores out of 10 drift upward every round.
- **Named exit after N rounds.** The exit is winning the comparison, or the user stopping the run. Never a round count.
- **Over-specifying.** Every parameter value in the prompt is one fewer decision the builder makes after reading `massif describe`. Minimal wins.
