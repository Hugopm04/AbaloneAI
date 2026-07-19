#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <string>

#include "abalone/arena.hpp"
#include "abalone/game.hpp"
#include "abalone/ui.hpp"

namespace {

using namespace abalone;

GameConfig g_config;  // menu-adjustable settings, shared by all game modes

std::string prompt(const std::string& question) {
    std::cout << question;
    std::cout.flush();
    std::string line;
    if (!std::getline(std::cin, line)) return {};
    return line;
}

int prompt_int(const std::string& question, int fallback) {
    const std::string line = prompt(question);
    if (line.empty()) return fallback;
    try {
        return std::stoi(line);
    } catch (...) {
        return fallback;
    }
}

// --- agent selection --------------------------------------------------------

const AgentEntry* choose_agent(const std::string& role) {
    const auto& all = AgentRegistry::instance().all();
    if (all.empty()) {
        std::cout << "No agents are registered. See agents/random_agent.cpp.\n";
        return nullptr;
    }

    std::cout << "\nChoose the AI for " << role << ":\n";
    for (std::size_t i = 0; i < all.size(); ++i) {
        std::cout << "  " << (i + 1) << ") " << all[i].name;
        if (!all[i].description.empty()) std::cout << "  -- " << all[i].description;
        std::cout << '\n';
    }

    const int pick = prompt_int("> ", 1);
    if (pick < 1 || pick > static_cast<int>(all.size())) return &all[0];
    return &all[static_cast<std::size_t>(pick) - 1];
}

// --- human input ------------------------------------------------------------

// Returns false if the player asked to quit.
bool read_human_move(const Game& game, const std::vector<Move>& legal, Move* out) {
    for (;;) {
        std::cout << "\nYour move (e.g. \"C3 C5 NE\", or \"E4 W\" for one marble)\n"
                     "  [l]ist moves, [q]uit\n> ";
        std::string line;
        if (!std::getline(std::cin, line)) return false;
        if (line == "q" || line == "quit") return false;

        if (line == "l" || line == "list") {
            for (std::size_t i = 0; i < legal.size(); ++i) {
                std::cout << "  " << legal[i].to_string() << '\n';
            }
            std::cout << "  (" << legal.size() << " legal moves)\n";
            continue;
        }

        if (parse_move(legal, line, out)) return true;
        std::cout << "Not a legal move. Type 'l' to see what is available.\n";
        (void)game;
    }
}

// --- game modes -------------------------------------------------------------

// `black` / `white` may be null, meaning a human plays that seat.
void play_game(std::shared_ptr<Agent> black, std::shared_ptr<Agent> white) {
    Game game(g_config);
    if (black) black->on_game_start(Player::kBlack);
    if (white) white->on_game_start(Player::kWhite);

    while (!game.over()) {
        render_board(std::cout, game.board());
        render_status(std::cout, game);
        const Player mover = game.to_move();
        const std::shared_ptr<Agent>& agent = (mover == Player::kBlack) ? black : white;

        if (agent) {
            std::cout << "\n" << player_name(mover) << " (" << agent->name() << ") thinking...\n";
            const MoveReport r = game.play_agent_turn(agent);
            render_move_report(std::cout, mover, r);
        } else {
            const std::vector<Move> legal = game.legal_moves();
            Move chosen;
            if (!read_human_move(game, legal, &chosen)) {
                std::cout << "Game abandoned.\n";
                return;
            }
            MoveReport r;
            r.move = chosen;
            game.play(chosen, r);
        }
    }

    render_board(std::cout, game.board());
    std::cout << "\n*** " << result_name(game.result()) << " *** after " << game.ply()
              << " plies.\n";
}

void mode_arena() {
    const AgentEntry* black = choose_agent("Black");
    if (!black) return;
    const AgentEntry* white = choose_agent("White");
    if (!white) return;

    const int games = prompt_int("How many games? [10] ", 10);
    std::cout << "\nRunning " << games << " games...\n";
    const ArenaResult r = run_match(*black, *white, g_config, games, &std::cout);
    render_arena_result(std::cout, r);
}

void mode_settings() {
    for (;;) {
        std::cout << "\n--- Settings ---\n";
        std::cout << "  Opening:        " << opening_name(g_config.opening) << '\n';
        std::cout << "  Move limit:     "
                  << (g_config.move_limit ? std::to_string(*g_config.move_limit) + " plies"
                                          : std::string("none (uncapped)"))
                  << '\n';
        std::cout << "  Time per move:  "
                  << (g_config.time_per_move
                          ? std::to_string(g_config.time_per_move->count()) + " ms"
                          : std::string("none (unlimited)"))
                  << '\n';
        std::cout << "\n  1) Change opening\n  2) Change move limit\n"
                     "  3) Change time per move\n  0) Back\n> ";

        std::string line;
        if (!std::getline(std::cin, line)) return;

        if (line == "0" || line.empty()) return;
        if (line == "1") {
            const std::string which =
                prompt("Opening -- [1] Classic, [2] Belgian Daisy: ");
            Opening o;
            if (parse_opening(which, &o)) g_config.opening = o;
        } else if (line == "2") {
            const std::string v = prompt("Move limit in plies (0 or blank = uncapped): ");
            const int n = v.empty() ? 0 : std::atoi(v.c_str());
            g_config.move_limit = (n > 0) ? std::optional<int>(n) : std::nullopt;
        } else if (line == "3") {
            const std::string v = prompt("Milliseconds per move (0 or blank = unlimited): ");
            const int n = v.empty() ? 0 : std::atoi(v.c_str());
            g_config.time_per_move =
                (n > 0) ? std::optional<std::chrono::milliseconds>(std::chrono::milliseconds(n))
                        : std::nullopt;
        }
    }
}

void main_menu() {
    for (;;) {
        std::cout << "\n=========================\n"
                     "   ABALONE\n"
                     "=========================\n"
                     "  1) Human vs Human (local)\n"
                     "  2) Human vs AI\n"
                     "  3) AI vs AI (watch one game)\n"
                     "  4) Arena (batch AI vs AI, statistics)\n"
                     "  5) Settings\n"
                     "  0) Quit\n> ";

        std::string line;
        if (!std::getline(std::cin, line)) return;

        if (line == "0" || line == "q") return;
        if (line == "1") {
            play_game(nullptr, nullptr);
        } else if (line == "2") {
            const std::string seat = prompt("Play as [1] Black (first) or [2] White? ");
            const AgentEntry* ai = choose_agent("your opponent");
            if (!ai) continue;
            if (seat == "2") {
                play_game(ai->factory(), nullptr);
            } else {
                play_game(nullptr, ai->factory());
            }
        } else if (line == "3") {
            const AgentEntry* black = choose_agent("Black");
            const AgentEntry* white = black ? choose_agent("White") : nullptr;
            if (black && white) play_game(black->factory(), white->factory());
        } else if (line == "4") {
            mode_arena();
        } else if (line == "5") {
            mode_settings();
        }
    }
}

}  // namespace

int main() {
    std::cout << "Abalone engine + AI workspace\n";
    main_menu();
    return 0;
}
