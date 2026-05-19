#pragma once

#include <cstdint>
#include <list>
#include <optional>
#include <unordered_map>
#include <string>
#include <memory>

// Abstract base class for cache replacement policies.
// Manages ordering of tags within a single cache set.
class ReplacementPolicy {
public:
    virtual ~ReplacementPolicy() = default;

    // Called on every access. Returns true if tag is present (hit).
    // On hit, updates internal ordering (policy-dependent).
    virtual bool access(uint64_t tag) = 0;

    // Insert a tag. If the set is full, evicts and returns the evicted tag.
    // If not full, returns std::nullopt.
    virtual std::optional<uint64_t> insert(uint64_t tag) = 0;

    // Returns true if the tag is present.
    virtual bool contains(uint64_t tag) const noexcept = 0;

    // Remove a specific tag (used during invalidation / hierarchy management).
    virtual void remove(uint64_t tag) = 0;

    // Current number of entries.
    virtual size_t size() const noexcept = 0;

    // Policy name for reporting.
    virtual std::string name() const noexcept = 0;
};

// LRU (Least Recently Used) replacement policy.
// On hit: tag is moved to the front (most recent).
// On eviction: tag at the back (least recent) is evicted.
// Complexity: O(1) for access, insert, evict.
class LRUPolicy : public ReplacementPolicy {
public:
    explicit LRUPolicy(size_t capacity);

    bool access(uint64_t tag) override;
    std::optional<uint64_t> insert(uint64_t tag) override;
    bool contains(uint64_t tag) const noexcept override;
    void remove(uint64_t tag) override;
    size_t size() const noexcept override;
    std::string name() const noexcept override { return "LRU"; }

private:
    size_t capacity_;
    std::list<uint64_t> order_;                                        // front = MRU, back = LRU
    std::unordered_map<uint64_t, std::list<uint64_t>::iterator> map_;  // tag -> list position
};

// FIFO (First In, First Out) replacement policy.
// On hit: no reordering (arrival order preserved).
// On eviction: oldest inserted tag is evicted.
// Complexity: O(1) for access, insert, evict.
class FIFOPolicy : public ReplacementPolicy {
public:
    explicit FIFOPolicy(size_t capacity);

    bool access(uint64_t tag) override;
    std::optional<uint64_t> insert(uint64_t tag) override;
    bool contains(uint64_t tag) const noexcept override;
    void remove(uint64_t tag) override;
    size_t size() const noexcept override;
    std::string name() const noexcept override { return "FIFO"; }

private:
    size_t capacity_;
    std::list<uint64_t> order_;                                        // front = newest, back = oldest
    std::unordered_map<uint64_t, std::list<uint64_t>::iterator> map_;
};

// Factory: create policy by name string.
std::unique_ptr<ReplacementPolicy> make_policy(const std::string& name, size_t capacity);
