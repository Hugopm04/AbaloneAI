#pragma once

#include <iosfwd>
#include <string>

#include "abalone/game.hpp"

namespace abalone {

// Renders the hex board as indented ASCII, widest row in the middle:
//
//         I  o o o o o
//        H  o o o o o o
//       G  . . o o o . .
//      ...
//
// '@' = Black, 'o' = White, '.' = empty.
void render_board(std::ostream& os, const Board& board);

void render_status(std::ostream& os, const Game& game);

// Prints the per-move timing and node statistics for the last turn.
void render_move_report(std::ostream& os, Player mover, const MoveReport& report);

}  // namespace abalone
