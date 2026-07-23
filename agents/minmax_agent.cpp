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
        ctx.submit(pos.legal.front(), 0);

        // Statistics. A real search separates these two: count_node() for every
        // position you touch, count_eval() only where you run your heuristic.
        // Pruning shows up as nodes rising while evals stays flat.
        ctx.count_node(pos.legal.size());
        ctx.count_eval(1);

        // A searching agent would loop here, deepening and re-submitting, and
        // bail out when ctx.deadline_passed() turns true. Random has nothing
        // to think about, so it just returns.

        float best_score = NEG_INFINITE;
        auto best_move = pos.legal.front();
        for (int i = 1; i < MAX_DEPTH; i++){
            for (const abalone::Move& m : pos.legal) {
                abalone::Board next = pos.board;
                abalone::apply_move(&next, pos.to_move, m);
                const float score = -search(next, abalone::other(pos.to_move), i - 1, ctx);

                if (score > best_score) {
                    best_score = score;
                    best_move = m;

                    // Submit as we improve, with the score attached so the UI can
                    // show what this move was worth to us.
                    ctx.submit(best_move, best_score);
                }
            }
        }

    }

private:
    const int MAX_DEPTH = 4;

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

    float evaluate(const abalone::Board& board, const abalone::Player& p) const {
        int own_losses = board.losses(p);
        if (own_losses == 6){
            return NEG_INFINITE;
        }

        int enemy_losses = board.losses(abalone::other(p));
        if (enemy_losses == 6){
            return POS_INFINITE;
        }
        
        float puntuation = 0;
        
         int own_marbles = board.marbles(p);
        int enemy_marbles = board.marbles(abalone::other(p));

        float marble_count_puntuation = own_marbles - enemy_marbles; // [-5, 5] -> 10
        marble_count_puntuation = (marble_count_puntuation + 5) / 10.0;

        // Nº of Arrows
        int own_arrows = abalone::arrows(board, p);
        int enemy_arrows = abalone::arrows(board, abalone::other(p));

        float arrows_puntuation = own_arrows - enemy_arrows;
        arrows_puntuation = (arrows_puntuation + 16) / 32.0; // [-16, 16] -> 32

        // Nº of Edge Marbles
        int own_edge = abalone::edge_marbles(board, p);
        int enemy_edge = abalone::edge_marbles(board, abalone::other(p));
        
        float edge_puntuation = enemy_edge - own_edge;
        edge_puntuation = (edge_puntuation + 14) / 28.0;  // [-14, 14] -> 28

        puntuation +=
        5 * marble_count_puntuation + 
        1.5 * arrows_puntuation +
        3.5 * edge_puntuation;
        
        return puntuation;// Own vs Enemy marbles
    }
};

}  // namespace

REGISTER_AGENT(MinMaxAgent);
