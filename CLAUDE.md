# AbaloneIA — project notes

Abalone (the hex-board marble-pushing game), **not** Avalon. C++17, no external dependencies.

## Layout

```
include/abalone/   public headers (board, move, agent, game, ui, gui, arena)
src/               engine implementation + main.cpp + gui.cpp (raylib, optional)
agents/            user-written AIs; every .cpp here is globbed into the build
tests/             assert-based rule tests, no framework
docs/              writing_agents.md, toolchain_setup.md
```

## Design decisions already made

- **Pure C++.** No Python bindings — the user writes AIs in C++.
- **Graphical UI is the default front end** (raylib, `src/gui.cpp`), because entering moves
  as `C3 C5 NE` was too cumbersome — you click a marble group, then click where it goes.
  This *reverses* two earlier decisions ("terminal UI only", "no external dependencies");
  it was an explicit user decision, so do not revert it.
  - The terminal UI is still fully supported and lives on under `--tui`. Do not delete it.
  - **raylib is confined to the `abalone` executable.** The engine library, the agents and
    the arena must never link it, so headless training still builds and runs on a machine
    with no GPU, no display and no raylib. `-DABALONE_GUI=OFF` must always keep working.
  - Detect raylib with the hand-rolled `find_path`/`find_library` in `CMakeLists.txt`, not
    `find_package(raylib)` — MSYS2's config resolves paths via pkg-config and silently
    reports success with an empty include dir when pkg-config is absent.
- **Headless batch play still matters most.** The arena needs to run many games fast;
  `--headless` takes the whole match configuration from the command line for exactly that.
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

**Builds, tests pass, and plays.** Built with g++ 16.1.0 (MSYS2 UCRT64) and the **Ninja**
generator — not the default Visual Studio one (see `docs/toolchain_setup.md`). No source
changes were needed. Run `ctest --test-dir build --output-on-failure` after engine changes.

## Agent helpers

Shared conveniences for user-written AIs are free functions in `board.hpp`
(`marbles_left`, `is_eliminated`, `game_over`), documented under "Helper functions" in
`docs/writing_agents.md`. Keep that section in sync whenever a helper is added — it is the
reference the user writes bots against.

Two constraints on any helper added here:

- **Take `(board, player)` explicitly; never hang it off `Position`.** An earlier version
  put `own_marbles()`/`enemy_marbles()` on `Position`, which read the *root* board and
  `to_move`. Called from inside a search they returned the same value at every leaf, making
  the evaluator a constant without any visible error. Removed for that reason — do not
  reintroduce that shape.
- **Cheap enough for a search loop.** Prefer counters the engine already maintains
  (`kMarblesPerPlayer - board.losses(p)`) over rescanning the 61 cells; `Board::marbles()`
  is the O(61) version and exists only for non-hot code.
