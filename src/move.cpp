#include "abalone/move.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace abalone {
namespace {

constexpr int kMaxLine = 3;

// The three canonical axes. Using only half the directions when enumerating
// broadside lines stops every line being generated twice (once per end).
constexpr std::array<Direction, 3> kAxes = {kEast, kNorthEast, kNorthWest};

Coord step_n(Coord c, Direction d, int n) {
    for (int i = 0; i < n; ++i) c = step(c, d);
    return c;
}

bool parse_direction(const std::string& text, Direction* out) {
    std::string up;
    for (char ch : text) up.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
    for (int i = 0; i < kNumDirections; ++i) {
        if (up == direction_name(static_cast<Direction>(i))) {
            *out = static_cast<Direction>(i);
            return true;
        }
    }
    return false;
}

}  // namespace

std::vector<Coord> Move::marbles() const {
    std::vector<Coord> out;
    out.reserve(count);
    const Direction back = opposite(line_dir);
    for (int i = 0; i < count; ++i) out.push_back(step_n(head, back, i));
    return out;
}

std::string Move::to_string() const {
    const std::vector<Coord> ms = marbles();
    std::ostringstream os;
    if (ms.size() == 1) {
        os << coord_to_string(ms.front());
    } else {
        os << coord_to_string(ms.back()) << "-" << coord_to_string(ms.front());
    }
    os << " -> " << direction_name(dir);
    if (pushes_off) {
        os << " (push off)";
    } else if (pushed > 0) {
        os << " (push " << pushed << ")";
    }
    return os.str();
}

std::vector<Move> generate_moves(const Board& board, Player p) {
    std::vector<Move> moves;
    moves.reserve(96);

    const Cell own = to_cell(p);
    const Cell foe = to_cell(other(p));

    // --- Inline moves -------------------------------------------------------
    // `head` is the leading marble in the direction of travel. Requiring the
    // cell ahead not to be friendly makes each (group, direction) pair unique.
    for (const Coord& head : Board::cells()) {
        if (board.at(head) != own) continue;

        for (int di = 0; di < kNumDirections; ++di) {
            const Direction d = static_cast<Direction>(di);
            const Coord ahead = step(head, d);
            if (board.at(ahead) == own) continue;  // not really the head

            const Direction back = opposite(d);
            for (int n = 1; n <= kMaxLine; ++n) {
                // Every marble of the line must be ours.
                if (n > 1 && board.at(step_n(head, back, n - 1)) != own) break;

                Move m;
                m.head = head;
                m.count = n;
                m.line_dir = d;
                m.dir = d;
                m.inline_move = true;

                if (board.at(ahead) == Cell::kEmpty) {
                    moves.push_back(m);
                    continue;
                }
                if (board.at(ahead) != foe) continue;  // off-board, or blocked

                // Sumito: count the opposing marbles directly ahead.
                int pushed = 0;
                while (pushed < n && board.at(step_n(head, d, pushed + 1)) == foe) ++pushed;

                if (pushed >= n) continue;  // not outnumbered -> illegal

                const Coord landing = step_n(head, d, pushed + 1);
                const Cell landing_cell = board.at(landing);
                if (landing_cell == own || landing_cell == foe) continue;  // blocked behind

                m.pushed = pushed;
                m.pushes_off = (landing_cell == Cell::kOffBoard);
                moves.push_back(m);
            }
        }
    }

    // --- Broadside moves ----------------------------------------------------
    // Lines of 2-3 stepping sideways. Every destination must be empty.
    for (const Coord& tail : Board::cells()) {
        if (board.at(tail) != own) continue;

        for (const Direction axis : kAxes) {
            if (board.at(step(tail, opposite(axis))) == own) continue;  // not the tail

            for (int n = 2; n <= kMaxLine; ++n) {
                if (board.at(step_n(tail, axis, n - 1)) != own) break;

                for (int di = 0; di < kNumDirections; ++di) {
                    const Direction d = static_cast<Direction>(di);
                    if (d == axis || d == opposite(axis)) continue;  // that is an inline move

                    bool clear = true;
                    for (int i = 0; i < n && clear; ++i) {
                        clear = board.at(step(step_n(tail, axis, i), d)) == Cell::kEmpty;
                    }
                    if (!clear) continue;

                    Move m;
                    m.head = step_n(tail, axis, n - 1);
                    m.count = n;
                    m.line_dir = axis;
                    m.dir = d;
                    m.inline_move = false;
                    moves.push_back(m);
                }
            }
        }
    }

    return moves;
}

void apply_move(Board* board, Player p, const Move& move) {
    const Cell own = to_cell(p);
    const Cell foe = to_cell(other(p));

    if (!move.inline_move) {
        const std::vector<Coord> ms = move.marbles();
        for (const Coord& c : ms) board->set(c, Cell::kEmpty);
        for (const Coord& c : ms) board->set(step(c, move.dir), own);
        return;
    }

    // Inline. The whole chain shifts one cell, so only three cells change:
    // the vacated tail, the cell the head steps into, and wherever the far
    // end of the pushed block lands.
    if (move.pushed > 0) {
        const Coord landing = step_n(move.head, move.dir, move.pushed + 1);
        if (on_board(landing)) {
            board->set(landing, foe);
        } else {
            board->add_loss(other(p));
        }
    }

    const Coord tail = step_n(move.head, opposite(move.line_dir), move.count - 1);
    board->set(tail, Cell::kEmpty);
    board->set(step(move.head, move.dir), own);
}

bool parse_move(const std::vector<Move>& legal, const std::string& text, Move* out) {
    std::istringstream is(text);
    std::vector<std::string> tokens;
    for (std::string tok; is >> tok;) tokens.push_back(tok);
    if (tokens.size() < 2 || tokens.size() > 3) return false;

    Direction dir;
    if (!parse_direction(tokens.back(), &dir)) return false;

    Coord first;
    if (!parse_coord(tokens[0], &first)) return false;

    Coord second = first;
    if (tokens.size() == 3 && !parse_coord(tokens[1], &second)) return false;

    for (const Move& m : legal) {
        if (m.dir != dir) continue;
        const std::vector<Coord> ms = m.marbles();
        const Coord a = ms.front();
        const Coord b = ms.back();
        const bool endpoints_match = (a == first && b == second) || (a == second && b == first);
        if (!endpoints_match) continue;
        // A 2-token command names one marble, so reject longer lines.
        if (tokens.size() == 2 && m.count != 1) continue;
        *out = m;
        return true;
    }
    return false;
}

}  // namespace abalone
