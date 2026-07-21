# Writing an AI

Everything you need is in `include/abalone/agent.hpp`, and
[`agents/random_agent.cpp`](../agents/random_agent.cpp) is a complete, heavily commented
example. Copy that file to start.

## The three steps

1. Create `agents/my_agent.cpp`.
2. Subclass `abalone::Agent`; implement `name()` and `choose_move()`.
3. `REGISTER_AGENT(MyAgent);` at file scope.

Rebuild and it appears in the main menu, in the arena and in `abalone --help`. Every `.cpp`
in `agents/` is compiled automatically — there is no build file to edit.

If a new agent does not show up, it is almost always one of two things: the file is not in
`agents/` (only that directory is scanned), or `REGISTER_AGENT` is missing. The menu is
built from the registry, so an unregistered agent is invisible everywhere at once rather
than just in the GUI.

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

### Position queries

| Call | Returns | Cost |
| --- | --- | --- |
| `marbles_left(board, p)` | Marbles `p` still has on the board | O(1) |
| `is_eliminated(board, p)` | True once `p` has lost 6 marbles | O(1) |
| `game_over(board)` | True when either side has been eliminated | O(1) |

Nothing here scans the board — `Board` keeps the counts up to date as marbles move. A
material term is then:

```cpp
const int material = marbles_left(board, p) - marbles_left(board, other(p));
```

**They take the board and the player explicitly, and that is deliberate.** Inside a search
you are looking at a board that is not `pos.board` and, half the time, a player that is not
`pos.to_move`. An evaluator should be a pure function of `(board, player)` — if it reaches
for `pos` instead, it returns the same score at every leaf and the search silently stops
meaning anything. Pass the node down; do not close over the root.

At the root — in `choose_move()`, before any search — you build the pair yourself from
`pos`, which is the one place where doing so is correct:

```cpp
const abalone::Board& board = pos.board;
const abalone::Player me    = pos.to_move;
const abalone::Player enemy = abalone::other(me);

const int my_marbles    = marbles_left(board, me);
const int enemy_marbles = marbles_left(board, enemy);
const int my_losses     = board.losses(me);       // marbles I have had pushed off
const int enemy_losses  = board.losses(enemy);    // marbles I have pushed off
```

`board.losses(p)` is the losses counterpart of `marbles_left(board, p)` — the two always sum
to `kMarblesPerPlayer`, so use whichever reads better in your heuristic. Both take any
player, so there is no separate "own" and "enemy" call to look for; `other(p)` supplies the
opposing side. Inside the search, the same four lines work with the node's `board` and `p`
substituted for `pos.board` and `pos.to_move` — that substitution is the whole difference,
and it is why the helpers do not do it for you.

### On `Board`

| Call | Returns | Cost |
| --- | --- | --- |
| `board.at(coord)` | `Cell::kEmpty` / `kBlack` / `kWhite` / `kOffBoard` | O(1) |
| `board.losses(p)` | Marbles of `p` pushed off, for either player; 6 means `p` has lost | O(1) |
| `board.marbles(p)` | Marbles of `p` on the board | O(1) — counter kept up to date by `set()` |
| `Board::cells()` | The 61 playable coordinates, stable order | O(1) to obtain |

`marbles_left(board, p)` is just `board.marbles(p)`; use either. `Board` keeps a running
count per player, updated in `set()`, so neither one scans. Use `Board::cells()` only when
you genuinely need to walk the whole board, e.g. a centre-of-mass or cohesion term.

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

### Where the counters go

Count inside the search, not in the deepening loop — and **do not reset between
iterations**. `ctx` accumulates across the whole turn, so the number the arena reports is
the total work the turn cost, which is what you want when comparing two agents.

Here is a plain minimax — no pruning, nothing clever — with the counters in place. It is
written in the negamax form, where one function serves both sides by negating the child's
score, so `evaluate()` must return its score **from the point of view of `p`**:

```cpp
float search(const abalone::Board& board, abalone::Player p, int depth,
             abalone::SearchContext& ctx) {
    if (depth == 0 || abalone::game_over(board)) {
        ctx.count_eval();               // leaf: the heuristic actually ran
        return evaluate(board, p);
    }

    auto moves = abalone::generate_moves(board, p);
    ctx.count_node(moves.size());       // positions this node put in front of us

    float best = -std::numeric_limits<float>::max();
    for (const abalone::Move& m : moves) {
        if (ctx.deadline_passed()) break;
        abalone::Board next = board;
        abalone::apply_move(&next, p, m);
        best = std::max(best, -search(next, abalone::other(p), depth - 1, ctx));
    }
    return best;
}
```

Use `-std::numeric_limits<float>::max()` (or `::lowest()`) for "worse than anything". For
floating-point types `::min()` is the smallest *positive* value, roughly `1.2e-38`, so a
lost game would score as slightly good and the search would walk straight into it.

With no pruning, `nodes` and `evals` stay in lockstep — that is the baseline the ratio is
measured against once you add cutoffs.

Two things follow from this that surprise people:

- **Re-searching the same node at every depth is counted every time, and that is correct.**
  Iterative deepening genuinely does revisit depth 1 on each pass. The re-search is real
  work, so it belongs in the total. Trying to deduplicate makes your numbers incomparable
  with everyone else's.
- **Only the leaf calls `count_eval()`.** Put it where `evaluate()` is called and nowhere
  else. If you also count it at interior nodes, the ratio stops telling you anything about
  pruning once you add it.

Counting `moves.size()` once per node, rather than `count_node()` per child inside the
loop, means a node whose children are all pruned still reports the moves it generated — the
cost you actually paid. Either convention is defensible; just pick one and keep it, since
the ratio is only meaningful compared against your own earlier runs.

## Breadth-first search

Worth saying plainly first: **for game playing, iterative deepening is the breadth-first
approach you actually want.** It visits the tree in the same depth-by-depth order, but keeps
only one path in memory instead of a whole frontier, and it can stop at any moment with a
usable answer. A literal BFS over a branching factor that runs to hundreds of moves per
position will exhaust memory a couple of plies in.

It is still useful when you want to reason about the frontier itself — say, scoring every
position reachable in exactly N plies, or collecting statistics about the opening tree. The
shape is a queue of (board, side to move, the root move that led here):

```cpp
#include <deque>

struct Node {
    abalone::Board board;
    abalone::Player to_move;
    std::size_t root_index;   // which of pos.legal this subtree came from
    int depth;
};

void choose_move(const abalone::Position& pos, abalone::SearchContext& ctx) override {
    ctx.submit(pos.legal.front());          // rule 1: always publish first

    constexpr int kMaxDepth = 2;
    std::vector<int> best_for_root(pos.legal.size(), kMinScore);

    std::deque<Node> frontier;
    for (std::size_t i = 0; i < pos.legal.size(); ++i) {
        abalone::Board next = pos.board;
        abalone::apply_move(&next, pos.to_move, pos.legal[i]);
        frontier.push_back({next, abalone::other(pos.to_move), i, 1});
    }
    ctx.count_node(pos.legal.size());

    while (!frontier.empty()) {
        if (ctx.deadline_passed()) return;  // frontier is ours; just walk away

        const Node node = frontier.front();
        frontier.pop_front();

        if (node.depth == kMaxDepth) {
            ctx.count_eval();
            const int score = evaluate(node.board, pos.to_move);
            best_for_root[node.root_index] =
                std::max(best_for_root[node.root_index], score);
            continue;
        }

        auto replies = abalone::generate_moves(node.board, node.to_move);
        ctx.count_node(replies.size());
        for (const abalone::Move& r : replies) {
            abalone::Board next = node.board;
            abalone::apply_move(&next, node.to_move, r);
            frontier.push_back({next, abalone::other(node.to_move),
                                node.root_index, node.depth + 1});
        }
    }

    const auto it = std::max_element(best_for_root.begin(), best_for_root.end());
    ctx.submit(pos.legal[std::distance(best_for_root.begin(), it)]);
}
```

Points to note:

- `root_index` is what makes the result usable. BFS loses the path, so each node has to
  carry which root move it descended from or you cannot turn a leaf score into a decision.
- The **submit only happens at the end**, once the whole frontier is drained. That is the
  weakness compared with iterative deepening: cut the search off early and everything past
  the opening `submit()` is wasted. Never submit from a partially explored frontier — a
  score built from some of the replies but not others is not comparable across roots.
- Returning early on `deadline_passed()` is safe here because `frontier` and
  `best_for_root` are locals. Keep it that way; see rule 2 above.
- `evaluate()` takes `pos.to_move` as the perspective, not `node.to_move`. Every score has
  to be from the root player's point of view or the maximum at the end is meaningless.
- Each level multiplies the frontier by the branching factor, which in Abalone is often
  60–100. Depth 3 is already millions of boards. Bound it, and treat this as a tool for
  analysis rather than the engine of a competitive bot.

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
