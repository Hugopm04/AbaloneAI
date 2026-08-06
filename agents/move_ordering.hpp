#pragma once

// ---------------------------------------------------------------------------
// Move ordering
// ---------------------------------------------------------------------------
//
// Shared search infrastructure for the agents in this directory. Header-only,
// so it costs nothing to include and there is no CMake wiring to do -- the glob
// in CMakeLists.txt only picks up .cpp files.
//
// Two tools, for two different jobs that look alike but are not:
//
//   RootMoves    -- remembers the *real search scores* of the root moves and
//                   carries them between iterative-deepening passes.
//   order_moves  -- a cheap *static guess* for interior nodes, computed from
//                   the move alone, with no search and no board scan.
//
// Deliberately holds no Board and no Player: a container that cached the root
// position would evaluate the root at every leaf, silently and without any
// visible error. See the note on Position in CLAUDE.md.

#include <algorithm>
#include <cstddef>
#include <limits>
#include <vector>

#include "abalone/board.hpp"
#include "abalone/move.hpp"

namespace agents {

// ---------------------------------------------------------------------------
// RootMoves
// ---------------------------------------------------------------------------
//
// The root move list plus the score each move earned on the last completed
// iteration, kept in searched-best-first order.
//
// It exists to solve one specific problem. Iterative deepening restarts its
// scores at every depth, so when the clock kills a deep pass halfway through,
// the best move it found is the best of whatever *prefix* of the move list it
// happened to reach -- which need not include the move the previous depth chose.
// Ordering best-first puts the incumbent at index 0, so an interrupted pass is
// always at least as good as the pass before it.
//
// Usage is a strict cycle, one per depth:
//
//   1. reset() once, before the deepening loop.
//   2. set_score() for each root move as the search returns.
//   3. order_best_first() only when the iteration COMPLETED. Ordering on a
//      partial pass would promote unscored moves on stale or missing data.
class RootMoves {
public:
    static constexpr float kUnscored = -std::numeric_limits<float>::max();

    // Takes the engine's legal move list and starts every score at "worse than
    // anything". Generation order is preserved, so the first iteration searches
    // exactly what a plain deepening loop would.
    void reset(const std::vector<abalone::Move>& legal) {
        entries_.clear();
        entries_.reserve(legal.size());
        for (const abalone::Move& m : legal) entries_.push_back(Entry{m, kUnscored});
    }

    // Clears the scores but keeps the current order. Call at the top of each
    // iteration: the ordering from the last pass is what you want to search
    // first, but its scores came from a shallower search and must not be
    // compared against this pass's.
    void clear_scores() {
        for (Entry& e : entries_) e.score = kUnscored;
    }

    std::size_t size() const { return entries_.size(); }
    bool empty() const { return entries_.empty(); }

    const abalone::Move& move(std::size_t i) const { return entries_[i].move; }
    float score(std::size_t i) const { return entries_[i].score; }

    void set_score(std::size_t i, float s) { entries_[i].score = s; }

    // Highest score first. Stable, so moves left unscored by an interrupted
    // pass keep their relative order instead of being shuffled arbitrarily.
    void order_best_first() {
        std::stable_sort(entries_.begin(), entries_.end(),
                         [](const Entry& a, const Entry& b) { return a.score > b.score; });
    }

    // Best move of the last completed iteration. Only meaningful after
    // order_best_first(); before that it is simply the first generated move.
    const abalone::Move& best() const { return entries_.front().move; }
    float best_score() const { return entries_.front().score; }

private:
    struct Entry {
        abalone::Move move;
        float score = kUnscored;
    };

    std::vector<Entry> entries_;
};

// ---------------------------------------------------------------------------
// Interior-node ordering
// ---------------------------------------------------------------------------

// A static guess at how promising a move is, from the move alone -- no search,
// no board scan, so it is safe to call on every move of every node.
//
// The ranking: knocking a marble off is decisive, pushing is progress, and a
// longer line is harder for the opponent to answer.
inline int static_move_score(const abalone::Move& m) {
    int s = 0;
    if (m.pushes_off) s += 1000;
    s += 100 * m.pushed;
    s += m.count;
    return s;
}

// Sorts `moves` in place, most promising first.
//
// Only worth calling when `depth` is large enough that a cutoff saves more than
// the sort costs -- near the leaves, ordering is pure overhead. Hence the guard
// at the call site:
//
//   if (depth >= 2) agents::order_moves(moves);
inline void order_moves(std::vector<abalone::Move>& moves) {
    std::stable_sort(moves.begin(), moves.end(),
                     [](const abalone::Move& a, const abalone::Move& b) {
                         return static_move_score(a) > static_move_score(b);
                     });
}

}  // namespace agents
