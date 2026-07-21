// ---------------------------------------------------------------------------
// MinMax Agent
// ---------------------------------------------------------------------------
//
// Everything an agent needs is here:
//
//   1. Subclass Agent.
//   2. Implement name() and choose_move().
//   3. REGISTER_AGENT(YourType) at the bottom.
//   4. Add the file to AGENT_SOURCES in CMakeLists.txt.
//
// It then shows up in the main menu and in the arena with no further wiring.

#include <random>

#include "abalone/agent.hpp"

namespace {

class RandomAgent : public abalone::Agent {
public:
    std::string name() const override { return "minmax"; }

    std::string description() const override {
        return "Picks a uniformly random legal move. The baseline every other agent should beat.";
    }

    // Called once per game. Reseed here so repeated games are not identical.
    void on_game_start(abalone::Player /*seat*/) override {
        rng_.seed(std::random_device{}());
    }

    void choose_move(const abalone::Position& pos, abalone::SearchContext& ctx) override {
        // Submit something immediately. If your search is ever cut off before
        // it submits, the engine has to play a fallback move for you and flags
        // the turn as forfeited -- so always publish a legal move up front.
        std::uniform_int_distribution<std::size_t> pick(0, pos.legal.size() - 1);
        const abalone::Move& choice = pos.legal[pick(rng_)];
        ctx.submit(choice);

        // Statistics. A real search separates these two: count_node() for every
        // position you touch, count_eval() only where you run your heuristic.
        // Pruning shows up as nodes rising while evals stays flat.
        ctx.count_node(pos.legal.size());
        ctx.count_eval(1);

        // A searching agent would loop here, deepening and re-submitting, and
        // bail out when ctx.deadline_passed() turns true. Random has nothing
        // to think about, so it just returns.
    }

private:
    std::mt19937 rng_{std::random_device{}()};
};

}  // namespace

REGISTER_AGENT(RandomAgent);
