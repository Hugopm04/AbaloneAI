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

#include "abalone/agent.hpp"
#include <limits>

namespace {

const float POS_INFINITE = std::numeric_limits<float>::max();
const float NEG_INFINITE = -POS_INFINITE;

class MinMaxAgent : public abalone::Agent {
public:
    std::string name() const override { return "minmax"; }

    std::string description() const override {
        return "Basic MinMax Algorithm";
    }

    // Called once per game. Reseed here so repeated games are not identical.
    void on_game_start(abalone::Player /*seat*/) override {
        
    }

    void choose_move(const abalone::Position& pos, abalone::SearchContext& ctx) override {
        // Submit something immediately. If your search is ever cut off before
        // it submits, the engine has to play a fallback move for you and flags
        // the turn as forfeited -- so always publish a legal move up front.
        ctx.submit(pos.legal.front());

        // Statistics. A real search separates these two: count_node() for every
        // position you touch, count_eval() only where you run your heuristic.
        // Pruning shows up as nodes rising while evals stays flat.
        ctx.count_node(pos.legal.size());
        ctx.count_eval(1);

        // A searching agent would loop here, deepening and re-submitting, and
        // bail out when ctx.deadline_passed() turns true. Random has nothing
        // to think about, so it just returns.

        for (int depth = 1; depth < MAX_DEPTH; ++depth) {
            std::optional<Move> best = search(pos, depth, ctx);
            if (ctx.deadline_passed()) break;   // depth incomplete, discard it
            if (best) ctx.submit(*best);        // only submit completed depths
        }
    }

private:
    const int MAX_DEPTH = 5;

    float search(const abalone::Board& board, abalone::Player p, int depth, abalone::SearchContext& ctx) {
        if (depth == 0 || abalone::game_over(board)) {
            ctx.count_eval();               // leaf: the heuristic actually ran
            return evaluate(board, p);
        }

        auto moves = abalone::generate_moves(board, p);
        ctx.count_node(moves.size());       // positions this node put in front of us

        float best = NEG_INFINITE;
        for (const abalone::Move& m : moves) {
            if (ctx.deadline_passed()) break;
            abalone::Board next = board;
            abalone::apply_move(&next, p, m);
            best = std::max(best, -search(next, abalone::other(p), depth - 1, ctx));
        }
        return best;
    }

    float evaluate(const Board& board, const abalone::Position& pos) const {
        int own_losses = pos.own_losses();
        if (own_losses == 6){
            return NEG_INFINITE;
        }

        int enemy_losses = pos.enemy_losses();
        if (enemy_losses == 6){
            return POS_INFINITE;
        }
        
        float puntuation = 0;

        int own_marbles = pos.own_marbles();
        int enemy_marbles = pos.enemy_marbles();

        puntuation = own_marbles - enemy_marbles;
        
        return puntuation;
    }
};

}  // namespace

REGISTER_AGENT(RandomAgent);
