#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "abalone/board.hpp"
#include "abalone/move.hpp"

namespace abalone {

using Clock = std::chrono::steady_clock;

// ---------------------------------------------------------------------------
// SearchContext
// ---------------------------------------------------------------------------
//
// Handed to an agent for the duration of one move. It carries the deadline,
// collects search statistics, and receives the agent's answer.
//
// Timing is enforced by the engine, not by you: choose_move() runs on a worker
// thread and the engine takes whatever you last submit()ed once the clock runs
// out. You cannot overrun a time limit even by accident. The one obligation
// that carries is that anything you touch after the deadline must be owned by
// your agent -- the engine has already moved on.
//
// Statistics are deliberately split in two:
//
//   count_node()  -- a position you generated or stepped into during search
//   count_eval()  -- a position you actually ran a heuristic on
//
// Effective pruning drives these apart: nodes keeps climbing while evals stays
// flat. The ratio between them is the number worth watching when you start
// cutting branches.

class SearchContext {
public:
    SearchContext() = default;

    // --- called by the agent ---

    // Publishes the best move found so far. Call it early with any legal move,
    // then again each time your search improves on it.
    void submit(const Move& move);

    // True once the time limit has elapsed. Unlimited searches never see true.
    bool deadline_passed() const;

    // Remaining time; nullopt when the search is untimed.
    std::optional<std::chrono::milliseconds> time_left() const;

    void count_node(std::uint64_t n = 1) { nodes_.fetch_add(n, std::memory_order_relaxed); }
    void count_eval(std::uint64_t n = 1) { evals_.fetch_add(n, std::memory_order_relaxed); }

    // --- called by the engine ---

    void begin(std::optional<std::chrono::milliseconds> limit);
    std::optional<Move> best() const;
    std::uint64_t nodes() const { return nodes_.load(std::memory_order_relaxed); }
    std::uint64_t evals() const { return evals_.load(std::memory_order_relaxed); }

private:
    mutable std::mutex mu_;
    std::optional<Move> best_;

    std::atomic<std::uint64_t> nodes_{0};
    std::atomic<std::uint64_t> evals_{0};

    Clock::time_point start_{};
    std::optional<Clock::time_point> deadline_;
};

// ---------------------------------------------------------------------------
// Agent
// ---------------------------------------------------------------------------
//
// To write an AI: subclass Agent, implement name() and choose_move(), and
// register it (see agents/random_agent.cpp for a complete worked example).
// Registered agents appear in the main menu and in the arena automatically.

struct Position {
    const Board& board;
    Player to_move;
    int move_number = 0;                  // plies played so far
    const std::vector<Move>& legal;       // pre-generated, never empty

    // Marbles still on the board, from the perspective of the side to move.
    // O(1): the board already tracks marbles pushed off, so these are just
    // the starting count minus that -- no scanning of the 61 cells.
    int own_marbles() const { return kMarblesPerPlayer - own_losses(); }
    int enemy_marbles() const { return kMarblesPerPlayer - enemy_losses(); }

    // Marbles pushed off the edge. 6 loses the game.
    int own_losses() const { return board.losses(to_move); }
    int enemy_losses() const { return board.losses(other(to_move)); }
};

class Agent {
public:
    virtual ~Agent() = default;

    virtual std::string name() const = 0;
    virtual std::string description() const { return {}; }

    // Called once at the start of each game. Reset any per-game state here.
    virtual void on_game_start(Player /*seat*/) {}

    // Pick a move. Submit early and often via ctx.submit() -- whatever was
    // submitted last is what gets played when the clock expires.
    virtual void choose_move(const Position& pos, SearchContext& ctx) = 0;
};

// ---------------------------------------------------------------------------
// Registry
// ---------------------------------------------------------------------------

using AgentFactory = std::unique_ptr<Agent> (*)();

struct AgentEntry {
    std::string name;
    std::string description;
    AgentFactory factory;
};

class AgentRegistry {
public:
    static AgentRegistry& instance();

    void add(std::string name, std::string description, AgentFactory factory);
    const std::vector<AgentEntry>& all() const { return entries_; }
    const AgentEntry* find(const std::string& name) const;

private:
    std::vector<AgentEntry> entries_;
};

// Drop this at file scope in your agent's .cpp to register it:
//
//   REGISTER_AGENT(MyAgent);
//
#define REGISTER_AGENT(Type)                                                   \
    namespace {                                                                \
    struct Type##Registrar {                                                   \
        Type##Registrar() {                                                    \
            Type sample;                                                       \
            ::abalone::AgentRegistry::instance().add(                          \
                sample.name(), sample.description(),                           \
                []() -> std::unique_ptr<::abalone::Agent> {                    \
                    return std::make_unique<Type>();                           \
                });                                                            \
        }                                                                      \
    } g_##Type##_registrar;                                                    \
    }

}  // namespace abalone
