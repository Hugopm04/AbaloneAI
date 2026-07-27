#pragma once

#include <string>

#include "abalone/board.hpp"
#include "abalone/game.hpp"

// ---------------------------------------------------------------------------
// Export formats. Abalone has no canonical wire format the way chess has
// FEN/PGN, so these are house formats. There are two, deliberately different:
//
//   * A board position is a single line of readable ASCII -- terse, but plain
//     text anyone (or any tool) can read at a glance and edit by hand:
//
//       abalone/board v1 opening=classic turn=w ply=12 losses=1,0 \
//         cells=A1:w,A2:w,C3:b,E5:b,...
//
//     Only occupied cells are listed, as <coord>:<b|w> in Board::cells() order.
//     `opening` is informational (the position is given explicitly by `cells`);
//     `turn` is the side to move, `losses` is black,white pushed off so far.
//
//   * A whole game is bit-packed and Base64-encoded into a short opaque blob,
//     because a move list wants to stay compact. Layout (little-endian):
//
//       [0] magic 0xAB  [1] version 0x01
//       [2] kind : 1 = game, 2 = game + stats
//       [3] opening : 0 = classic, 1 = Belgian daisy
//       [4..5] move_count : u16
//       [6] stat_mask : (kind 2 only) OR of StatField bits below
//       then per move 2 bytes, LSB-first:
//           head_index (0..60) 6 bits | count-1 (0..2) 2 bits |
//           line_dir (0..5) 3 bits | dir (0..5) 3 bits | inline_move 1 bit
//       for kind 2 each move's 2 bytes are followed by the selected stats, in
//       StatField bit order: time u32(ms), nodes u64, evals u64, score
//       (u8 present flag then f32 when present), flags u8 (bit0 timed_out,
//       bit1 forfeited).
// ---------------------------------------------------------------------------

namespace abalone {

// Which per-move statistics to include in a kind-2 (game + stats) blob.
enum StatField : unsigned {
    kStatTime  = 1u << 0,
    kStatNodes = 1u << 1,
    kStatEvals = 1u << 2,
    kStatScore = 1u << 3,
    kStatFlags = 1u << 4,  // timed_out / forfeited
};

// The current position on its own.
std::string encode_board(const Board& board, Player to_move, int ply, Opening opening);

// The whole game as its opening plus the move list, no statistics.
std::string encode_game(const Game& game);

// The whole game plus the selected per-move statistics. A stat_mask of 0
// produces the same content as encode_game() but tagged as kind 2.
std::string encode_game_with_stats(const Game& game, unsigned stat_mask);

// ---------------------------------------------------------------------------
// Decoding (the inverse of the encoders above).
// ---------------------------------------------------------------------------

// A game recovered from a kind-1 or kind-2 blob. The moves are fully formed
// (re-matched against the legal moves at each step as the game is replayed), so
// they can be handed straight to Game::play(). For a kind-1 blob every report
// carries only its move; a kind-2 blob also fills the stat fields that were
// selected at export time, and leaves the rest at their defaults.
struct DecodedGame {
    Opening opening = Opening::kClassic;
    std::vector<MoveReport> moves;
    bool has_stats = false;
    unsigned stat_mask = 0;
};

// Classifies a blob: 0 = board (the text form), 1 = game, 2 = game + stats, or
// -1 when it is neither a board line nor a valid game blob.
int blob_kind(const std::string& blob);

// Recovers a kind-0 (board) blob. Any out-pointer may be null. Returns false if
// the blob is not a valid board snapshot.
bool decode_board(const std::string& blob, Board* board, Player* to_move, int* ply,
                  Opening* opening);

// Recovers a kind-1 or kind-2 (game) blob by replaying it from its opening.
// Returns false if the blob is malformed or a recorded move is not legal in the
// position it reaches (i.e. the blob does not describe a real game).
bool decode_game(const std::string& blob, DecodedGame* out);

}  // namespace abalone
