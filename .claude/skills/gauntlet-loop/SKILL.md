---
name: gauntlet-loop
description: Runs a terrain gauntlet loop in this session. The user gives a terrain-generation task, a picture of what the output looks like now, and a reference of what it should look like. You analyse the gap, spawn one Opus 5 builder to close it, hand the built export and the reference to a fresh Sonnet 5 critic for a harsh verdict, write the next builder prompt from that verdict, and repeat until the critic says ours matches. State lives in terrains/<name>.gauntlet.md so the loop survives compaction; every two rounds it pauses for /compact. Terrain only - .massifgraph authoring, engine nodes, erosion, colour, export. Never UI. Triggers on "/gauntlet-loop", "gauntlet loop", "gauntlet this", "loop until it matches X".
---

# Gauntlet Loop

You are the lead. You do not write terrain code and you do not judge the result yourself. You analyse, brief the builder, brief the critic, turn the critic's verdict into the next brief, and keep the progress file current. The builder and the critic are separate subagents with fresh context every round.

## Inputs

Three things, all from the user:

1. **Task.** What to change. A graph under `terrains/`, a node in `engine/src/nodes/`, an erosion or colour pass, an export. Terrain generation only. If the task touches UI, Svelte, Electron, or the viewport, say the skill does not cover it and stop.
2. **Now.** What the output looks like today. An image path, or a graph to build and export. If a graph, build it yourself with `bun cli/src/massif.ts build <graph> --resolution N` and use the exported PNG.
3. **Target.** What it should look like. An image path, a DEM tile, a named render. Copy it under `terrains/reference/` and commit it before round zero so every critic sees the same file.

If any of the three is missing, ask for it in one line and stop. Do not guess a target. A vague target ("realistic", "like Gaea") makes the critic invent a comparison and approve everything.

Resolution: the one the user will ship at. Default to the graph's `settings.resolution`. Author low, judge at shipping resolution. Erosion at 512 is a different solve from erosion at 2048.

## Roles

| Role    | Model    | Context      | Sees                                                   | Does not see                              |
| ------- | -------- | ------------ | ------------------------------------------------------ | ----------------------------------------- |
| Lead    | you      | this session | everything                                             |                                           |
| Builder | Opus 5   | fresh        | task, gap brief, files, CLAUDE.md, massif command      | the critic's transcript, previous builders |
| Critic  | Sonnet 5 | fresh        | our export, the target, the measurement instructions   | the builder's notes, how hard anyone tried |

One builder at a time. Never fan out. Spawn with `Agent`, `model: opus` for the builder and `model: sonnet` for the critic, `subagent_type: general-purpose`. Never `subagent_type: fork`, for either role. A fork inherits this whole session, so the builder would see every critic verdict and the critic would see every builder receipt. Fresh context is the point. Everything a subagent needs goes into its brief, nothing else.

## Round zero: analysis

Read the now image and the target image with the `Read` tool. Write down, in terrain terms, everything that differs, ranked by how much it changes the read of the terrain. Terrain terms means ridge network, spur spacing, valley floor width, talus apron, relief in metres, slope distribution, drainage pattern, colour banding. Not "looks off".

Pick the first piece: the single biggest gap that can be built and judged on its own from an export. Then create the progress file and commit it.

Progress file at `terrains/<name>.gauntlet.md`:

```
# <name> gauntlet

Task: <one line>
Graph: terrains/<graph>.massifgraph, judged at <resolution>
Now: <path>   Target: <path>
Measured every round: <slope histogram | relief in metres | determinism hash | build seconds>

## Gaps (ranked, round zero)
1. ...
2. ...

## Log
| Round | Piece | Builder change | Critic verdict | Biggest remaining gap |
|-------|-------|----------------|----------------|-----------------------|

## Next builder brief
<the exact prompt for the next builder, verbatim>
```

The file is the loop's memory. After compaction it is the only thing you have, so every round writes the next builder brief into it verbatim before spawning.

## The round

### 1. Builder

Spawn one Opus 5 agent with the brief from the progress file. The brief contains:

- The task in one line and the graph or node file to touch.
- The one gap to close this round, in terrain terms, with the measurement if there is one. Nothing about how to implement it. The builder reads `massif nodes` and `massif describe` and decides better than a brief written before the work started.
- What to keep: registration name, parameter surface, anything a previous round already won.
- Rules: obey CLAUDE.md, terrain math in the engine, deterministic, `task test` green, commit with an AI-authored message, no push.
- Deliverable: the exported PNG at the judging resolution, the commit hash, and a five-line receipt of what changed. No self-assessment.

Wait for it. Confirm the export exists and the commit landed. Do not judge the picture.

### 2. Critic

Spawn one Sonnet 5 agent. It gets our export path, the target path, the measurement instruction, and nothing else. Brief:

```
Two terrain images. A is <our export>, B is <target>. B is the reference; A is an attempt to match it.

Judge A against B harshly. Harsh means precise and unsparing about the terrain, not about the person who made it. No praise, no softening, no score out of ten.

Answer in this order:
1. MATCH or NO MATCH. Would a terrain artist accept A as the same kind of landform as B at this resolution.
2. Ranked list of gaps, largest first, each in terrain terms with a measurement where you can take one (<measurement instruction>). Name what B has that A lacks, or what A has that B does not.
3. The single gap the next builder should close, one sentence.

Do not suggest code, node chains or parameter values. Do not open the app or take screenshots. Judge the files only.
```

### 3. Next brief

Take the critic's item 3 and item 2. Write the next builder brief: the gap to close, the critic's measurements, what to keep from this round. Add one row to the log. Write the brief into the progress file. Commit the file.

If the critic said MATCH, the loop is over. Say so, point at the commit, stop.

### 4. Compaction checkpoint

After every second round, stop before spawning the next builder. Check that the progress file has the full log and the next brief verbatim. Then print exactly one line and end the turn:

```
Round <N> done. Run: /compact focus on terrains/<name>.gauntlet.md, resume the gauntlet loop at round <N+1>
```

You cannot trigger `/compact` yourself. When the user comes back with "continue" or the compacted context, re-read the progress file, do not redo round zero, spawn the builder with the stored brief, and carry on.

## Exit

The critic says MATCH, or the user stops the run. Never a round count. Never the lead deciding it looks close enough.

## What breaks a gauntlet loop

- **A vague target.** Most common failure. The critic needs a file, not a description.
- **The lead judging.** You have seen every round and you want it to be done. The critic has fresh eyes. Trust the verdict over your own read.
- **Judging the app.** A viewport screenshot has lighting and camera baked in. Only built exports count, and CLAUDE.md forbids judging the app's look.
- **Judging at the wrong resolution.** A win at 512 says nothing about 2048.
- **Builder sees the critic's transcript.** It starts arguing with the verdict instead of fixing the terrain. Give it the gap, not the conversation.
- **A forked subagent.** `subagent_type: fork` carries this session into the builder or the critic. Both must start empty.
- **A brief that specifies the fix.** Every parameter value in the brief is one fewer decision the builder makes after reading `massif describe`.
- **Skipping the progress file write.** Compaction hits and the next brief is gone. Write before spawn, every round.
- **Two builders.** They fight over the same graph file and the critic cannot tell whose gap it is naming.
