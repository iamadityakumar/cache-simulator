#include "cache_level.h"
#include <stdexcept>

constexpr bool CacheLevel::is_power_of_2(size_t v) noexcept {
    return v > 0 && (v & (v - 1)) == 0;
}

constexpr size_t CacheLevel::log2_int(size_t v) noexcept {
    size_t r = 0;
    while (v >>= 1) r++;
    return r;
}

CacheLevel::CacheLevel(const std::string& name,
                         size_t total_size,
                         size_t block_size,
                         size_t associativity,
                         uint64_t hit_latency,
                         const std::string& policy_name)
    : name_(name)
    , total_size_(total_size)
    , block_size_(block_size)
    , associativity_(associativity)
    , hit_latency_(hit_latency)
{
    if (!is_power_of_2(block_size))
        throw std::invalid_argument(name + ": block_size must be a power of 2");
    if (!is_power_of_2(associativity))
        throw std::invalid_argument(name + ": associativity must be a power of 2");

    num_sets_ = total_size / (block_size * associativity);
    if (num_sets_ == 0)
        throw std::invalid_argument(name + ": cache too small for given block_size and associativity");
    if (!is_power_of_2(num_sets_))
        throw std::invalid_argument(name + ": total_size must result in a power-of-2 number of sets");

    offset_bits_ = log2_int(block_size);
    index_bits_  = log2_int(num_sets_);

    // Initialize all sets
    sets_.reserve(num_sets_);
    for (size_t i = 0; i < num_sets_; i++) {
        sets_.emplace_back(associativity, policy_name);
    }
}

uint64_t CacheLevel::get_block_address(uint64_t address) const {
    return address >> offset_bits_;
}

uint64_t CacheLevel::get_tag(uint64_t address) const {
    return address >> (offset_bits_ + index_bits_);
}

size_t CacheLevel::get_set_index(uint64_t address) const {
    uint64_t block_addr = address >> offset_bits_;
    return static_cast<size_t>(block_addr & ((1ULL << index_bits_) - 1));
}

AccessResult CacheLevel::lookup(uint64_t address) {
    size_t idx   = get_set_index(address);
    uint64_t tag = get_tag(address);

    bool hit = sets_[idx].lookup(tag);

    AccessResult result;
    result.hit     = hit;
    result.latency = hit ? hit_latency_ : 0;  // 0 latency on miss (will be added by lower level)
    result.evicted_tag = std::nullopt;
    return result;
}

std::optional<uint64_t> CacheLevel::fill(uint64_t address, bool is_write) {
    size_t idx   = get_set_index(address);
    uint64_t tag = get_tag(address);

    auto evicted_tag = sets_[idx].insert(tag, is_write);

    // Convert evicted tag back to a block address for the caller
    if (evicted_tag.has_value()) {
        // Reconstruct block address: (tag << index_bits) | set_index
        uint64_t block_addr = (evicted_tag.value() << index_bits_) | idx;
        // Convert back to byte address
        return block_addr << offset_bits_;
    }
    return std::nullopt;
}

bool CacheLevel::contains(uint64_t address) const {
    size_t idx   = get_set_index(address);
    uint64_t tag = get_tag(address);
    return sets_[idx].contains(tag);
}

void CacheLevel::invalidate(uint64_t address) {
    size_t idx   = get_set_index(address);
    uint64_t tag = get_tag(address);
    sets_[idx].invalidate(tag);
}
