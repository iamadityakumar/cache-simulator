// test_edge_cases.cpp — Edge case tests for Phase 6
#include "test_framework.h"
#include "cache_level.h"
#include "cache_set.h"
#include "memory_hierarchy.h"
#include "stats.h"
#include "sweep.h"
#include "trace_loader.h"
#include <cmath>
#include <sstream>

// ============================================================================
// Cold Cache Edge Cases
// ============================================================================

TEST(EdgeCase_ColdCache_AllMisses) {
    // Every access to a unique address should be a cold miss.
    std::vector<CacheLevelConfig> levels = {
        {"L1", 256, 64, 2, 1, "lru"},
        {"L2", 1024, 64, 4, 10, "lru"},
        {"L3", 4096, 64, 8, 40, "lru"}
    };
    MemoryHierarchy mh(levels, 200);

    // 10 unique addresses, all far apart → all cold misses
    for (uint64_t i = 0; i < 10; i++) {
        auto r = mh.read(0x10000 + i * 0x1000);
        ASSERT_FALSE(r.hit);
        ASSERT_EQ(r.hit_level, (size_t)3);  // main memory
    }

    ASSERT_EQ(mh.stats().level(0).hits, (uint64_t)0);
    ASSERT_EQ(mh.stats().level(0).misses, (uint64_t)10);
}

TEST(EdgeCase_ColdCache_StatsCorrect) {
    // Cold cache: 100% miss rate, AMAT = worst case
    std::vector<CacheLevelConfig> levels = {
        {"L1", 256, 64, 2, 1, "lru"},
        {"L2", 1024, 64, 4, 10, "lru"},
        {"L3", 4096, 64, 8, 40, "lru"}
    };
    MemoryHierarchy mh(levels, 200);

    // Access unique addresses
    for (uint64_t i = 0; i < 5; i++) {
        mh.read(0x20000 + i * 0x2000);
    }

    const auto& stats = mh.stats();
    // L1: 0 hits, 5 misses → hit_rate = 0%
    ASSERT_TRUE(stats.level(0).hit_rate() < 0.001);
    ASSERT_TRUE(stats.level(0).miss_rate() > 0.999);

    // AMAT = 1 + 1*(10 + 1*(40 + 1*200)) = 251
    ASSERT_TRUE(std::abs(stats.amat() - 251.0) < 0.01);
}

// ============================================================================
// Single-Entry Cache (minimal configuration)
// ============================================================================

TEST(EdgeCase_SingleEntryCache) {
    // 1-way, 1-set cache: total_size = 64 bytes (one block)
    // Every new block evicts the previous one.
    CacheLevel cl("L1-tiny", 64, 64, 1, 1, "lru");
    ASSERT_EQ(cl.num_sets(), (size_t)1);

    cl.fill(0x1000, false);
    ASSERT_TRUE(cl.contains(0x1000));

    // Filling a new block evicts the old one
    cl.fill(0x2000, false);
    ASSERT_TRUE(cl.contains(0x2000));
    ASSERT_FALSE(cl.contains(0x1000));  // evicted

    // Again
    cl.fill(0x3000, false);
    ASSERT_TRUE(cl.contains(0x3000));
    ASSERT_FALSE(cl.contains(0x2000));  // evicted
}

TEST(EdgeCase_SingleEntryReuse) {
    // Single-entry cache with repeated access to the same address:
    // 1 cold miss + N-1 hits
    std::vector<CacheLevelConfig> levels = {
        {"L1", 64, 64, 1, 1, "lru"}
    };
    MemoryHierarchy mh(levels, 200);

    // First access: cold miss
    auto r1 = mh.read(0x1000);
    ASSERT_FALSE(r1.hit);

    // Next 9 accesses: all hits
    for (int i = 0; i < 9; i++) {
        auto r = mh.read(0x1000);
        ASSERT_TRUE(r.hit);
        ASSERT_EQ(r.hit_level, (size_t)0);
    }

    ASSERT_EQ(mh.stats().level(0).hits, (uint64_t)9);
    ASSERT_EQ(mh.stats().level(0).misses, (uint64_t)1);
}

// ============================================================================
// Power-of-2 Validation
// ============================================================================

TEST(EdgeCase_PowerOf2_BlockSize) {
    // Non-power-of-2 block size should throw
    bool caught = false;
    try {
        CacheLevel cl("bad", 256, 48, 1, 1, "lru");  // 48 is not power of 2
    } catch (const std::invalid_argument&) {
        caught = true;
    }
    ASSERT_TRUE(caught);
}

TEST(EdgeCase_PowerOf2_Assoc) {
    // Non-power-of-2 associativity should throw
    bool caught = false;
    try {
        CacheLevel cl("bad", 256, 64, 3, 1, "lru");  // 3-way is not power of 2
    } catch (const std::invalid_argument&) {
        caught = true;
    }
    ASSERT_TRUE(caught);
}

TEST(EdgeCase_PowerOf2_NumSets) {
    // Config that results in non-power-of-2 number of sets should throw.
    // total_size=192, block_size=64, assoc=1 → num_sets=3 (not power of 2)
    bool caught = false;
    try {
        CacheLevel cl("bad", 192, 64, 1, 1, "lru");
    } catch (const std::invalid_argument&) {
        caught = true;
    }
    ASSERT_TRUE(caught);
}

TEST(EdgeCase_ZeroAssoc) {
    // Zero associativity should throw (from policy constructor: capacity=0)
    bool caught = false;
    try {
        CacheLevel cl("bad", 256, 64, 0, 1, "lru");
    } catch (const std::invalid_argument&) {
        caught = true;
    }
    ASSERT_TRUE(caught);
}

// ============================================================================
// Hierarchy Reset
// ============================================================================

TEST(EdgeCase_HierarchyReset) {
    std::vector<CacheLevelConfig> levels = {
        {"L1", 256, 64, 2, 1, "lru"},
        {"L2", 1024, 64, 4, 10, "lru"}
    };
    MemoryHierarchy mh(levels, 200);

    // Accumulate some stats
    mh.read(0x1000);
    mh.read(0x1000);
    mh.write(0x2000);

    ASSERT_EQ(mh.stats().total_accesses(), (uint64_t)3);

    // Reset
    mh.reset();

    ASSERT_EQ(mh.stats().total_accesses(), (uint64_t)0);
    ASSERT_EQ(mh.stats().level(0).hits, (uint64_t)0);
    ASSERT_EQ(mh.stats().level(0).misses, (uint64_t)0);

    // After reset, previously cached data should be gone
    auto r = mh.read(0x1000);
    ASSERT_FALSE(r.hit);  // cold miss again
}

// ============================================================================
// Sweep with Empty Trace
// ============================================================================

TEST(EdgeCase_EmptyTrace_Sweep) {
    SweepConfig sc;
    sc.l1_sizes        = {32 * 1024};
    sc.l2_sizes        = {256 * 1024};
    sc.l3_sizes        = {4 * 1024 * 1024};
    sc.policies        = {"lru"};
    sc.associativities = {8};

    SimConfig base;
    std::vector<TraceEntry> empty_trace;  // no entries

    auto results = run_sweep(sc, base, empty_trace);

    ASSERT_EQ(results.size(), (size_t)1);
    ASSERT_EQ(results[0].total_accesses, (uint64_t)0);
    // Hit rate should be 0 (no accesses)
    ASSERT_TRUE(results[0].l1_hit_rate < 0.001);
}

// ============================================================================
// Large Addresses
// ============================================================================

TEST(EdgeCase_LargeAddress) {
    // Test with 64-bit addresses near UINT64_MAX
    std::vector<CacheLevelConfig> levels = {
        {"L1", 256, 64, 2, 1, "lru"}
    };
    MemoryHierarchy mh(levels, 200);

    uint64_t big_addr = 0xFFFFFFFFFFFF0000ULL;
    auto r1 = mh.read(big_addr);
    ASSERT_FALSE(r1.hit);  // cold miss

    auto r2 = mh.read(big_addr);
    ASSERT_TRUE(r2.hit);   // should hit after fill
    ASSERT_EQ(r2.hit_level, (size_t)0);
}

// ============================================================================
// Write-Only Trace
// ============================================================================

TEST(EdgeCase_WriteOnlyTrace) {
    std::vector<CacheLevelConfig> levels = {
        {"L1", 256, 64, 2, 1, "lru"},
        {"L2", 1024, 64, 4, 10, "lru"}
    };
    MemoryHierarchy mh(levels, 200);

    // All writes
    mh.write(0x1000);  // cold miss
    mh.write(0x2000);  // cold miss
    mh.write(0x1000);  // hit (filled on previous write)
    mh.write(0x2000);  // hit

    ASSERT_EQ(mh.stats().level(0).hits, (uint64_t)2);
    ASSERT_EQ(mh.stats().level(0).misses, (uint64_t)2);
    ASSERT_EQ(mh.stats().total_accesses(), (uint64_t)4);
}
