#include "memory_hierarchy.h"
#include <stdexcept>

// Helper to build a Statistics object from configs.
static Statistics build_stats(const std::vector<CacheLevelConfig>& configs,
                               uint64_t mem_latency) {
    std::vector<std::string> names;
    std::vector<uint64_t> latencies;
    names.reserve(configs.size());
    latencies.reserve(configs.size());
    for (const auto& c : configs) {
        names.push_back(c.name);
        latencies.push_back(c.hit_latency);
    }
    return Statistics(names, latencies, mem_latency);
}

MemoryHierarchy::MemoryHierarchy(const std::vector<CacheLevelConfig>& configs,
                                   uint64_t mem_latency)
    : configs_(configs)
    , mem_latency_(mem_latency)
    , stats_(build_stats(configs, mem_latency))
{
    if (configs.empty()) {
        throw std::invalid_argument("MemoryHierarchy: at least one cache level required");
    }

    levels_.reserve(configs.size());
    for (const auto& cfg : configs) {
        levels_.push_back(std::make_unique<CacheLevel>(
            cfg.name, cfg.total_size, cfg.block_size,
            cfg.associativity, cfg.hit_latency, cfg.policy_name));
    }
}

HierarchyAccessResult MemoryHierarchy::read(uint64_t address) {
    return do_access(address, false);
}

HierarchyAccessResult MemoryHierarchy::write(uint64_t address) {
    return do_access(address, true);
}

HierarchyAccessResult MemoryHierarchy::access(uint64_t address, char type) {
    if (type == 'W' || type == 'w') {
        return write(address);
    }
    return read(address);
}

const Statistics& MemoryHierarchy::stats() const {
    return stats_;
}

const CacheLevel& MemoryHierarchy::cache_level(size_t index) const {
    if (index >= levels_.size()) {
        throw std::out_of_range("MemoryHierarchy::cache_level: index out of range");
    }
    return *levels_[index];
}

size_t MemoryHierarchy::num_levels() const {
    return levels_.size();
}

void MemoryHierarchy::reset() {
    stats_.reset();

    // Rebuild all cache levels from scratch using stored configs.
    levels_.clear();
    levels_.reserve(configs_.size());
    for (const auto& cfg : configs_) {
        levels_.push_back(std::make_unique<CacheLevel>(
            cfg.name, cfg.total_size, cfg.block_size,
            cfg.associativity, cfg.hit_latency, cfg.policy_name));
    }
}

HierarchyAccessResult MemoryHierarchy::do_access(uint64_t address, bool is_write) {
    HierarchyAccessResult result;
    result.is_write      = is_write;
    result.total_latency = 0;
    result.hit           = false;
    result.hit_level     = levels_.size();  // default: main memory

    // Walk down the hierarchy looking for a hit.
    // For each level, we pay the hit_latency cost to perform the lookup.

    for (size_t i = 0; i < levels_.size(); i++) {
        auto ar = levels_[i]->lookup(address);

        if (ar.hit) {
            // Found it at level i
            result.hit       = true;
            result.hit_level = i;
            result.total_latency += ar.latency;  // hit latency at this level

            // Record hit at this level
            stats_.record_hit(i);

            // Fill all levels ABOVE this one (0 .. i-1) with the data.
            // Fill from the level just above the hit back up to L1.
            for (int j = static_cast<int>(i) - 1; j >= 0; j--) {
                levels_[j]->fill(address, is_write);
            }

            return result;
        } else {
            // Miss at level i — accumulate the lookup cost.
            // We pay the hit_latency to check this level before discovering the miss.
            result.total_latency += levels_[i]->hit_latency();

            // Record miss at this level
            stats_.record_miss(i);
        }
    }

    // Miss at all cache levels — fetch from main memory.
    result.total_latency += mem_latency_;

    // Fill on miss: propagate data back up through ALL cache levels.
    // Fill from the lowest cache level (closest to memory) up to L1.
    for (int i = static_cast<int>(levels_.size()) - 1; i >= 0; i--) {
        levels_[i]->fill(address, is_write);
    }

    return result;
}
