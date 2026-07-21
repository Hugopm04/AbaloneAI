#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace abalone {

// ---------------------------------------------------------------------------
// Coordinates
// ---------------------------------------------------------------------------
//
// The board is stored as a padded 9x9 grid indexed by (row, col). Rows run
// bottom-to-top: row 0 is rank 'A' (5 cells), row 4 is rank 'E' (9 cells,
// the widest), row 8 is rank 'I' (5 cells).
//
// Columns are aligned in an axial frame so that hex adjacency is uniform
// across the whole board:
//
//   rows 0..4 (A..E) occupy cols 0 .. row+4
//   rows 5..8 (F..I) occupy cols row-4 .. 8
//
// With that alignment the six neighbours of (r, c) are always the same six
// offsets -- no special-casing above/below the middle row.

inline constexpr int kRows = 9;
inline constexpr int kCols = 9;
inline constexpr int kCells = 61;

// Marbles each side starts with; every opening uses the same count.
inline constexpr int kMarblesPerPlayer = 14;

struct Coord {
    int row = -1;
    int col = -1;

    constexpr bool operator==(const Coord& o) const { return row == o.row && col == o.col; }
    constexpr bool operator!=(const Coord& o) const { return !(*this == o); }
};

// Six hex directions. Order matters: dir i and dir i+3 are opposites.
enum Direction : int {
    kEast = 0,
    kNorthEast = 1,
    kNorthWest = 2,
    kWest = 3,
    kSouthWest = 4,
    kSouthEast = 5,
};

inline constexpr int kNumDirections = 6;

inline constexpr std::array<Coord, kNumDirections> kDirOffsets = {{
    { 0, +1},  // E
    {+1, +1},  // NE
    {+1,  0},  // NW
    { 0, -1},  // W
    {-1, -1},  // SW
    {-1,  0},  // SE
}};

inline constexpr Direction opposite(Direction d) {
    return static_cast<Direction>((static_cast<int>(d) + 3) % kNumDirections);
}

const char* direction_name(Direction d);

// Parses notation like "E5" / "e5". Returns false if malformed or off-board.
bool parse_coord(const std::string& text, Coord* out);
std::string coord_to_string(Coord c);

// True when (row, col) is one of the 61 playable cells.
bool on_board(Coord c);

inline Coord step(Coord c, Direction d) {
    const Coord off = kDirOffsets[static_cast<int>(d)];
    return Coord{c.row + off.row, c.col + off.col};
}

// ---------------------------------------------------------------------------
// Cell contents
// ---------------------------------------------------------------------------

enum class Cell : std::uint8_t {
    kEmpty = 0,
    kBlack = 1,
    kWhite = 2,
    kOffBoard = 3,
};

enum class Player : std::uint8_t {
    kBlack = 1,
    kWhite = 2,
};

inline constexpr Player other(Player p) {
    return p == Player::kBlack ? Player::kWhite : Player::kBlack;
}

inline constexpr Cell to_cell(Player p) {
    return p == Player::kBlack ? Cell::kBlack : Cell::kWhite;
}

const char* player_name(Player p);

// ---------------------------------------------------------------------------
// Opening positions
// ---------------------------------------------------------------------------

enum class Opening {
    kClassic,       // default: two solid rows plus three centred marbles
    kBelgianDaisy,  // two separated clusters per side; far less drawish
};

const char* opening_name(Opening o);
bool parse_opening(const std::string& text, Opening* out);

// ---------------------------------------------------------------------------
// Board
// ---------------------------------------------------------------------------
//
// Holds only the marble layout and how many marbles each side has lost.
// Turn order, move counting and end-of-game policy live in Game.

class Board {
public:
    Board();  // empty board
    static Board from_opening(Opening opening);

    Cell at(Coord c) const;
    void set(Coord c, Cell value);

    // Marbles of `p` that have been pushed off the edge. 6 => that side loses.
    int losses(Player p) const;
    void add_loss(Player p);

    // Marbles still on the board.
    int marbles(Player p) const;

    // All 61 playable coordinates, in a stable order.
    static const std::vector<Coord>& cells();

private:
    std::array<Cell, kRows * kCols> grid_{};
    int black_losses_ = 0;
    int white_losses_ = 0;
};

// ---------------------------------------------------------------------------
// Cheap queries for agents
// ---------------------------------------------------------------------------
//
// These take the board and the player explicitly, so they answer about the
// position you hand them -- which inside a search is the node you are on, not
// the root. Both are O(1): the board already counts marbles pushed off, so
// there is nothing to scan. Prefer marbles_left() over Board::marbles(), which
// walks all 61 cells.

inline int marbles_left(const Board& board, Player p) {
    return kMarblesPerPlayer - board.losses(p);
}

// True once `p` has lost 6 marbles, i.e. `p` has lost the game.
inline bool is_eliminated(const Board& board, Player p) {
    return board.losses(p) >= 6;
}

inline bool game_over(const Board& board) {
    return is_eliminated(board, Player::kBlack) || is_eliminated(board, Player::kWhite);
}

}  // namespace abalone
