Source: https://docs.gaea.app/developers/automation/

# Automation

Automation in Gaea starts with a `.terrain` file that already builds correctly in the UI, then makes that build repeatable from another process. The supported workflow in this section is command-line and file based: expose the values that should change, generate or write the command that should run, and let Build Swarm execute the build.

Use [Build Swarm](https://docs.gaea.app/using/advanced-topics/build-swarm/index.html) to understand the build engine itself. Use [Command-line Interface](https://docs.gaea.app/developers/automation/cli/index.html) for the available `Gaea.exe` and `Gaea.Swarm.exe` commands, then use [Command Line Automation](https://docs.gaea.app/developers/automation/cli/command-line-automation.html) for the Build Swarm arguments used in production automation.

## What You Can Automate

- Build a terrain through `Gaea.Swarm.exe` without opening the full UI.
- Select a Build Profile or Region for a specific output target.
- Override exposed variables from the command line or from a variable file.
- Set a mutation seed for repeatable variations.
- Copy a known-good command line from the [Commands](https://docs.gaea.app/ui/interface/build-options/commands.html) tab.
- Run a post-build command from the [Script](https://docs.gaea.app/ui/interface/build-options/script.html) tab.
- Build bridge-style workflows that pass files into Gaea and collect exported files afterward.

## Recommended Workflow

1. Build the terrain once in the UI so the graph, exports, resolution, and output folders are known to work.
2. Expose the values that the pipeline needs to control as [Variables](https://docs.gaea.app/developers/extensibility/scripting-and-expressions/variables.html). Use file-friendly variable names when those values will be set from another tool.
3. Configure [Build Options](https://docs.gaea.app/ui/interface/build-options/index.html), including Profiles or Regions if the same terrain needs multiple repeatable build targets.
4. Use **Copy Command Line** in Build Options to capture the exact command for the current file.
5. Replace the copied variable values with pipeline values, or move them into a `.json` or `.txt` variable file for easier automation.
6. Run the build through `Gaea.Swarm.exe`, then consume the exported files or post-action report in the next tool.

## Automation Building Blocks

| Need | Use |
| --- | --- |
| Launch, activation, diagnostics, or safe-mode startup | [Command-line Interface](https://docs.gaea.app/developers/automation/cli/index.html) |
| Headless terrain builds | [Command Line Automation](https://docs.gaea.app/developers/automation/cli/command-line-automation.html) |
| External control over node properties | [Variables](https://docs.gaea.app/developers/extensibility/scripting-and-expressions/variables.html) |
| Derived or constrained parameter values | [Expressions](https://docs.gaea.app/developers/extensibility/scripting-and-expressions/expressions.html) |
| Standardized preview, final, or engine-specific output sets | [Profiles](https://docs.gaea.app/ui/interface/build-options/profiles.html) |
| Focused builds for part of a terrain | [Regions](https://docs.gaea.app/ui/interface/build-options/regions.html) |
| Post-build file moves or downstream commands | [Script](https://docs.gaea.app/ui/interface/build-options/script.html) |
| Bridge workflows between Gaea and another application | [Building Bridges](https://docs.gaea.app/developers/automation/building-bridges/index.html) |

## Keep Automation Stable

- Start from a command copied from the UI before hand-writing automation commands.
- Quote file paths, especially paths coming from another application or build system.
- Prefer absolute paths for bridge workflows unless the caller controls the working directory.
- Use variables for values that must change between runs; do not edit the `.terrain` file for every build.
- Use `--vars` with a variable file when the command line would become long or hard to inspect.
- Decide intentionally whether cached data should be reused or whether the build should ignore cache.
- Keep variable preset files with the same project or pipeline configuration that launches the build.

## In this section

## In This Section

- [Developers](https://docs.gaea.app/developers/)
- [Extensibility](https://docs.gaea.app/developers/extensibility/)
