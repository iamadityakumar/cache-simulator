#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Per-level statistics snapshot.
struct LevelStats {
    std::string name;           // "L1", "L2", "L3", "Memory"
    uint64_t    hits       = 0;
    uint64_t    misses     = 0;
    uint64_t    accesses   = 0;  // hits + misses
    uint64_t    latency    = 0;  // hit latency for this level (cycles)

    // Computed values
    double hit_rate() const {
        return accesses > 0 ? static_cast<double>(hits) / accesses : 0.0;
    }

    double miss_rate() const {
        return accesses > 0 ? static_cast<double>(misses) / accesses : 0.0;
    }
};

// Collects statistics across all cache levels and computes AMAT.
class Statistics {
public:
    // Initialize with level names and their hit latencies.
    //   level_names:    e.g., {"L1", "L2", "L3"}
    //   hit_latencies:  e.g., {1, 10, 40}
    //   mem_latency:    main memory access latency in cycles
    Statistics(const std::vector<std::string>& level_names,
               const std::vector<uint64_t>& hit_latencies,
               uint64_t mem_latency);

    // Record a hit at the given level index (0 = L1, 1 = L2, ...).
    void record_hit(size_t level_index);

    // Record a miss at the given level index.
    void record_miss(size_t level_index);

    // Get stats for a specific level.
    const LevelStats& level(size_t index) const;

    // Get all level stats.
    const std::vector<LevelStats>& all_levels() const;

    // Number of cache levels (not counting main memory).
    size_t num_levels() const;

    // Total accesses driven through the hierarchy (= L1 accesses).
    uint64_t total_accesses() const;

    // Compute Average Memory Access Time (AMAT).
    // AMAT = L1_hit_time + L1_miss_rate * (L2_hit_time + L2_miss_rate * (L3_hit_time + L3_miss_rate * mem_latency))
    double amat() const;

    // Total accumulated latency across all accesses.
    uint64_t total_latency() const;

    // Average latency per access.
    double avg_latency() const;

    // Main memory latency.
    uint64_t mem_latency() const { return mem_latency_; }

    // Reset all counters.
    void reset();

private:
    std::vector<LevelStats> levels_;
    uint64_t mem_latency_;
    uint64_t total_latency_ = 0;  // sum of per-access latencies
};
