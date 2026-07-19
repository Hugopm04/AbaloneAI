#pragma once

#include <chrono>
#include <cstdint>
#include <iosfwd>
#include <string>

#include "abalone/agent.hpp"
#include "abalone/game.hpp"

namespace abalone {

// Aggregated cost of one agent across a match.
struct Stats {
    std::chrono::milliseconds total_time{0};
    std::uint64_t nodes = 0;
    std::uint64_t evals = 0;
    int moves = 0;
    int timeouts = 0;  // clock expired mid-search (normal for timed searches)
    int forfeits = 0;  // nothing submitted in time (always an agent bug)
};

struct ArenaResult {
    std::string black_name;
    std::string white_name;
    int black_wins = 0;
    int white_wins = 0;
    int draws = 0;
    int total_plies = 0;
    Stats black;
    Stats white;
};

// Plays `games` headless games. Pass a stream for per-game progress, or
// nullptr to stay quiet.
ArenaResult run_match(const AgentEntry& black, const AgentEntry& white,
                      const GameConfig& config, int games,
                      std::ostream* progress = nullptr);

void render_arena_result(std::ostream& os, const ArenaResult& result);

}  // namespace abalone
