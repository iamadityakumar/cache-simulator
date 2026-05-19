#include "policies.h"
#include <stdexcept>

// ============================================================================
// LRUPolicy
// ============================================================================

LRUPolicy::LRUPolicy(size_t capacity) : capacity_(capacity) {
    if (capacity_ == 0) throw std::invalid_argument("LRUPolicy capacity must be > 0");
}

bool LRUPolicy::access(uint64_t tag) {
    auto it = map_.find(tag);
    if (it == map_.end()) return false;  // miss

    // Hit: move to front (MRU position)
    order_.splice(order_.begin(), order_, it->second);
    return true;
}

std::optional<uint64_t> LRUPolicy::insert(uint64_t tag) {
    // If already present, just move to front
    if (access(tag)) return std::nullopt;

    std::optional<uint64_t> evicted = std::nullopt;

    // Evict LRU (back) if full
    if (order_.size() >= capacity_) {
        uint64_t victim = order_.back();
        order_.pop_back();
        map_.erase(victim);
        evicted = victim;
    }

    // Insert at front (MRU)
    order_.push_front(tag);
    map_[tag] = order_.begin();
    return evicted;
}

bool LRUPolicy::contains(uint64_t tag) const noexcept {
    return map_.find(tag) != map_.end();
}

void LRUPolicy::remove(uint64_t tag) {
    auto it = map_.find(tag);
    if (it != map_.end()) {
        order_.erase(it->second);
        map_.erase(it);
    }
}

size_t LRUPolicy::size() const noexcept {
    return order_.size();
}

// ============================================================================
// FIFOPolicy
// ============================================================================

FIFOPolicy::FIFOPolicy(size_t capacity) : capacity_(capacity) {
    if (capacity_ == 0) throw std::invalid_argument("FIFOPolicy capacity must be > 0");
}

bool FIFOPolicy::access(uint64_t tag) {
    // FIFO does NOT reorder on hit — just check presence
    return map_.find(tag) != map_.end();
}

std::optional<uint64_t> FIFOPolicy::insert(uint64_t tag) {
    // If already present, do nothing (FIFO: no reorder)
    if (contains(tag)) return std::nullopt;

    std::optional<uint64_t> evicted = std::nullopt;

    // Evict oldest (back) if full
    if (order_.size() >= capacity_) {
        uint64_t victim = order_.back();
        order_.pop_back();
        map_.erase(victim);
        evicted = victim;
    }

    // Insert at front (newest)
    order_.push_front(tag);
    map_[tag] = order_.begin();
    return evicted;
}

bool FIFOPolicy::contains(uint64_t tag) const noexcept {
    return map_.find(tag) != map_.end();
}

void FIFOPolicy::remove(uint64_t tag) {
    auto it = map_.find(tag);
    if (it != map_.end()) {
        order_.erase(it->second);
        map_.erase(it);
    }
}

size_t FIFOPolicy::size() const noexcept {
    return order_.size();
}

// ============================================================================
// Factory
// ============================================================================

std::unique_ptr<ReplacementPolicy> make_policy(const std::string& name, size_t capacity) {
    if (name == "lru" || name == "LRU") {
        return std::make_unique<LRUPolicy>(capacity);
    } else if (name == "fifo" || name == "FIFO") {
        return std::make_unique<FIFOPolicy>(capacity);
    }
    throw std::invalid_argument("Unknown replacement policy: " + name);
}
