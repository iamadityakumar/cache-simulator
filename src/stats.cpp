#include "stats.h"
#include <stdexcept>

Statistics::Statistics(const std::vector<std::string>& level_names,
                       const std::vector<uint64_t>& hit_latencies,
                       uint64_t mem_latency)
    : mem_latency_(mem_latency)
{
    if (level_names.size() != hit_latencies.size()) {
        throw std::invalid_argument("Statistics: level_names and hit_latencies must have same size");
    }
    if (level_names.empty()) {
        throw std::invalid_argument("Statistics: must have at least one cache level");
    }

    levels_.resize(level_names.size());
    for (size_t i = 0; i < level_names.size(); i++) {
        levels_[i].name    = level_names[i];
        levels_[i].latency = hit_latencies[i];
    }
}

void Statistics::record_hit(size_t level_index) {
    if (level_index >= levels_.size()) {
        throw std::out_of_range("Statistics::record_hit: level index out of range");
    }
    levels_[level_index].hits++;
    levels_[level_index].accesses++;
}

void Statistics::record_miss(size_t level_index) {
    if (level_index >= levels_.size()) {
        throw std::out_of_range("Statistics::record_miss: level index out of range");
    }
    levels_[level_index].misses++;
    levels_[level_index].accesses++;
}

const LevelStats& Statistics::level(size_t index) const {
    if (index >= levels_.size()) {
        throw std::out_of_range("Statistics::level: index out of range");
    }
    return levels_[index];
}

const std::vector<LevelStats>& Statistics::all_levels() const {
    return levels_;
}

size_t Statistics::num_levels() const {
    return levels_.size();
}

uint64_t Statistics::total_accesses() const {
    // Total accesses = L1 accesses (everything enters through L1)
    return levels_.empty() ? 0 : levels_[0].accesses;
}

double Statistics::amat() const {
    // AMAT = L1_hit_time + L1_miss_rate * (L2_hit_time + L2_miss_rate * (...))
    // Recursive from innermost level outward:
    //   cost(last) = latency[last] + miss_rate[last] * mem_latency
    //   cost(i)    = latency[i]    + miss_rate[i]    * cost(i+1)

    if (levels_.empty()) return static_cast<double>(mem_latency_);

    // Start from the innermost cache level
    double cost = static_cast<double>(mem_latency_);

    for (int i = static_cast<int>(levels_.size()) - 1; i >= 0; i--) {
        double miss_rate = levels_[i].miss_rate();
        double hit_time  = static_cast<double>(levels_[i].latency);
        cost = hit_time + miss_rate * cost;
    }

    return cost;
}

uint64_t Statistics::total_latency() const {
    return total_latency_;
}

double Statistics::avg_latency() const {
    uint64_t accesses = total_accesses();
    return accesses > 0 ? static_cast<double>(total_latency_) / accesses : 0.0;
}

void Statistics::reset() {
    for (auto& l : levels_) {
        l.hits     = 0;
        l.misses   = 0;
        l.accesses = 0;
    }
    total_latency_ = 0;
}
