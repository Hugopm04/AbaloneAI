#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "abalone/arena.hpp"
#include "abalone/game.hpp"
#include "abalone/ui.hpp"

#ifdef ABALONE_GUI
#include "abalone/gui.hpp"
#endif

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

// --- command line -----------------------------------------------------------

void print_usage() {
    std::cout <<
        "Abalone engine + AI workspace\n\n"
        "Usage: abalone [options]\n\n"
        "  (no options)         graphical window\n"
        "  --tui                terminal UI instead of the window\n"
        "  --headless           batch AI-vs-AI games, no UI at all; prints statistics\n"
        "  -h, --help           this message\n\n"
        "Headless options (for training and benchmarking):\n"
        "  --black NAME         agent for Black   (default: first registered)\n"
        "  --white NAME         agent for White   (default: same as --black)\n"
        "  --games N            games to play     (default: 100)\n"
        "  --move-limit N       cap game length in plies (default: uncapped)\n"
        "  --time-per-move MS   per-move budget in ms (default: unlimited)\n"
        "  --opening NAME       classic | belgian  (default: classic)\n"
        "  --quiet              final statistics only, no per-game lines\n\n"
        "Registered agents:";
    if (AgentRegistry::instance().all().empty()) {
        std::cout << " (none)\n";
    } else {
        for (const AgentEntry& a : AgentRegistry::instance().all()) {
            std::cout << "\n  " << a.name;
        }
        std::cout << '\n';
    }
}

// Looks an agent up by name. Null (with a message) when it does not exist, so
// a typo in a training script fails loudly instead of silently benchmarking
// the wrong bot.
const AgentEntry* find_agent(const std::string& name) {
    for (const AgentEntry& a : AgentRegistry::instance().all()) {
        if (a.name == name) return &a;
    }
    std::cerr << "error: no agent named '" << name << "'. Use --help to list them.\n";
    return nullptr;
}

int run_headless(const std::vector<std::string>& args) {
    const auto& all = AgentRegistry::instance().all();
    if (all.empty()) {
        std::cerr << "error: no agents are registered; nothing to run.\n";
        return 1;
    }

    std::string black_name, white_name;
    int games = 100;
    bool quiet = false;

    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string& a = args[i];
        const bool has_value = (i + 1 < args.size());
        const std::string value = has_value ? args[i + 1] : std::string();

        auto need = [&](const char* what) {
            if (!has_value) std::cerr << "error: " << what << " needs a value.\n";
            return has_value;
        };

        if (a == "--headless" || a == "--quiet") {
            quiet = quiet || (a == "--quiet");
        } else if (a == "--black") {
            if (!need("--black")) return 1;
            black_name = value; ++i;
        } else if (a == "--white") {
            if (!need("--white")) return 1;
            white_name = value; ++i;
        } else if (a == "--games") {
            if (!need("--games")) return 1;
            games = std::atoi(value.c_str()); ++i;
        } else if (a == "--move-limit") {
            if (!need("--move-limit")) return 1;
            const int n = std::atoi(value.c_str());
            g_config.move_limit = (n > 0) ? std::optional<int>(n) : std::nullopt;
            ++i;
        } else if (a == "--time-per-move") {
            if (!need("--time-per-move")) return 1;
            const int n = std::atoi(value.c_str());
            g_config.time_per_move =
                (n > 0) ? std::optional<std::chrono::milliseconds>(std::chrono::milliseconds(n))
                        : std::nullopt;
            ++i;
        } else if (a == "--opening") {
            if (!need("--opening")) return 1;
            Opening o;
            if (!parse_opening(value, &o)) {
                std::cerr << "error: unknown opening '" << value << "'.\n";
                return 1;
            }
            g_config.opening = o;
            ++i;
        } else {
            std::cerr << "error: unrecognised option '" << a << "'. Use --help.\n";
            return 1;
        }
    }

    if (games <= 0) {
        std::cerr << "error: --games must be positive.\n";
        return 1;
    }

    const AgentEntry* black = black_name.empty() ? &all[0] : find_agent(black_name);
    if (!black) return 1;
    const AgentEntry* white = white_name.empty() ? black : find_agent(white_name);
    if (!white) return 1;

    const ArenaResult r =
        run_match(*black, *white, g_config, games, quiet ? nullptr : &std::cout);
    render_arena_result(std::cout, r);
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    const std::vector<std::string> args(argv + 1, argv + argc);

    for (const std::string& a : args) {
        if (a == "-h" || a == "--help") {
            print_usage();
            return 0;
        }
    }

    const bool headless =
        std::find(args.begin(), args.end(), "--headless") != args.end();
    const bool tui = std::find(args.begin(), args.end(), "--tui") != args.end();

    if (headless) return run_headless(args);

    if (!tui) {
#ifdef ABALONE_GUI
        return run_gui(g_config);
#else
        std::cout << "This build has no graphical UI (built with -DABALONE_GUI=OFF).\n"
                     "Falling back to the terminal UI.\n";
#endif
    }

    std::cout << "Abalone engine + AI workspace\n";
    main_menu();
    return 0;
}
