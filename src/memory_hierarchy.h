#pragma once

#include "cache_level.h"
#include "stats.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// Describes the configuration for one cache level.
struct CacheLevelConfig {
    std::string name;           // "L1", "L2", "L3"
    size_t      total_size;     // Total cache size in bytes
    size_t      block_size;     // Bytes per cache line
    size_t      associativity;  // Number of ways
    uint64_t    hit_latency;    // Cycles for a hit at this level
    std::string policy_name;    // "lru" or "fifo"
};

// Result of a single memory access through the entire hierarchy.
struct HierarchyAccessResult {
    bool     hit;               // True if found in any cache level (not main memory)
    size_t   hit_level;         // Level index where the data was found (0 = L1, etc.)
                                // If miss at all levels, equals num_levels (= main memory)
    uint64_t total_latency;     // Total cycle cost for this access
    bool     is_write;          // Whether this was a write access
};

// Orchestrates the multi-level cache hierarchy: L1 → L2 → L3 → Main Memory.
// Implements inclusive fill-on-miss:
//   - On miss at level i, check level i+1.
//   - On hit at level j (or main memory), fill all levels from j-1 down to 0.
class MemoryHierarchy {
public:
    // Construct from a list of cache level configs and main memory latency.
    MemoryHierarchy(const std::vector<CacheLevelConfig>& configs,
                    uint64_t mem_latency);

    // Perform a read access at the given byte address.
    HierarchyAccessResult read(uint64_t address);

    // Perform a write access at the given byte address.
    HierarchyAccessResult write(uint64_t address);

    // Access (generic): type = 'R' for read, 'W' for write.
    HierarchyAccessResult access(uint64_t address, char type);

    // Get the statistics collector (const reference).
    const Statistics& stats() const;

    // Get a const reference to a specific cache level.
    const CacheLevel& cache_level(size_t index) const;

    // Number of cache levels (not counting main memory).
    size_t num_levels() const;

    // Main memory latency.
    uint64_t mem_latency() const { return mem_latency_; }

    // Reset the entire hierarchy and statistics.
    void reset();

private:
    std::vector<CacheLevelConfig> configs_;
    std::vector<std::unique_ptr<CacheLevel>> levels_;
    uint64_t    mem_latency_;
    Statistics  stats_;

    // Core access logic shared by read/write.
    HierarchyAccessResult do_access(uint64_t address, bool is_write);
};
