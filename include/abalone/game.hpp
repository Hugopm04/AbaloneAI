#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "abalone/agent.hpp"
#include "abalone/board.hpp"
#include "abalone/move.hpp"

namespace abalone {

inline constexpr int kMarblesToLose = 6;

enum class Result {
    kOngoing,
    kBlackWins,
    kWhiteWins,
    kDraw,  // only reachable when a move limit is set
};

const char* result_name(Result r);

struct GameConfig {
    Opening opening = Opening::kClassic;

    // Optional cap on game length, in plies. Unset means the game runs until
    // somebody loses six marbles, however long that takes.
    std::optional<int> move_limit;

    // Optional per-move thinking time. Unset means agents think as long as
    // they like -- useful when debugging, unwise in a tournament.
    std::optional<std::chrono::milliseconds> time_per_move;
};

// What one agent turn cost, recorded for every ply.
struct MoveReport {
    Move move{};
    std::chrono::milliseconds elapsed{0};
    std::uint64_t nodes = 0;
    std::uint64_t evals = 0;

    // The agent was still searching when the clock expired. Its last submitted
    // move was played. Not an error -- this is the expected shape of a
    // time-limited search.
    bool timed_out = false;

    // What the agent thought the position was worth after its move, if it
    // supplied a score to submit(). Unset when the agent never reported one.
    std::optional<double> score;

    // The agent submitted nothing before the deadline, so the engine played a
    // fallback move on its behalf. Always a bug in the agent.
    bool forfeited = false;
};

class Game {
public:
    explicit Game(GameConfig config);

    const Board& board() const { return board_; }
    Player to_move() const { return to_move_; }
    int ply() const { return ply_; }
    const GameConfig& config() const { return config_; }
    const std::vector<MoveReport>& history() const { return history_; }

    Result result() const;
    bool over() const { return result() != Result::kOngoing; }

    std::vector<Move> legal_moves() const;

    // Plays `move` and advances the turn.
    void play(const Move& move, const MoveReport& report);

    // Takes back the last ply. Undo is unlimited: every position since the
    // opening is kept, which costs a padded 9x9 grid per ply and nothing else.
    bool can_undo() const { return !past_.empty(); }
    bool undo();

    // Runs one agent turn under the configured time limit and plays the
    // result. The limit is enforced by the engine; an agent cannot exceed it.
    // Blocks until the agent is done, so it suits headless batch play.
    MoveReport play_agent_turn(const std::shared_ptr<Agent>& agent);

    // The same turn, split so a caller with a frame loop never blocks:
    //
    //   begin_agent_turn(agent);        // starts the search, returns at once
    //   while (!agent_turn_ready()) {}  // draw frames here instead
    //   finish_agent_turn();            // collects stats and plays the move
    //
    // The board is not touched until finish_agent_turn(), so it stays safe to
    // read and draw for the whole search.
    void begin_agent_turn(const std::shared_ptr<Agent>& agent);
    bool agent_turn_pending() const { return pending_ != nullptr; }
    bool agent_turn_ready() const;
    MoveReport finish_agent_turn();

    ~Game();

private:
    struct PendingTurn;
    std::unique_ptr<PendingTurn> pending_;

    GameConfig config_;
    Board board_;
    Player to_move_ = Player::kBlack;
    int ply_ = 0;
    std::vector<MoveReport> history_;
    std::vector<Board> past_;  // board before each played ply, for undo
};

}  // namespace abalone
