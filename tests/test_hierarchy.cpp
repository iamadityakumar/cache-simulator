// test_hierarchy.cpp — Integration tests for MemoryHierarchy and Statistics
#include "test_framework.h"
#include "memory_hierarchy.h"
#include "stats.h"

// ============================================================================
// Statistics Unit Tests
// ============================================================================

TEST(Stats_Construction) {
    Statistics stats({"L1", "L2", "L3"}, {1, 10, 40}, 200);
    ASSERT_EQ(stats.num_levels(), (size_t)3);
    ASSERT_EQ(stats.total_accesses(), (uint64_t)0);
    ASSERT_EQ(stats.mem_latency(), (uint64_t)200);
}

TEST(Stats_RecordHitMiss) {
    Statistics stats({"L1", "L2"}, {1, 10}, 200);

    stats.record_hit(0);   // L1 hit
    stats.record_miss(0);  // L1 miss
    stats.record_hit(1);   // L2 hit (the miss from L1 was found in L2)

    ASSERT_EQ(stats.level(0).hits, (uint64_t)1);
    ASSERT_EQ(stats.level(0).misses, (uint64_t)1);
    ASSERT_EQ(stats.level(0).accesses, (uint64_t)2);
    ASSERT_EQ(stats.level(1).hits, (uint64_t)1);
    ASSERT_EQ(stats.level(1).accesses, (uint64_t)1);
}

TEST(Stats_HitRate) {
    Statistics stats({"L1"}, {1}, 200);
    stats.record_hit(0);
    stats.record_hit(0);
    stats.record_miss(0);

    // 2 hits / 3 accesses ≈ 0.6667
    double hr = stats.level(0).hit_rate();
    ASSERT_TRUE(hr > 0.666 && hr < 0.667);

    double mr = stats.level(0).miss_rate();
    ASSERT_TRUE(mr > 0.333 && mr < 0.334);
}

TEST(Stats_AMAT_AllHitsL1) {
    // If everything hits in L1, AMAT = L1_hit_time = 1
    Statistics stats({"L1", "L2"}, {1, 10}, 200);
    stats.record_hit(0);
    stats.record_hit(0);
    stats.record_hit(0);

    double amat = stats.amat();
    // L1 miss_rate = 0 → AMAT = 1 + 0 * (...) = 1
    ASSERT_TRUE(amat > 0.99 && amat < 1.01);
}

TEST(Stats_AMAT_AllMissToMemory) {
    // Everything misses all levels → AMAT = L1_hit + L2_hit + mem_latency
    Statistics stats({"L1", "L2"}, {1, 10}, 200);
    stats.record_miss(0);
    stats.record_miss(1);

    double amat = stats.amat();
    // L1_miss_rate=1, L2_miss_rate=1
    // AMAT = 1 + 1*(10 + 1*200) = 1 + 210 = 211
    ASSERT_TRUE(amat > 210.99 && amat < 211.01);
}

TEST(Stats_AMAT_Mixed) {
    // 2 L1 hits, 1 L1 miss that hits L2
    Statistics stats({"L1", "L2"}, {1, 10}, 200);
    stats.record_hit(0);
    stats.record_hit(0);
    stats.record_miss(0);
    stats.record_hit(1);

    // L1: 2 hits, 1 miss → miss_rate = 1/3
    // L2: 1 hit, 0 miss → miss_rate = 0
    // AMAT = 1 + (1/3) * (10 + 0 * 200)
    //      = 1 + (1/3) * 10
    //      ≈ 1 + 3.333 = 4.333
    double amat = stats.amat();
    ASSERT_TRUE(amat > 4.33 && amat < 4.34);
}

TEST(Stats_Reset) {
    Statistics stats({"L1"}, {1}, 200);
    stats.record_hit(0);
    stats.record_miss(0);
    stats.reset();

    ASSERT_EQ(stats.level(0).hits, (uint64_t)0);
    ASSERT_EQ(stats.level(0).misses, (uint64_t)0);
    ASSERT_EQ(stats.total_accesses(), (uint64_t)0);
}

// ============================================================================
// MemoryHierarchy — Single Level Tests
// ============================================================================

static std::vector<CacheLevelConfig> single_level_config() {
    // Small L1: 256 bytes, 64-byte blocks, 2-way, 1-cycle hit, LRU
    return { {"L1", 256, 64, 2, 1, "lru"} };
}

TEST(Hierarchy_SingleLevel_ColdMiss) {
    MemoryHierarchy mh(single_level_config(), 200);

    auto r = mh.read(0x1000);
    ASSERT_FALSE(r.hit);
    ASSERT_EQ(r.hit_level, (size_t)1);  // main memory
    // Latency: L1_check(1) + mem(200) = 201
    ASSERT_EQ(r.total_latency, (uint64_t)201);

    // Stats: 1 miss at L1
    ASSERT_EQ(mh.stats().level(0).misses, (uint64_t)1);
    ASSERT_EQ(mh.stats().level(0).hits, (uint64_t)0);
}

TEST(Hierarchy_SingleLevel_HitAfterMiss) {
    MemoryHierarchy mh(single_level_config(), 200);

    // First access: cold miss
    mh.read(0x1000);

    // Second access to same address: should hit in L1
    auto r = mh.read(0x1000);
    ASSERT_TRUE(r.hit);
    ASSERT_EQ(r.hit_level, (size_t)0);
    ASSERT_EQ(r.total_latency, (uint64_t)1);

    // Stats: 1 hit, 1 miss
    ASSERT_EQ(mh.stats().level(0).hits, (uint64_t)1);
    ASSERT_EQ(mh.stats().level(0).misses, (uint64_t)1);
}

TEST(Hierarchy_SingleLevel_SameBlock) {
    MemoryHierarchy mh(single_level_config(), 200);

    // Access offset 0 of a block
    mh.read(0x1000);

    // Access offset 0x20 of the same 64-byte block → should hit
    auto r = mh.read(0x1020);
    ASSERT_TRUE(r.hit);
    ASSERT_EQ(r.hit_level, (size_t)0);
}

TEST(Hierarchy_SingleLevel_WriteSetsDirty) {
    MemoryHierarchy mh(single_level_config(), 200);

    auto r = mh.write(0x2000);
    ASSERT_FALSE(r.hit);
    ASSERT_TRUE(r.is_write);

    // After write, the block should be in L1
    auto r2 = mh.read(0x2000);
    ASSERT_TRUE(r2.hit);
}

// ============================================================================
// MemoryHierarchy — Multi-Level Tests (L1 → L2)
// ============================================================================

static std::vector<CacheLevelConfig> two_level_config() {
    return {
        {"L1", 256, 64, 2, 1, "lru"},    // L1: 256B, 2-way, 1 cycle
        {"L2", 1024, 64, 4, 10, "lru"}   // L2: 1KB, 4-way, 10 cycles
    };
}

TEST(Hierarchy_TwoLevel_ColdMiss) {
    MemoryHierarchy mh(two_level_config(), 200);

    auto r = mh.read(0x3000);
    ASSERT_FALSE(r.hit);
    ASSERT_EQ(r.hit_level, (size_t)2);  // main memory
    // Latency: L1(1) + L2(10) + mem(200) = 211
    ASSERT_EQ(r.total_latency, (uint64_t)211);

    ASSERT_EQ(mh.stats().level(0).misses, (uint64_t)1);
    ASSERT_EQ(mh.stats().level(1).misses, (uint64_t)1);
}

TEST(Hierarchy_TwoLevel_HitL1AfterFill) {
    MemoryHierarchy mh(two_level_config(), 200);

    mh.read(0x3000);  // Cold miss → fills L1 and L2

    auto r = mh.read(0x3000);
    ASSERT_TRUE(r.hit);
    ASSERT_EQ(r.hit_level, (size_t)0);  // L1 hit
    ASSERT_EQ(r.total_latency, (uint64_t)1);
}

TEST(Hierarchy_TwoLevel_EvictFromL1_HitL2) {
    MemoryHierarchy mh(two_level_config(), 200);

    // L1: 256 bytes, 64-byte blocks, 2-way = 2 sets
    // Set capacity per set = 2 blocks
    // Fill set 0 with 2 blocks, then evict one by adding a third.

    // These addresses all map to the same set in L1 (set 0):
    // Address 0x0000 → set 0, tag 0
    // Address 0x0100 → set 0, tag 1  (256 = 64 * 4 sets → wraps around index)
    // Address 0x0200 → set 0, tag 2

    // Wait — let's compute carefully:
    // L1: 256B, 64B blocks, 2-way → num_sets = 256/(64*2) = 2
    // offset_bits = 6, index_bits = 1 (2 sets)
    // Address 0x0000: block_addr = 0, set = 0 & 1 = 0, tag = 0 >> 1 = 0
    // Address 0x0080: block_addr = 2, set = 2 & 1 = 0, tag = 2 >> 1 = 1
    // Address 0x0100: block_addr = 4, set = 4 & 1 = 0, tag = 4 >> 1 = 2

    mh.read(0x0000);  // L1 miss → fills L1 set 0 way 0, L2
    mh.read(0x0080);  // L1 miss → fills L1 set 0 way 1, L2
    mh.read(0x0100);  // L1 miss → evicts from L1 set 0, fills L1 + L2

    // Now 0x0000 is evicted from L1 but should still be in L2.
    auto r = mh.read(0x0000);

    // Should miss L1 but hit L2
    ASSERT_TRUE(r.hit);
    ASSERT_EQ(r.hit_level, (size_t)1);  // L2 hit
    // Latency: L1 miss(1) + L2 hit(10) = 11
    ASSERT_EQ(r.total_latency, (uint64_t)11);
}

// ============================================================================
// MemoryHierarchy — Three-Level Tests (L1 → L2 → L3)
// ============================================================================

static std::vector<CacheLevelConfig> three_level_config() {
    return {
        {"L1", 256, 64, 2, 1, "lru"},     // L1: 256B, 2-way
        {"L2", 1024, 64, 4, 10, "lru"},    // L2: 1KB, 4-way
        {"L3", 4096, 64, 8, 40, "lru"}     // L3: 4KB, 8-way
    };
}

TEST(Hierarchy_ThreeLevel_ColdMiss) {
    MemoryHierarchy mh(three_level_config(), 200);

    auto r = mh.read(0x5000);
    ASSERT_FALSE(r.hit);
    ASSERT_EQ(r.hit_level, (size_t)3);
    // Latency: 1 + 10 + 40 + 200 = 251
    ASSERT_EQ(r.total_latency, (uint64_t)251);
}

TEST(Hierarchy_ThreeLevel_FillPopulatesAllLevels) {
    MemoryHierarchy mh(three_level_config(), 200);

    mh.read(0x5000);  // Cold miss → fills L1, L2, L3

    // Should be in L1 now
    auto r = mh.read(0x5000);
    ASSERT_TRUE(r.hit);
    ASSERT_EQ(r.hit_level, (size_t)0);
    ASSERT_EQ(r.total_latency, (uint64_t)1);
}

TEST(Hierarchy_ThreeLevel_FIFO) {
    // Use FIFO policy instead of LRU
    std::vector<CacheLevelConfig> config = {
        {"L1", 256, 64, 2, 1, "fifo"},
        {"L2", 1024, 64, 4, 10, "fifo"}
    };

    MemoryHierarchy mh(config, 200);

    mh.read(0x0000);  // miss → fill
    mh.read(0x0000);  // hit L1

    auto r = mh.read(0x0000);
    ASSERT_TRUE(r.hit);
    ASSERT_EQ(r.hit_level, (size_t)0);
}

// ============================================================================
// MemoryHierarchy — Hand-Crafted Trace Scenarios
// ============================================================================

TEST(Hierarchy_Trace_SequentialReuse) {
    // Simulate: read A, read B, read A, read B (should get 2 misses, 2 hits)
    MemoryHierarchy mh(single_level_config(), 200);

    mh.read(0x1000);  // miss
    mh.read(0x2000);  // miss
    mh.read(0x1000);  // hit
    mh.read(0x2000);  // hit

    ASSERT_EQ(mh.stats().level(0).hits, (uint64_t)2);
    ASSERT_EQ(mh.stats().level(0).misses, (uint64_t)2);
}

TEST(Hierarchy_Trace_WriteReadBack) {
    MemoryHierarchy mh(single_level_config(), 200);

    mh.write(0x4000);  // miss → fill + dirty
    auto r = mh.read(0x4000);  // hit (data was filled on write miss)
    ASSERT_TRUE(r.hit);
    ASSERT_EQ(r.hit_level, (size_t)0);
}

TEST(Hierarchy_Trace_MixedReadWrite) {
    MemoryHierarchy mh(two_level_config(), 200);

    mh.access(0xA000, 'R');  // cold miss
    mh.access(0xA000, 'W');  // hit L1 (already cached from read)
    mh.access(0xB000, 'R');  // cold miss
    mh.access(0xB000, 'R');  // hit L1

    ASSERT_EQ(mh.stats().level(0).hits, (uint64_t)2);
    ASSERT_EQ(mh.stats().level(0).misses, (uint64_t)2);
}

TEST(Hierarchy_NumLevels) {
    MemoryHierarchy mh(three_level_config(), 200);
    ASSERT_EQ(mh.num_levels(), (size_t)3);
    ASSERT_EQ(mh.cache_level(0).name(), std::string("L1"));
    ASSERT_EQ(mh.cache_level(1).name(), std::string("L2"));
    ASSERT_EQ(mh.cache_level(2).name(), std::string("L3"));
}

TEST(Hierarchy_LatencyMath_FullMiss3Level) {
    MemoryHierarchy mh(three_level_config(), 200);

    // 5 cold misses
    for (uint64_t i = 0; i < 5; i++) {
        mh.read(0x10000 + i * 0x1000);
    }

    ASSERT_EQ(mh.stats().level(0).misses, (uint64_t)5);
    ASSERT_EQ(mh.stats().level(1).misses, (uint64_t)5);
    ASSERT_EQ(mh.stats().level(2).misses, (uint64_t)5);

    // All 100% miss rate → AMAT = 1 + 1*(10 + 1*(40 + 1*200)) = 251
    double amat = mh.stats().amat();
    ASSERT_TRUE(amat > 250.99 && amat < 251.01);
}
