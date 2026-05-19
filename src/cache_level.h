#pragma once

#include "cache_set.h"
#include <cstdint>
#include <string>
#include <vector>

// Result of a single cache access at one level.
struct AccessResult {
    bool     hit;          // True if tag was found
    uint64_t latency;      // Cycles consumed at this level (hit_latency if hit)
    std::optional<uint64_t> evicted_tag;  // Full block address evicted, if any
};

// Represents one level of cache (e.g., L1, L2, or L3).
// Internally composed of multiple CacheSets (set-associative).
class CacheLevel {
public:
    // Construct a cache level.
    //   name:          "L1", "L2", "L3" (for reporting)
    //   total_size:    total cache size in bytes
    //   block_size:    bytes per cache line (must be power of 2)
    //   associativity: number of ways (must be power of 2)
    //   hit_latency:   cycles for a hit at this level
    //   policy_name:   "lru" or "fifo"
    CacheLevel(const std::string& name,
               size_t total_size,
               size_t block_size,
               size_t associativity,
               uint64_t hit_latency,
               const std::string& policy_name);

    // Perform a lookup (read or write access) for the given byte address.
    // Returns AccessResult indicating hit/miss. If miss, does NOT insert.
    AccessResult lookup(uint64_t address);

    // Insert a block into the cache (called on fill after miss at higher level).
    // Returns the evicted block address if eviction occurred.
    std::optional<uint64_t> fill(uint64_t address, bool is_write);

    // Check if a block address is present in the cache.
    [[nodiscard]] bool contains(uint64_t address) const;

    // Invalidate a block address.
    void invalidate(uint64_t address);

    // Getters
    const std::string& name() const noexcept { return name_; }
    size_t total_size() const noexcept { return total_size_; }
    size_t block_size() const noexcept { return block_size_; }
    size_t associativity() const noexcept { return associativity_; }
    size_t num_sets() const noexcept { return num_sets_; }
    uint64_t hit_latency() const noexcept { return hit_latency_; }

    // Address decomposition helpers
    uint64_t get_tag(uint64_t address) const;
    size_t   get_set_index(uint64_t address) const;
    uint64_t get_block_address(uint64_t address) const;

private:
    std::string name_;
    size_t total_size_;
    size_t block_size_;
    size_t associativity_;
    size_t num_sets_;
    uint64_t hit_latency_;

    // Bit widths for address decomposition
    size_t offset_bits_;   // log2(block_size)
    size_t index_bits_;    // log2(num_sets)

    std::vector<CacheSet> sets_;

    // Validate that value is a power of 2
    static constexpr bool is_power_of_2(size_t v) noexcept;
    static constexpr size_t log2_int(size_t v) noexcept;
};
