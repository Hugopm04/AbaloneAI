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
    past_.push_back(board_);
    apply_move(&board_, to_move_, move);
    history_.push_back(report);
    to_move_ = other(to_move_);
    ++ply_;
}

bool Game::undo() {
    if (past_.empty()) return false;
    board_ = past_.back();
    past_.pop_back();
    history_.pop_back();
    to_move_ = other(to_move_);
    --ply_;
    return true;
}

// One agent turn in flight. Everything the worker touches is shared_ptr-owned,
// so an overrunning search still has valid memory to write into after we walk
// away from it.
struct Game::PendingTurn {
    std::shared_ptr<Agent> agent;
    std::shared_ptr<SearchContext> ctx;
    std::shared_ptr<std::vector<Move>> legal;
    std::shared_ptr<Board> snapshot;
    std::thread worker;
    std::future<void> finished;
    Clock::time_point started{};
    std::optional<Clock::time_point> deadline;
};

Game::~Game() {
    // A search still running at teardown outlives us on a detached thread; its
    // context is kept alive by abandoned_searches().
    if (pending_) {
        pending_->worker.detach();
        abandoned_searches().push_back(
            Abandoned{pending_->agent, pending_->ctx, pending_->legal, pending_->snapshot});
    }
}

void Game::begin_agent_turn(const std::shared_ptr<Agent>& agent) {
    auto p = std::make_unique<PendingTurn>();
    p->agent = agent;
    p->legal = std::make_shared<std::vector<Move>>(legal_moves());
    p->snapshot = std::make_shared<Board>(board_);
    p->ctx = std::make_shared<SearchContext>();
    p->ctx->begin(config_.time_per_move);
    p->started = Clock::now();
    if (config_.time_per_move) p->deadline = p->started + *config_.time_per_move;

    const Position pos{*p->snapshot, to_move_, ply_, *p->legal};

    // `done` is signalled by the worker; we wait on it rather than joining so
    // that a runaway agent cannot hold the game hostage.
    auto done = std::make_shared<std::promise<void>>();
    p->finished = done->get_future();
    p->worker = std::thread(
        [agent, ctx = p->ctx, legal = p->legal, snapshot = p->snapshot, done, pos]() {
            agent->choose_move(pos, *ctx);
            done->set_value();
        });

    pending_ = std::move(p);
}

bool Game::agent_turn_ready() const {
    if (!pending_) return false;
    if (pending_->finished.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        return true;
    }
    return pending_->deadline && Clock::now() >= *pending_->deadline;
}

MoveReport Game::finish_agent_turn() {
    std::unique_ptr<PendingTurn> p = std::move(pending_);

    const bool done_on_time =
        p->finished.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
    if (done_on_time) {
        p->worker.join();
    } else {
        p->worker.detach();
        abandoned_searches().push_back(Abandoned{p->agent, p->ctx, p->legal, p->snapshot});
    }

    MoveReport report;
    report.elapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - p->started);
    report.nodes = p->ctx->nodes();
    report.evals = p->ctx->evals();
    report.timed_out = !done_on_time;
    report.score = p->ctx->score();

    if (const std::optional<Move> best = p->ctx->best()) {
        report.move = *best;
    } else {
        // Nothing was submitted in time. Play a legal move so the game can
        // continue, and flag it loudly -- this is an agent bug.
        report.move = p->legal->front();
        report.forfeited = true;
    }

    play(report.move, report);
    return report;
}

MoveReport Game::play_agent_turn(const std::shared_ptr<Agent>& agent) {
    begin_agent_turn(agent);
    if (pending_->deadline) {
        pending_->finished.wait_until(*pending_->deadline);
    } else {
        pending_->finished.wait();
    }
    return finish_agent_turn();
}

}  // namespace abalone
