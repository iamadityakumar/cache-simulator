#include "cache_set.h"

CacheSet::CacheSet(size_t associativity, const std::string& policy_name)
    : associativity_(associativity)
    , policy_(make_policy(policy_name, associativity))
{}

bool CacheSet::lookup(uint64_t tag) {
    return policy_->access(tag);
}

std::optional<uint64_t> CacheSet::insert(uint64_t tag, bool is_write) {
    // If already present, just update policy (access) and dirty bit
    if (policy_->contains(tag)) {
        policy_->access(tag);
        if (is_write) lines_[tag].dirty = true;
        return std::nullopt;
    }

    // Insert into policy — may trigger eviction
    auto evicted_tag = policy_->insert(tag);

    // If eviction happened, remove the evicted line
    if (evicted_tag.has_value()) {
        lines_.erase(evicted_tag.value());
    }

    // Create new cache line
    CacheLine line;
    line.tag   = tag;
    line.valid = true;
    line.dirty = is_write;
    lines_[tag] = line;

    return evicted_tag;
}

bool CacheSet::contains(uint64_t tag) const {
    return policy_->contains(tag);
}

void CacheSet::mark_dirty(uint64_t tag) {
    auto it = lines_.find(tag);
    if (it != lines_.end()) {
        it->second.dirty = true;
    }
}

bool CacheSet::is_dirty(uint64_t tag) const {
    auto it = lines_.find(tag);
    if (it != lines_.end()) {
        return it->second.dirty;
    }
    return false;
}

void CacheSet::invalidate(uint64_t tag) {
    policy_->remove(tag);
    lines_.erase(tag);
}

size_t CacheSet::occupancy() const {
    return policy_->size();
}
