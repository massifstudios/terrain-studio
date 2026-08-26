# Done

## T0 — Skeleton

The whole toolchain is proven end to end: a C++ engine, a native addon, and
an Electron window that talks to it.

**What works**

- The app opens a window with the four-panel layout stubbed out: top bar,
  preview area (3D and 2D panes, 2D toggleable), bottom graph/code panel,
  right-hand properties panel. All chrome comes from the Neoworks design
  system.
- The engine runs in its own process. The top bar shows its live status and
  the version it reports — that number travels from C++ through the native
  addon, across a direct port to the window.
- Building a package produces a runnable app folder with the engine binary
  bundled beside it.

**Where things are**

- `engine/` — the C++ core and its test suite.
- `bridge/` — the native addon that exposes the engine to the app.
- `app/` — the Electron shell and the Svelte interface.
- `ARCHITECTURE.md` — the decisions that are now locked.

**Useful to know**

- `task dev` runs the app, `task build` builds everything, `task test` runs
  both test suites, `task package` produces the distributable folder.
- The `task` runner is installed here as `go-task`; either name works if
  `task` is on the path.
