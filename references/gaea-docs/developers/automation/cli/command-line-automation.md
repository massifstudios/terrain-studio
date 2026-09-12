Source: https://docs.gaea.app/developers/automation/cli/command-line-automation.html

# Command Line Automation

Gaea's Build Swarm (`Gaea.Swarm.exe`) can be automated via the command line interface.

See [Command-line Interface](https://docs.gaea.app/developers/automation/cli/index.html) for details on the full CLI available in both Gaea.exe and Gaea.Swarm.exe

## Arguments

The Build Swarm can accept the following arguments:

| Argument | Description |
| --- | --- |
| `-filename <filename>` | The fully qualified path to the .terrain file that should be built. It should be wrapped `""` quotes for safety. |
| `-interactive` | Show interactive prompts when launched from the CLI. |
| `-ignorecache` | Ignore baked cache when set to `true`. |
| `-verbose` | Enable Verbose logging for diagnostics purposes. |
| `-safemode` | Enforce Sade Mode for debugging or diagnostics purposes. Not recommended for production builds. |
| `-seed <int>` | Mutation seed to use for the build. |
| `-v` or `--v` | Variables. This argument marks the beginning of key-value pairs of variables.   *This must ALWAYS be the last argument. All other switches and arguments should precede* `-v`    The `-v` argument must be repeated for each variable (key-value pair).  For example:  `-v foo=bar -v value=0.35 -v erosion=1` |
| `--va <string>` | Comma separated variable values alphanumerically-sorted by variable names. Must include all values. This is to help reduce character count by not having to i    Variables passed this argument must be equal in count to the number of variables declared in the terrain file. |
| `-vars <filename>` | Instead of `-v` the `-vars` argument can be used to pass a specifically formatted plain text or JSON file to provide Variable values. |

![](https://docs.gaea.app/.data/assets/command_line_example.png)

The [Build Options](https://docs.gaea.app/ui/interface/build-options/index.html) window has a helpful button to Show Command Line that will show the current file's Build Command Line including all variables. It is useful to copy this and then modify it as needed.

### Using Files for Variables

You can store variables in delimited files or JSON and pass them in the `-v` argument. This is useful for more complex automation where an external tool may save variables instead of passing them directly to a command line.

#### Text file

The text file is an INI style dictionary of key-value pairs.

```
variable=value
filename=C:\Path\File.ext
erosion=0.35
switchErosionProcess=1
```

#### JSON File

```
{
  "variable": value,
  "filename": "C:\Path\File.ext",
  "erosion": 0.35,
  "switchErosionProcess": 1
}
```

Variable names cannot be repeated.

To see how variables can be bound to node properties, see [Variables](https://docs.gaea.app/developers/extensibility/scripting-and-expressions/variables.html)
