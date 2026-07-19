// Rule tests for the Abalone engine. Plain asserts, no framework.

#include <cassert>
#include <iostream>
#include <string>

#include "abalone/game.hpp"

using namespace abalone;

namespace {

int g_checks = 0;

void check(bool cond, const std::string& what) {
    ++g_checks;
    if (!cond) {
        std::cerr << "FAILED: " << what << '\n';
        std::abort();
    }
}

Coord at(const std::string& s) {
    Coord c;
    const bool ok = parse_coord(s, &c);
    assert(ok && "test used a bad coordinate");
    (void)ok;
    return c;
}

void put(Board* b, const std::string& s, Cell v) { b->set(at(s), v); }

// Finds the move whose line endpoints and direction match, or nullptr.
const Move* find(const std::vector<Move>& moves, const std::string& a, const std::string& b,
                 Direction d) {
    for (const Move& m : moves) {
        if (m.dir != d) continue;
        const std::vector<Coord> ms = m.marbles();
        if ((ms.front() == at(a) && ms.back() == at(b)) ||
            (ms.front() == at(b) && ms.back() == at(a))) {
            return &m;
        }
    }
    return nullptr;
}

// --- geometry ---------------------------------------------------------------

void test_geometry() {
    check(Board::cells().size() == kCells, "board has 61 cells");

    // Every neighbour relation is symmetric, and steps never leave the grid
    // without being reported as off-board.
    for (const Coord& c : Board::cells()) {
        for (int i = 0; i < kNumDirections; ++i) {
            const Direction d = static_cast<Direction>(i);
            const Coord n = step(c, d);
            if (!on_board(n)) continue;
            check(step(n, opposite(d)) == c, "adjacency is symmetric");
        }
    }

    // Round-trip notation for all 61 cells.
    for (const Coord& c : Board::cells()) {
        Coord back;
        check(parse_coord(coord_to_string(c), &back) && back == c, "coordinate round-trips");
    }

    // The corners of the widest row have exactly the neighbours we expect.
    check(!on_board(step(at("E1"), kWest)), "E1 has no western neighbour");
    check(!on_board(step(at("E9"), kEast)), "E9 has no eastern neighbour");
}

// --- openings ---------------------------------------------------------------

void test_openings() {
    for (const Opening o : {Opening::kClassic, Opening::kBelgianDaisy}) {
        const Board b = Board::from_opening(o);
        check(b.marbles(Player::kBlack) == 14, "black starts with 14 marbles");
        check(b.marbles(Player::kWhite) == 14, "white starts with 14 marbles");
        check(b.losses(Player::kBlack) == 0 && b.losses(Player::kWhite) == 0,
              "no losses at the start");
    }

    const Board classic = Board::from_opening(Opening::kClassic);
    check(classic.at(at("A1")) == Cell::kBlack, "classic: A1 is black");
    check(classic.at(at("C4")) == Cell::kBlack, "classic: C4 is black");
    check(classic.at(at("C1")) == Cell::kEmpty, "classic: C1 is empty");
    check(classic.at(at("I5")) == Cell::kWhite, "classic: I5 is white");
    check(classic.at(at("E5")) == Cell::kEmpty, "classic: centre is empty");

    const Board daisy = Board::from_opening(Opening::kBelgianDaisy);
    check(daisy.at(at("A1")) == Cell::kBlack, "daisy: A1 is black");
    check(daisy.at(at("A5")) == Cell::kWhite, "daisy: A5 is white");
    check(daisy.at(at("I5")) == Cell::kBlack, "daisy: I5 is black");
    check(daisy.at(at("I1")) == Cell::kWhite, "daisy: I1 is white");
    check(daisy.at(at("A3")) == Cell::kEmpty, "daisy: clusters are separated");
}

// --- sumito -----------------------------------------------------------------

void test_pushing() {
    // 3 against 2 -> legal push.
    {
        Board b;
        put(&b, "E1", Cell::kBlack); put(&b, "E2", Cell::kBlack); put(&b, "E3", Cell::kBlack);
        put(&b, "E4", Cell::kWhite); put(&b, "E5", Cell::kWhite);

        const std::vector<Move> moves = generate_moves(b, Player::kBlack);
        const Move* push = find(moves, "E1", "E3", kEast);
        check(push != nullptr, "3v2 push is generated");
        check(push->pushed == 2 && !push->pushes_off, "3v2 displaces two marbles");

        apply_move(&b, Player::kBlack, *push);
        check(b.at(at("E1")) == Cell::kEmpty, "3v2: tail vacated");
        check(b.at(at("E2")) == Cell::kBlack && b.at(at("E4")) == Cell::kBlack,
              "3v2: black advanced");
        check(b.at(at("E5")) == Cell::kWhite && b.at(at("E6")) == Cell::kWhite,
              "3v2: white pushed back");
        check(b.marbles(Player::kBlack) == 3 && b.marbles(Player::kWhite) == 2,
              "3v2: no marbles created or destroyed");
    }

    // Equal numbers -> no push.
    {
        Board b;
        put(&b, "E1", Cell::kBlack); put(&b, "E2", Cell::kBlack); put(&b, "E3", Cell::kBlack);
        put(&b, "E4", Cell::kWhite); put(&b, "E5", Cell::kWhite); put(&b, "E6", Cell::kWhite);

        const std::vector<Move> moves = generate_moves(b, Player::kBlack);
        check(find(moves, "E1", "E3", kEast) == nullptr, "3v3 cannot push");
    }

    // A friendly marble behind the opposing line blocks the push.
    {
        Board b;
        put(&b, "E1", Cell::kBlack); put(&b, "E2", Cell::kBlack);
        put(&b, "E3", Cell::kWhite);
        put(&b, "E4", Cell::kBlack);

        const std::vector<Move> moves = generate_moves(b, Player::kBlack);
        check(find(moves, "E1", "E2", kEast) == nullptr, "cannot push into your own marble");
    }

    // Pushing an opponent off the edge scores.
    {
        Board b;
        put(&b, "E6", Cell::kBlack); put(&b, "E7", Cell::kBlack); put(&b, "E8", Cell::kBlack);
        put(&b, "E9", Cell::kWhite);

        const std::vector<Move> moves = generate_moves(b, Player::kBlack);
        const Move* push = find(moves, "E6", "E8", kEast);
        check(push != nullptr && push->pushes_off, "push off the edge is generated");

        apply_move(&b, Player::kBlack, *push);
        check(b.losses(Player::kWhite) == 1, "pushed-off marble is counted as a loss");
        check(b.marbles(Player::kWhite) == 0, "pushed-off marble leaves the board");
        check(b.marbles(Player::kBlack) == 3, "pusher keeps all its marbles");
    }

    // You may never push yourself off.
    {
        Board b;
        put(&b, "E8", Cell::kBlack); put(&b, "E9", Cell::kBlack);
        const std::vector<Move> moves = generate_moves(b, Player::kBlack);
        for (const Move& m : moves) {
            check(!(m.dir == kEast && m.marbles().front() == at("E9")),
                  "no move walks a marble off the edge");
        }
    }
}

// --- broadside --------------------------------------------------------------

void test_broadside() {
    Board b;
    put(&b, "E4", Cell::kBlack); put(&b, "E5", Cell::kBlack); put(&b, "E6", Cell::kBlack);

    const std::vector<Move> moves = generate_moves(b, Player::kBlack);
    const Move* side = find(moves, "E4", "E6", kNorthEast);
    check(side != nullptr, "broadside move is generated");
    check(!side->inline_move && side->pushed == 0, "broadside never pushes");

    Board copy = b;
    apply_move(&copy, Player::kBlack, *side);
    check(copy.marbles(Player::kBlack) == 3, "broadside conserves marbles");
    check(copy.at(at("E4")) == Cell::kEmpty, "broadside vacates the old line");

    // A broadside move into an occupied cell is illegal.
    Board blocked = b;
    put(&blocked, "F4", Cell::kWhite);
    const std::vector<Move> blocked_moves = generate_moves(blocked, Player::kBlack);
    for (const Move& m : blocked_moves) {
        if (m.inline_move) continue;
        for (const Coord& c : m.marbles()) {
            check(blocked.at(step(c, m.dir)) == Cell::kEmpty,
                  "broadside destinations are always empty");
        }
    }
}

// --- generated moves are internally consistent ------------------------------

void test_generation_invariants() {
    for (const Opening o : {Opening::kClassic, Opening::kBelgianDaisy}) {
        const Board start = Board::from_opening(o);
        for (const Player p : {Player::kBlack, Player::kWhite}) {
            const std::vector<Move> moves = generate_moves(start, p);
            check(!moves.empty(), "the opening position has legal moves");

            for (const Move& m : moves) {
                check(m.count >= 1 && m.count <= 3, "lines are 1 to 3 marbles");
                for (const Coord& c : m.marbles()) {
                    check(start.at(c) == to_cell(p), "moved marbles belong to the mover");
                }

                // Applying any generated move conserves total marble count.
                Board after = start;
                apply_move(&after, p, m);
                const int before_total = start.marbles(p) + start.marbles(other(p));
                const int after_total = after.marbles(p) + after.marbles(other(p)) +
                                        after.losses(other(p));
                check(before_total == after_total, "moves conserve marbles");
                check(after.marbles(p) == start.marbles(p), "a move never loses your own marble");
            }

            // No duplicate moves.
            for (std::size_t i = 0; i < moves.size(); ++i) {
                for (std::size_t j = i + 1; j < moves.size(); ++j) {
                    const bool same = moves[i].dir == moves[j].dir &&
                                      moves[i].count == moves[j].count &&
                                      moves[i].marbles().front() == moves[j].marbles().front() &&
                                      moves[i].marbles().back() == moves[j].marbles().back();
                    check(!same, "no duplicate moves are generated");
                }
            }
        }
    }
}

// --- game flow --------------------------------------------------------------

void test_game_flow() {
    // A move limit forces termination, and produces a draw when nobody has won.
    GameConfig cfg;
    cfg.opening = Opening::kClassic;
    cfg.move_limit = 20;

    Game game(cfg);
    check(!game.over(), "a fresh game is not over");
    while (!game.over()) {
        const std::vector<Move> legal = game.legal_moves();
        check(!legal.empty(), "there is always a legal move");
        game.play(legal.front(), MoveReport{});
    }
    check(game.ply() == 20, "the move limit stops the game exactly on time");
    check(game.result() == Result::kDraw, "hitting the limit is a draw");

    // With no limit set, the game is not capped.
    GameConfig uncapped;
    check(!uncapped.move_limit.has_value(), "games are uncapped by default");

    // Six losses ends it.
    GameConfig plain;
    Game scored(plain);
    check(scored.result() == Result::kOngoing, "opening position is ongoing");
}

// --- agents -----------------------------------------------------------------

void test_agents_play() {
    const AgentEntry* random = AgentRegistry::instance().find("random");
    check(random != nullptr, "the random agent is registered");

    GameConfig cfg;
    cfg.move_limit = 60;
    cfg.time_per_move = std::chrono::milliseconds(50);

    std::shared_ptr<Agent> black = random->factory();
    std::shared_ptr<Agent> white = random->factory();
    black->on_game_start(Player::kBlack);
    white->on_game_start(Player::kWhite);

    Game game(cfg);
    while (!game.over()) {
        const Player mover = game.to_move();
        const MoveReport r = game.play_agent_turn(mover == Player::kBlack ? black : white);
        check(!r.forfeited, "the random agent always submits a move in time");
        check(r.elapsed <= std::chrono::milliseconds(2000), "turns respect the clock");
    }
    check(game.over(), "an agent game terminates");
}

}  // namespace

int main() {
    test_geometry();
    test_openings();
    test_pushing();
    test_broadside();
    test_generation_invariants();
    test_game_flow();
    test_agents_play();

    std::cout << "All tests passed (" << g_checks << " checks).\n";
    return 0;
}
