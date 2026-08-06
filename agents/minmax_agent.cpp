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
#include "abalone/game.hpp"
#include "move_ordering.hpp"
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

        roots_.reset(pos.legal);
        MAX_PLIES = pos.move_limit.value_or(std::numeric_limits<int>::max());
        int root_ply = pos.move_number;

        for (int depth = 1; depth <= MAX_DEPTH; depth++){
            roots_.clear_scores();

            for (std::size_t idx = 0; idx < roots_.size(); idx++) {
                abalone::Board next = pos.board;
                abalone::apply_move(&next, pos.to_move, roots_.move(idx));
                const float score = -search(next, abalone::other(pos.to_move), depth - 1, root_ply + 1, ctx);
                if (ctx.deadline_passed()) break;
                roots_.set_score(idx, score);
            }

            if (ctx.deadline_passed()) break;   // pass incomplete -- do not reorder on it
            roots_.order_best_first();
            ctx.submit(roots_.best(), roots_.best_score());
        }

    }

private:
    const int MAX_DEPTH = 4;
    int MAX_PLIES;
    agents::RootMoves roots_;

    float search(const abalone::Board& board, abalone::Player p, int depth, const int current_ply, abalone::SearchContext& ctx) {
        // `depth <= 0`, not `== 0`: an off-by-one that lets depth go negative
        // turns this into unbounded recursion and a stack overflow.
        if (depth <= 0 || abalone::game_over(board)) {
            ctx.count_eval();               // leaf: the heuristic actually ran
            return evaluate(board, p, current_ply);
        }

        if (current_ply >= MAX_PLIES){
            if (abalone::result_by_count(board) == abalone::Result::kDraw) {
                 return 0;   // 0, or a small contempt bias
            }

            const bool i_won = (abalone::result_by_count(board) == abalone::Result::kBlackWins)
                            == (p == abalone::Player::kBlack);

            if (i_won) return POS_INFINITE;
            else return NEG_INFINITE;
        }

        auto moves = abalone::generate_moves(board, p);
        ctx.count_node(moves.size());       // positions this node put in front of us

        float best = NEG_INFINITE;
        for (const abalone::Move& m : moves) {
            if (ctx.deadline_passed()) break;
            abalone::Board next = board;
            abalone::apply_move(&next, p, m);
            best = std::max(best, -search(next, abalone::other(p), depth - 1, current_ply + 1, ctx));
        }
        return best;
    }

    float evaluate(const abalone::Board& board, const abalone::Player& p, const int current_ply) const {
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
        marble_count_puntuation = marble_count_puntuation / 5.0;

        // Nº of Arrows
        int own_arrows = abalone::arrows(board, p);
        int enemy_arrows = abalone::arrows(board, abalone::other(p));

        float arrows_puntuation = own_arrows - enemy_arrows;
        arrows_puntuation = arrows_puntuation/ 16.0; // [-16, 16] -> 32

        // Nº of Edge Marbles
        int own_edge = abalone::edge_marbles(board, p);
        int enemy_edge = abalone::edge_marbles(board, abalone::other(p));
        
        float edge_puntuation = enemy_edge - own_edge;
        edge_puntuation = edge_puntuation / 14.0;  // [-14, 14] -> 28

        int remaining_plies = MAX_PLIES - current_ply;
        float urgency_factor = 1.0;
        float urgency = 1 + urgency_factor * (1 - static_cast<float>(remaining_plies) / MAX_PLIES); 

        puntuation +=
        5 * marble_count_puntuation * urgency + 
        1.5 * arrows_puntuation +
        3.5 * edge_puntuation;
        
        return puntuation;// Own vs Enemy marbles
    }
};

}  // namespace

REGISTER_AGENT(MinMaxAgent);
