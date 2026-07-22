#include "abalone/agent.hpp"

#include <algorithm>

namespace abalone {

void SearchContext::begin(std::optional<std::chrono::milliseconds> limit) {
    std::lock_guard<std::mutex> lock(mu_);
    best_.reset();
    score_.reset();
    nodes_.store(0, std::memory_order_relaxed);
    evals_.store(0, std::memory_order_relaxed);
    start_ = Clock::now();
    deadline_ = limit ? std::optional<Clock::time_point>(start_ + *limit) : std::nullopt;
}

void SearchContext::submit(const Move& move) {
    std::lock_guard<std::mutex> lock(mu_);
    best_ = move;
    // A move submitted without a score clears the previous one rather than
    // leaving a stale number attached to a different move.
    score_.reset();
}

void SearchContext::submit(const Move& move, double score) {
    std::lock_guard<std::mutex> lock(mu_);
    best_ = move;
    score_ = score;
}

std::optional<double> SearchContext::score() const {
    std::lock_guard<std::mutex> lock(mu_);
    return score_;
}

std::optional<Move> SearchContext::best() const {
    std::lock_guard<std::mutex> lock(mu_);
    return best_;
}

bool SearchContext::deadline_passed() const {
    std::lock_guard<std::mutex> lock(mu_);
    return deadline_ && Clock::now() >= *deadline_;
}

std::optional<std::chrono::milliseconds> SearchContext::time_left() const {
    std::lock_guard<std::mutex> lock(mu_);
    if (!deadline_) return std::nullopt;
    const auto remaining = *deadline_ - Clock::now();
    if (remaining <= Clock::duration::zero()) return std::chrono::milliseconds(0);
    return std::chrono::duration_cast<std::chrono::milliseconds>(remaining);
}

AgentRegistry& AgentRegistry::instance() {
    static AgentRegistry registry;
    return registry;
}

void AgentRegistry::add(std::string name, std::string description, AgentFactory factory) {
    entries_.push_back(AgentEntry{std::move(name), std::move(description), factory});
    std::sort(entries_.begin(), entries_.end(),
              [](const AgentEntry& a, const AgentEntry& b) { return a.name < b.name; });
}

const AgentEntry* AgentRegistry::find(const std::string& name) const {
    for (const AgentEntry& e : entries_) {
        if (e.name == name) return &e;
    }
    return nullptr;
}

}  // namespace abalone
