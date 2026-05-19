#pragma once

#include "cache_line.h"
#include "policies.h"
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

// Represents a single cache set (one row in a set-associative cache).
// Contains up to `associativity` cache lines, managed by a ReplacementPolicy.
class CacheSet {
public:
    // Construct a set with given associativity and replacement policy name.
    CacheSet(size_t associativity, const std::string& policy_name);

    // Lookup a tag in this set. Returns true on hit, false on miss.
    // On hit, updates the replacement policy state (e.g., LRU reorder).
    bool lookup(uint64_t tag);

    // Insert a tag into the set. If the set is full, evicts according to policy.
    // Returns the evicted tag if eviction occurred, std::nullopt otherwise.
    // Also marks the cache line as valid.
    std::optional<uint64_t> insert(uint64_t tag, bool is_write);

    // Check if a tag is present in this set (does NOT update policy state).
    [[nodiscard]] bool contains(uint64_t tag) const;

    // Mark a line as dirty (for write-back tracking).
    void mark_dirty(uint64_t tag);

    // Check if a line is dirty.
    bool is_dirty(uint64_t tag) const;

    // Remove a tag from the set (used for invalidation).
    void invalidate(uint64_t tag);

    // Number of valid lines in the set.
    size_t occupancy() const;

    // Max capacity of this set.
    size_t capacity() const noexcept { return associativity_; }

private:
    size_t associativity_;
    std::unique_ptr<ReplacementPolicy> policy_;
    std::unordered_map<uint64_t, CacheLine> lines_;  // tag -> CacheLine
};
