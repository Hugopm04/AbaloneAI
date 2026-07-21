# AbaloneIA

A playable Abalone engine in C++17, plus a workspace for writing and benchmarking your own AIs.

Abalone is a two-player abstract game on a 61-cell hex board. Each side starts with 14 marbles
and pushes the opponent's off the edge; **losing 6 marbles loses the game**. Perfect information,
no randomness — which makes it a clean testbed for search-based AI.

## Building

Requires a C++17 compiler and CMake 3.16+.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The graphical UI needs **raylib** (`pacman -S mingw-w64-ucrt-x86_64-raylib` on MSYS2). If it
isn't installed the build still succeeds, just without the window — or disable it explicitly
with `-DABALONE_GUI=OFF`, which is what you want on a headless training box.

Then run the game:

```sh
./build/abalone          # Linux/macOS
.\build\abalone.exe      # Windows (PowerShell)
```

The `.\` prefix is required in PowerShell. Note that Ninja is single-config, so the
executable lands directly in `build/` — there is no `build/Release/` subdirectory. If you
build with the Visual Studio generator instead, it *is* `build\Release\abalone.exe`.

## Three ways to run it

| Command | What you get |
|---|---|
| `abalone` | **Graphical window** (default) — click a marble group, click where it goes |
| `abalone --tui` | The original terminal UI, with typed `C3 C5 NE` notation |
| `abalone --headless` | No UI at all: batch AI-vs-AI games and statistics, for training |

### Playing in the window

Click one, two or three of your own marbles to select a line. Every square you can legally
move that group to lights up, colour-coded — **green** a quiet move, **orange** a push,
**red** a push that knocks a marble off the edge. Click one to play it. Click an empty
square or press `Esc` to start over. No coordinates, no direction names.

### Headless batch play

Everything is on the command line, so it scripts cleanly:

```sh
abalone --headless --black my_agent --white random --games 500 --move-limit 400 --quiet
```

| Option | Meaning |
|---|---|
| `--black NAME` / `--white NAME` | Which agent takes each seat (default: first registered) |
| `--games N` | Games to play (default: 100) |
| `--move-limit N` | Cap game length in plies (default: uncapped) |
| `--time-per-move MS` | Per-move budget (default: unlimited) |
| `--opening NAME` | `classic` or `belgian` |
| `--quiet` | Final statistics only, no per-game lines |

Unknown agent names and bad options exit non-zero with a message, so a typo in a training
script fails loudly instead of quietly benchmarking the wrong bot. `abalone --help` lists
the registered agents.

And the rule tests:

```sh
ctest --test-dir build --output-on-failure
```

## What's in the menu

| Mode | What it does |
|---|---|
| Human vs Human | Two players at one screen |
| Human vs AI | Play any registered agent, either colour |
| AI vs AI | Watch a single game move by move |
| Arena | Batch AI-vs-AI games with win rates and search statistics |
| Settings | Opening, move limit, time per move |

## Rules implemented

- **Openings:** Classic (default) and Belgian Daisy.
- **Moves:** lines of 1–3 marbles, moved inline or broadside (sideways).
- **Sumito:** an inline move pushes opposing marbles only when it strictly outnumbers
  them (2v1, 3v1, 3v2). Pushes are blocked by a friendly marble behind the opposing line.
- **Scoring:** a marble pushed off the edge is lost. Six losses ends the game.
- **Move limit:** *optional*. Games are uncapped by default; set a ply limit in Settings
  and reaching it is scored as a draw. Useful to guarantee tournaments terminate.

## Board notation

Rows are lettered `A` (bottom, 5 cells) through `I` (top, 5 cells), with `E` the widest
row of 9. Files are numbered from 1 within each row, so valid cells run `A1`–`A5`,
`B1`–`B6`, … `E1`–`E9`, … `I1`–`I5`.

Directions are `E`, `NE`, `NW`, `W`, `SW`, `SE`.

Enter a move as the two ends of your line plus a direction — `C3 C5 NE` — or, for a single
marble, `E4 W`. Type `l` at the prompt to list every legal move.

## Writing an AI

See **[docs/writing_agents.md](docs/writing_agents.md)** for the full guide, and
[agents/random_agent.cpp](agents/random_agent.cpp) for a complete worked example.

The short version:

```cpp
class MyAgent : public abalone::Agent {
public:
    std::string name() const override { return "my_agent"; }

    void choose_move(const abalone::Position& pos, abalone::SearchContext& ctx) override {
        ctx.submit(pos.legal[0]);          // always publish something first
        // ... search, calling ctx.submit() whenever you find better ...
    }
};

REGISTER_AGENT(MyAgent);
```

Add the file to `AGENT_SOURCES` in `CMakeLists.txt` and it appears in the menu automatically.

## Time limits are enforced, not requested

`choose_move()` runs on a worker thread. When the configured time per move runs out, the
engine plays whatever you last passed to `ctx.submit()` and moves on — **an agent cannot
overrun its budget, even by accident.**

Two consequences worth internalising:

- **Submit a legal move immediately**, before searching. If the clock expires and you have
  submitted nothing, the engine picks a move for you and marks the turn as a **forfeit** in
  the output. That is always a bug in your agent.
- **Don't touch shared state after the deadline.** Your thread may still be running after
  the engine has moved on. Anything it writes to must be owned by your agent.

`ctx.deadline_passed()` lets a well-behaved search exit cleanly instead of being abandoned
mid-way — the natural fit for iterative deepening.

## Measuring your search

`SearchContext` tracks two deliberately separate counters:

- `ctx.count_node()` — every position you generate or step into.
- `ctx.count_eval()` — every position you actually run a heuristic on.

They mean different things, and the gap between them *is* your pruning. A brute-force search
evaluates nearly every node it touches; a well-ordered alpha-beta search touches far more
nodes than it evaluates. The arena reports both per move, along with the ratio, so you can
see directly whether a pruning change earned its keep.

## Status

**Builds, tests pass, and plays.** First successful build was with g++ 16.1.0 (MSYS2 UCRT64)
and the Ninja generator — see [docs/toolchain_setup.md](docs/toolchain_setup.md). The engine
compiled clean with no source changes; the rule suite in `tests/test_rules.cpp` passes, and a
20-game random-vs-random arena ran to completion with no forfeits.

Use the Ninja generator, not the default Visual Studio one:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
```
