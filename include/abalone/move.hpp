#pragma once

#include <string>
#include <vector>

#include "abalone/board.hpp"

namespace abalone {

// A move is a contiguous line of 1-3 friendly marbles moved one step in some
// direction. Two flavours:
//
//   Inline    -- the line moves along its own axis. May push opposing marbles
//                (sumito) when the mover strictly outnumbers them.
//   Broadside -- the line moves sideways. Every destination must be empty;
//                broadside moves can never push.
//
// A single marble has no axis, so its moves are all treated as inline.

struct Move {
    Coord head{};                  // Leading marble of the line, in `dir`.
    int count = 0;                 // 1..3 marbles moved.
    Direction line_dir = kEast;    // Axis the line runs along (from head backwards).
    Direction dir = kEast;         // Direction of travel.
    bool inline_move = false;
    int pushed = 0;                // Opposing marbles displaced (0 if none).
    bool pushes_off = false;       // True when a pushed marble leaves the board.

    // The marbles being moved, ordered head first.
    std::vector<Coord> marbles() const;

    // Standard-ish notation, e.g. "C3-C5 -> NE" or "E4 -> W".
    std::string to_string() const;
};

// All legal moves for `p`. Order is deterministic across runs, which keeps
// self-play games reproducible for a given seed.
std::vector<Move> generate_moves(const Board& board, Player p);

// Applies `move`, updating marble positions and push-off losses.
// `move` must have come from generate_moves() for this exact position.
void apply_move(Board* board, Player p, const Move& move);

// Convenience for the terminal UI: finds the move matching a typed command
// such as "C3 C5 NE" (line endpoints + direction) or "E4 W" (single marble).
bool parse_move(const std::vector<Move>& legal, const std::string& text, Move* out);

}  // namespace abalone
