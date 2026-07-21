# Writing an AI

Everything you need is in `include/abalone/agent.hpp`, and
[`agents/random_agent.cpp`](../agents/random_agent.cpp) is a complete, heavily commented
example. Copy that file to start.

## The four steps

1. Create `agents/my_agent.cpp`.
2. Subclass `abalone::Agent`; implement `name()` and `choose_move()`.
3. `REGISTER_AGENT(MyAgent);` at file scope.
4. Add the file to `AGENT_SOURCES` in `CMakeLists.txt`.

It now appears in the main menu and in the arena. No other wiring.

## What you get

```cpp
struct Position {
    const Board& board;                 // current position
    Player to_move;                     // your colour
    int move_number;                    // plies played so far
    const std::vector<Move>& legal;     // pre-generated, never empty
};
```

The legal moves are generated for you, so a working agent can be one line. To search, you
apply moves to your own copy of the board:

```cpp
abalone::Board next = pos.board;
abalone::apply_move(&next, pos.to_move, some_move);
auto replies = abalone::generate_moves(next, abalone::other(pos.to_move));
```

`Board` is a plain value type — copying it is cheap and there is no undo to get wrong.

## Helper functions

Things the engine already tracks, so your evaluator does not have to recompute them. This
section grows as helpers are added; anything listed here is safe to call from inside a
search loop unless it is marked otherwise.

### On `Position`

| Call | Returns | Cost |
| --- | --- | --- |
| `pos.own_marbles()` | Marbles `pos.to_move` still has on the board | O(1) |
| `pos.enemy_marbles()` | Marbles the opponent still has on the board | O(1) |
| `pos.own_losses()` | Marbles of `pos.to_move` pushed off; 6 means you lost | O(1) |
| `pos.enemy_losses()` | Marbles of the opponent pushed off; 6 means you won | O(1) |

Both are derived from the push-off counters the board maintains
(`kMarblesPerPlayer - board.losses(...)`), not by scanning cells. A material term is then
just:

```cpp
const int material = pos.own_marbles() - pos.enemy_marbles();
```

Note these read `pos.to_move`, i.e. the *root* side. Inside a search you are looking at
boards for both colours, so at internal nodes use the `Board` calls below with an explicit
player and negate for the side to move as usual.

### On `Board`

| Call | Returns | Cost |
| --- | --- | --- |
| `board.at(coord)` | `Cell::kEmpty` / `kBlack` / `kWhite` / `kOffBoard` | O(1) |
| `board.losses(p)` | Marbles of `p` pushed off; 6 means `p` has lost | O(1) |
| `board.marbles(p)` | Marbles of `p` on the board | **O(61)** — scans every cell |
| `Board::cells()` | The 61 playable coordinates, stable order | O(1) to obtain |

`board.marbles(p)` and `kMarblesPerPlayer - board.losses(p)` give the same answer; prefer
the second one in hot code. Use `Board::cells()` when you genuinely need to walk the whole
board, e.g. a centre-of-mass or cohesion term.

### Geometry (`board.hpp`)

| Call | Returns |
| --- | --- |
| `step(coord, dir)` | The neighbour of `coord` in `dir` |
| `on_board(coord)` | Whether a coordinate is one of the 61 playable cells |
| `opposite(dir)` | The reverse direction; `kDirOffsets[i]` and `[i+3]` are opposites |
| `other(player)` | The other player |
| `to_cell(player)` | The `Cell` value matching a player |
| `coord_to_string(coord)` / `parse_coord(text, &out)` | Notation such as `"E5"` |

The six directions are `kEast`, `kNorthEast`, `kNorthWest`, `kWest`, `kSouthWest`,
`kSouthEast`, and the axial layout means those offsets are valid everywhere on the board —
no special cases above or below the middle row.

### Moves (`move.hpp`)

| Call | Returns |
| --- | --- |
| `generate_moves(board, p)` | All legal moves for `p`, deterministic order |
| `apply_move(&board, p, move)` | Applies a move in place, updating losses |
| `move.marbles()` | The moved coordinates, head first |
| `move.to_string()` | Readable notation, for debugging output |

`Move` also carries `pushed` (how many opposing marbles a sumito displaces) and
`pushes_off` (true when one of them leaves the board) — both are filled in by the generator,
so move-ordering heuristics can read them without simulating anything.

## The two rules you must follow

**1. Submit a legal move before you start thinking.**

```cpp
ctx.submit(pos.legal[0]);
```

The engine enforces the time limit by taking whatever you last submitted. Submit nothing
and it plays a move on your behalf and marks the turn **FORFEIT** in the output. Always
publish something first, then improve on it.

**2. Nothing you own may be touched by the engine after the deadline.**

Your `choose_move()` runs on a worker thread. If you overrun, the engine walks away and the
game continues while your thread is still running. The engine keeps your agent and its
context alive so this is memory-safe, but any state you share between turns can still be
corrupted by a search that outlived its turn. Either check `ctx.deadline_passed()` and
return, or keep per-search state strictly local.

## Iterative deepening

The pattern the API is shaped around:

```cpp
void choose_move(const Position& pos, SearchContext& ctx) override {
    ctx.submit(pos.legal.front());

    for (int depth = 1; depth < 64; ++depth) {
        std::optional<Move> best = search(pos, depth, ctx);
        if (ctx.deadline_passed()) break;   // depth incomplete, discard it
        if (best) ctx.submit(*best);        // only submit completed depths
    }
}
```

Note the ordering: check the deadline *before* submitting, so a half-finished depth never
overwrites a good result from a complete one.

## Instrumentation

```cpp
ctx.count_node();   // every position you generate or step into
ctx.count_eval();   // every position you run your heuristic on
```

These measure different things and you should call both. `nodes` is how much of the tree you
touched; `evals` is how much work you actually did at the leaves. Pruning makes them diverge —
that is the whole point of tracking them separately.

Rules of thumb once you start pruning:

- **nodes ≈ evals** — you are evaluating everything you see. No pruning is happening.
- **nodes ≫ evals** — cutoffs are working; you are discarding subtrees before scoring them.
- **evals climbing while depth stalls** — usually bad move ordering. Alpha-beta only pays
  off when you try good moves first; try the previous iteration's best move first.

Per-move figures appear during AI games, and the arena aggregates them across a match along
with the nodes-to-evals ratio.

## Timing your agent honestly

Set a time per move in Settings before benchmarking, otherwise an untimed search will run to
completion and the comparison means nothing. A fair arena run gives both agents the same
budget:

```
Settings -> Time per move -> 1000
Settings -> Move limit    -> 400        (so games always terminate)
Arena    -> my_agent vs random -> 100 games
```

Beating `random` is the floor, not a milestone — a one-ply greedy agent should manage it
comfortably. The interesting comparison is against your own previous version.
