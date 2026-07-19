#include "abalone/arena.hpp"

#include <iomanip>
#include <ostream>

namespace abalone {

ArenaResult run_match(const AgentEntry& black_entry, const AgentEntry& white_entry,
                      const GameConfig& config, int games, std::ostream* progress) {
    ArenaResult out;
    out.black_name = black_entry.name;
    out.white_name = white_entry.name;

    for (int i = 0; i < games; ++i) {
        // Fresh agents each game so nothing leaks between them.
        std::shared_ptr<Agent> black = black_entry.factory();
        std::shared_ptr<Agent> white = white_entry.factory();
        black->on_game_start(Player::kBlack);
        white->on_game_start(Player::kWhite);

        Game game(config);
        while (!game.over()) {
            const Player mover = game.to_move();
            const MoveReport r =
                game.play_agent_turn(mover == Player::kBlack ? black : white);

            Stats& s = (mover == Player::kBlack) ? out.black : out.white;
            s.total_time += r.elapsed;
            s.nodes += r.nodes;
            s.evals += r.evals;
            ++s.moves;
            if (r.timed_out) ++s.timeouts;
            if (r.forfeited) ++s.forfeits;
        }

        switch (game.result()) {
            case Result::kBlackWins: ++out.black_wins; break;
            case Result::kWhiteWins: ++out.white_wins; break;
            default: ++out.draws; break;
        }
        out.total_plies += game.ply();

        if (progress) {
            *progress << "  game " << (i + 1) << "/" << games << ": "
                      << result_name(game.result()) << " in " << game.ply() << " plies\n";
        }
    }

    return out;
}

void render_arena_result(std::ostream& os, const ArenaResult& r) {
    const int games = r.black_wins + r.white_wins + r.draws;
    if (games == 0) return;

    os << "\n=== " << r.black_name << " (Black) vs " << r.white_name << " (White) ===\n";
    os << games << " games\n";
    os << "  " << r.black_name << " wins: " << r.black_wins << "  ("
       << std::fixed << std::setprecision(1)
       << 100.0 * r.black_wins / games << "%)\n";
    os << "  " << r.white_name << " wins: " << r.white_wins << "  ("
       << 100.0 * r.white_wins / games << "%)\n";
    os << "  draws: " << r.draws << "\n";
    os << "  average game length: " << (r.total_plies / games) << " plies\n";

    const auto line = [&os](const std::string& who, const Stats& s) {
        if (s.moves == 0) return;
        os << "  " << who << ": " << (s.total_time.count() / s.moves) << " ms/move, "
           << (s.nodes / s.moves) << " nodes/move, " << (s.evals / s.moves) << " evals/move";
        if (s.evals > 0) {
            os << ", " << std::fixed << std::setprecision(1)
               << static_cast<double>(s.nodes) / static_cast<double>(s.evals) << "x pruned";
        }
        if (s.timeouts > 0) os << ", " << s.timeouts << " timeouts";
        if (s.forfeits > 0) os << ", *** " << s.forfeits << " FORFEITS ***";
        os << "\n";
    };
    line(r.black_name + " (B)", r.black);
    line(r.white_name + " (W)", r.white);
}

}  // namespace abalone
