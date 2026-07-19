#include "abalone/game.hpp"

#include <future>
#include <thread>

namespace abalone {
namespace {

// Agents that were still searching when their clock expired. We cannot safely
// kill a thread mid-search, so the thread is detached and its context kept
// alive here until it finishes on its own. Without this the worker would be
// writing into a destroyed SearchContext.
struct Abandoned {
    std::shared_ptr<Agent> agent;
    std::shared_ptr<SearchContext> ctx;
    std::shared_ptr<std::vector<Move>> legal;
    std::shared_ptr<Board> board;
};

std::vector<Abandoned>& abandoned_searches() {
    static std::vector<Abandoned> v;
    return v;
}

}  // namespace

const char* result_name(Result r) {
    switch (r) {
        case Result::kOngoing: return "ongoing";
        case Result::kBlackWins: return "Black wins";
        case Result::kWhiteWins: return "White wins";
        case Result::kDraw: return "draw";
    }
    return "?";
}

Game::Game(GameConfig config)
    : config_(config), board_(Board::from_opening(config.opening)) {}

std::vector<Move> Game::legal_moves() const {
    return generate_moves(board_, to_move_);
}

Result Game::result() const {
    if (board_.losses(Player::kBlack) >= kMarblesToLose) return Result::kWhiteWins;
    if (board_.losses(Player::kWhite) >= kMarblesToLose) return Result::kBlackWins;
    if (config_.move_limit && ply_ >= *config_.move_limit) return Result::kDraw;
    return Result::kOngoing;
}

void Game::play(const Move& move, const MoveReport& report) {
    apply_move(&board_, to_move_, move);
    history_.push_back(report);
    to_move_ = other(to_move_);
    ++ply_;
}

MoveReport Game::play_agent_turn(const std::shared_ptr<Agent>& agent) {
    // Everything the worker touches is shared_ptr-owned, so an overrunning
    // search still has valid memory to write into after we walk away.
    auto legal = std::make_shared<std::vector<Move>>(legal_moves());
    auto snapshot = std::make_shared<Board>(board_);
    auto ctx = std::make_shared<SearchContext>();

    const Position pos{*snapshot, to_move_, ply_, *legal};
    ctx->begin(config_.time_per_move);

    const auto started = Clock::now();

    // `done` is signalled by the worker; we wait on it rather than joining so
    // that a runaway agent cannot hold the game hostage.
    auto done = std::make_shared<std::promise<void>>();
    std::future<void> finished = done->get_future();

    std::thread worker([agent, ctx, legal, snapshot, done, pos]() {
        agent->choose_move(pos, *ctx);
        done->set_value();
    });

    bool timed_out = false;
    if (config_.time_per_move) {
        if (finished.wait_for(*config_.time_per_move) != std::future_status::ready) {
            timed_out = true;
        }
    } else {
        finished.wait();
    }

    if (timed_out) {
        worker.detach();
        abandoned_searches().push_back(Abandoned{agent, ctx, legal, snapshot});
    } else {
        worker.join();
    }

    MoveReport report;
    report.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - started);
    report.nodes = ctx->nodes();
    report.evals = ctx->evals();
    report.timed_out = timed_out;

    if (const std::optional<Move> best = ctx->best()) {
        report.move = *best;
    } else {
        // Nothing was submitted in time. Play a legal move so the game can
        // continue, and flag it loudly -- this is an agent bug.
        report.move = legal->front();
        report.forfeited = true;
    }

    play(report.move, report);
    return report;
}

}  // namespace abalone
