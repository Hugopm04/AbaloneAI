#include "abalone/board.hpp"

#include <algorithm>
#include <cctype>

namespace abalone {
namespace {

// Playable columns for each row: [start, start + length).
constexpr std::array<int, kRows> kRowStart = {0, 0, 0, 0, 0, 1, 2, 3, 4};
constexpr std::array<int, kRows> kRowLen = {5, 6, 7, 8, 9, 8, 7, 6, 5};

// Converts a 1-based rank index ("C3" -> 3) into a grid column.
constexpr int file_to_col(int row, int file_1based) {
    return kRowStart[row] + (file_1based - 1);
}

std::vector<Coord> build_cells() {
    std::vector<Coord> out;
    out.reserve(kCells);
    for (int r = 0; r < kRows; ++r) {
        for (int i = 0; i < kRowLen[r]; ++i) {
            out.push_back(Coord{r, kRowStart[r] + i});
        }
    }
    return out;
}

}  // namespace

const char* direction_name(Direction d) {
    switch (d) {
        case kEast: return "E";
        case kNorthEast: return "NE";
        case kNorthWest: return "NW";
        case kWest: return "W";
        case kSouthWest: return "SW";
        case kSouthEast: return "SE";
    }
    return "?";
}

const char* player_name(Player p) {
    return p == Player::kBlack ? "Black" : "White";
}

const char* opening_name(Opening o) {
    return o == Opening::kClassic ? "Classic" : "Belgian Daisy";
}

bool parse_opening(const std::string& text, Opening* out) {
    std::string lowered;
    for (char ch : text) lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    if (lowered == "classic" || lowered == "c" || lowered == "1") {
        *out = Opening::kClassic;
        return true;
    }
    if (lowered == "belgian" || lowered == "belgian daisy" || lowered == "daisy" ||
        lowered == "b" || lowered == "2") {
        *out = Opening::kBelgianDaisy;
        return true;
    }
    return false;
}

bool on_board(Coord c) {
    if (c.row < 0 || c.row >= kRows) return false;
    const int start = kRowStart[c.row];
    return c.col >= start && c.col < start + kRowLen[c.row];
}

bool parse_coord(const std::string& text, Coord* out) {
    if (text.size() < 2 || text.size() > 2) return false;
    const char rank = static_cast<char>(std::toupper(static_cast<unsigned char>(text[0])));
    if (rank < 'A' || rank > 'I') return false;
    if (!std::isdigit(static_cast<unsigned char>(text[1]))) return false;

    const int row = rank - 'A';
    const int file = text[1] - '0';
    if (file < 1 || file > kRowLen[row]) return false;

    *out = Coord{row, file_to_col(row, file)};
    return on_board(*out);
}

std::string coord_to_string(Coord c) {
    if (!on_board(c)) return "??";
    std::string s;
    s.push_back(static_cast<char>('A' + c.row));
    s.push_back(static_cast<char>('0' + (c.col - kRowStart[c.row] + 1)));
    return s;
}

const std::vector<Coord>& Board::cells() {
    static const std::vector<Coord> kAll = build_cells();
    return kAll;
}

Board::Board() {
    grid_.fill(Cell::kOffBoard);
    for (const Coord& c : cells()) {
        grid_[c.row * kCols + c.col] = Cell::kEmpty;
    }
}

Cell Board::at(Coord c) const {
    if (c.row < 0 || c.row >= kRows || c.col < 0 || c.col >= kCols) return Cell::kOffBoard;
    return grid_[c.row * kCols + c.col];
}

void Board::set(Coord c, Cell value) {
    if (!on_board(c)) return;
    grid_[c.row * kCols + c.col] = value;
}

int Board::losses(Player p) const {
    return p == Player::kBlack ? black_losses_ : white_losses_;
}

void Board::add_loss(Player p) {
    if (p == Player::kBlack) {
        ++black_losses_;
    } else {
        ++white_losses_;
    }
}

int Board::marbles(Player p) const {
    const Cell want = to_cell(p);
    int count = 0;
    for (const Coord& c : cells()) {
        if (at(c) == want) ++count;
    }
    return count;
}

Board Board::from_opening(Opening opening) {
    Board b;

    // Places marbles listed as (rank letter, 1-based file) pairs.
    const auto place = [&b](Cell who, std::initializer_list<std::pair<char, int>> spots) {
        for (const auto& [rank, file] : spots) {
            const int row = rank - 'A';
            b.set(Coord{row, file_to_col(row, file)}, who);
        }
    };

    if (opening == Opening::kClassic) {
        // Two solid back rows plus three centred marbles, 14 per side.
        place(Cell::kBlack, {{'A', 1}, {'A', 2}, {'A', 3}, {'A', 4}, {'A', 5},
                            {'B', 1}, {'B', 2}, {'B', 3}, {'B', 4}, {'B', 5}, {'B', 6},
                            {'C', 3}, {'C', 4}, {'C', 5}});
        place(Cell::kWhite, {{'I', 1}, {'I', 2}, {'I', 3}, {'I', 4}, {'I', 5},
                            {'H', 1}, {'H', 2}, {'H', 3}, {'H', 4}, {'H', 5}, {'H', 6},
                            {'G', 3}, {'G', 4}, {'G', 5}});
    } else {
        // Belgian Daisy: each side holds two opposing 7-marble clusters.
        place(Cell::kBlack, {{'A', 1}, {'A', 2}, {'B', 1}, {'B', 2}, {'B', 3}, {'C', 2}, {'C', 3},
                            {'I', 4}, {'I', 5}, {'H', 4}, {'H', 5}, {'H', 6}, {'G', 5}, {'G', 6}});
        place(Cell::kWhite, {{'A', 4}, {'A', 5}, {'B', 4}, {'B', 5}, {'B', 6}, {'C', 5}, {'C', 6},
                            {'I', 1}, {'I', 2}, {'H', 1}, {'H', 2}, {'H', 3}, {'G', 2}, {'G', 3}});
    }

    return b;
}

}  // namespace abalone
