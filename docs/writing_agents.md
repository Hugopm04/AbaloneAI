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
