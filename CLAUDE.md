# AbaloneIA — project notes

Abalone (the hex-board marble-pushing game), **not** Avalon. C++17, no external dependencies.

## Layout

```
include/abalone/   public headers (board, move, agent, game, ui, arena)
src/               engine implementation + main.cpp
agents/            user-written AIs; add new files to AGENT_SOURCES in CMakeLists.txt
tests/             assert-based rule tests, no framework
docs/              writing_agents.md, toolchain_setup.md
```

## Design decisions already made

- **Pure C++.** No Python bindings — the user writes AIs in C++.
- **Terminal UI only.** Headless batch play matters more than graphics; the arena needs to
  run many games fast.
- **Classic opening is the default**, Belgian Daisy is selectable.
- **Move limits are optional.** Games are uncapped unless a limit is set. Do not hardcode one.
- **Time limits are engine-enforced.** Agents run on a worker thread and cannot overrun their
  budget even by accident. This was an explicit requirement — do not weaken it to a
  cooperative "please check the deadline" scheme.
- **Only `random` ships as an agent**, as a worked example. Do not add more reference bots
  unless asked; the point is for the user to write them.

## Board representation

61 cells in a padded 9×9 grid. Rows 0–8 are ranks A–I with lengths 5,6,7,8,9,8,7,6,5. Columns
are aligned in an axial frame (rows A–E start at column 0; rows F–I start at column `row-4`)
so that all six hex neighbours are the *same six offsets everywhere* — no special-casing
above and below the middle row. `kDirOffsets[i]` and `kDirOffsets[i+3]` are opposites.

## Status

Written but **never compiled** — this machine has no working C++ toolchain (see
`docs/toolchain_setup.md`). Before trusting any of it, get it building and run the tests.
