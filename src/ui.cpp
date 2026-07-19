#include "abalone/ui.hpp"

#include <iomanip>
#include <ostream>

namespace abalone {
namespace {

char glyph(Cell c) {
    switch (c) {
        case Cell::kBlack: return '@';
        case Cell::kWhite: return 'o';
        case Cell::kEmpty: return '.';
        default: return ' ';
    }
}

int row_length(int row) {
    static constexpr int kLen[kRows] = {5, 6, 7, 8, 9, 8, 7, 6, 5};
    return kLen[row];
}

}  // namespace

void render_board(std::ostream& os, const Board& board) {
    os << '\n';
    // Top row first, so the board reads the way it sits on a table.
    for (int row = kRows - 1; row >= 0; --row) {
        const int len = row_length(row);
        os << std::string(static_cast<std::size_t>(kRows - len), ' ');
        os << static_cast<char>('A' + row) << "  ";

        for (int i = 0; i < len; ++i) {
            Coord c;
            parse_coord(std::string(1, static_cast<char>('A' + row)) +
                            static_cast<char>('1' + i),
                        &c);
            os << glyph(board.at(c)) << ' ';
        }
        os << '\n';
    }
    os << '\n';
}

void render_status(std::ostream& os, const Game& game) {
    const Board& b = game.board();
    os << "Black @  lost " << b.losses(Player::kBlack) << "/" << kMarblesToLose
       << "   |   White o  lost " << b.losses(Player::kWhite) << "/" << kMarblesToLose << '\n';
    os << "Ply " << game.ply();
    if (game.config().move_limit) os << " / " << *game.config().move_limit;
    os << "   |   " << player_name(game.to_move()) << " to move\n";
}

void render_move_report(std::ostream& os, Player mover, const MoveReport& report) {
    os << player_name(mover) << ": " << report.move.to_string()
       << "   [" << report.elapsed.count() << " ms";
    if (report.nodes > 0 || report.evals > 0) {
        os << ", " << report.nodes << " nodes, " << report.evals << " evals";
        if (report.evals > 0 && report.nodes >= report.evals) {
            os << " (" << std::fixed << std::setprecision(1)
               << static_cast<double>(report.nodes) / static_cast<double>(report.evals)
               << "x pruned)";
        }
    }
    os << "]";
    if (report.timed_out) os << "  <time limit reached>";
    if (report.forfeited) os << "  *** FORFEIT: no move submitted, engine chose one ***";
    os << '\n';
}

}  // namespace abalone
