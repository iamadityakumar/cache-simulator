// test_cache_level.cpp - Unit tests for CacheSet and CacheLevel
#include "test_framework.h"
#include "cache_set.h"
#include "cache_level.h"

// ============================================================================
// CacheSet Tests
// ============================================================================

TEST(CacheSet_LookupMissOnEmpty) {
    CacheSet cs(4, "lru");
    ASSERT_FALSE(cs.lookup(0xABC));
    ASSERT_EQ(cs.occupancy(), (size_t)0);
}

TEST(CacheSet_InsertAndLookup) {
    CacheSet cs(4, "lru");
    auto evicted = cs.insert(0x10, false);
    ASSERT_FALSE(evicted.has_value());
    ASSERT_TRUE(cs.lookup(0x10));
    ASSERT_TRUE(cs.contains(0x10));
    ASSERT_EQ(cs.occupancy(), (size_t)1);
}

TEST(CacheSet_Eviction) {
    CacheSet cs(2, "lru");
    cs.insert(0x1, false);
    cs.insert(0x2, false);
    auto evicted = cs.insert(0x3, false);  // should evict 0x1
    ASSERT_TRUE(evicted.has_value());
    ASSERT_EQ(evicted.value(), (uint64_t)0x1);
    ASSERT_FALSE(cs.contains(0x1));
    ASSERT_TRUE(cs.contains(0x2));
    ASSERT_TRUE(cs.contains(0x3));
}

TEST(CacheSet_DirtyBit) {
    CacheSet cs(4, "lru");
    cs.insert(0x10, false);
    ASSERT_FALSE(cs.is_dirty(0x10));
    cs.mark_dirty(0x10);
    ASSERT_TRUE(cs.is_dirty(0x10));
}

TEST(CacheSet_WriteInsertSetsDirty) {
    CacheSet cs(4, "lru");
    cs.insert(0x20, true);  // is_write = true
    ASSERT_TRUE(cs.is_dirty(0x20));
}

TEST(CacheSet_Invalidate) {
    CacheSet cs(4, "lru");
    cs.insert(0xA, false);
    cs.invalidate(0xA);
    ASSERT_FALSE(cs.contains(0xA));
    ASSERT_EQ(cs.occupancy(), (size_t)0);
}

TEST(CacheSet_FIFO) {
    CacheSet cs(2, "fifo");
    cs.insert(0x1, false);
    cs.insert(0x2, false);
    cs.lookup(0x1);  // access 0x1 — FIFO should NOT save it
    auto evicted = cs.insert(0x3, false);  // should evict 0x1 (oldest)
    ASSERT_EQ(evicted.value(), (uint64_t)0x1);
}

// ============================================================================
// CacheLevel Address Decomposition Tests
// ============================================================================

TEST(CacheLevel_AddressDecomp_DirectMapped) {
    // 256 bytes, 64-byte blocks, 1-way = 4 sets
    // offset_bits = 6, index_bits = 2, tag = remaining
    CacheLevel cl("test", 256, 64, 1, 1, "lru");
    ASSERT_EQ(cl.num_sets(), (size_t)4);

    // Address 0x00 -> block 0, set 0, tag 0
    ASSERT_EQ(cl.get_set_index(0x00), (size_t)0);
    ASSERT_EQ(cl.get_tag(0x00), (uint64_t)0);

    // Address 0x40 (64) -> block 1, set 1, tag 0
    ASSERT_EQ(cl.get_set_index(0x40), (size_t)1);
    ASSERT_EQ(cl.get_tag(0x40), (uint64_t)0);

    // Address 0x80 (128) -> block 2, set 2, tag 0
    ASSERT_EQ(cl.get_set_index(0x80), (size_t)2);

    // Address 0x100 (256) -> block 4, set 0, tag 1
    ASSERT_EQ(cl.get_set_index(0x100), (size_t)0);
    ASSERT_EQ(cl.get_tag(0x100), (uint64_t)1);
}

TEST(CacheLevel_AddressDecomp_SetAssociative) {
    // 512 bytes, 64-byte blocks, 2-way = 4 sets
    // offset_bits = 6, index_bits = 2
    CacheLevel cl("test", 512, 64, 2, 1, "lru");
    ASSERT_EQ(cl.num_sets(), (size_t)4);

    ASSERT_EQ(cl.get_set_index(0x00), (size_t)0);
    ASSERT_EQ(cl.get_set_index(0x40), (size_t)1);
    ASSERT_EQ(cl.get_set_index(0x80), (size_t)2);
    ASSERT_EQ(cl.get_set_index(0xC0), (size_t)3);
    ASSERT_EQ(cl.get_set_index(0x100), (size_t)0);  // wraps around
}

// ============================================================================
// CacheLevel Lookup and Fill Tests
// ============================================================================

TEST(CacheLevel_MissOnEmpty) {
    CacheLevel cl("L1", 256, 64, 1, 1, "lru");
    auto result = cl.lookup(0x100);
    ASSERT_FALSE(result.hit);
    ASSERT_EQ(result.latency, (uint64_t)0);
}

TEST(CacheLevel_FillThenHit) {
    CacheLevel cl("L1", 256, 64, 1, 4, "lru");
    cl.fill(0x100, false);
    auto result = cl.lookup(0x100);
    ASSERT_TRUE(result.hit);
    ASSERT_EQ(result.latency, (uint64_t)4);
}

TEST(CacheLevel_SameBlockDifferentOffset) {
    // Two addresses in same 64-byte block should map to same cache line
    CacheLevel cl("L1", 256, 64, 1, 1, "lru");
    cl.fill(0x100, false);
    // 0x100 and 0x120 are in the same 64-byte block (0x100–0x13F)
    auto r = cl.lookup(0x120);
    // They share the same tag and set index
    ASSERT_EQ(cl.get_tag(0x100), cl.get_tag(0x120));
    ASSERT_EQ(cl.get_set_index(0x100), cl.get_set_index(0x120));
    ASSERT_TRUE(r.hit);
}

TEST(CacheLevel_ConflictMiss) {
    // Direct-mapped cache: 2 addresses mapping to same set
    // 256 bytes, 64-byte blocks, 1-way = 4 sets
    CacheLevel cl("L1", 256, 64, 1, 1, "lru");

    cl.fill(0x00, false);  // set 0
    ASSERT_TRUE(cl.contains(0x00));

    cl.fill(0x100, false);  // set 0, different tag — evicts 0x00
    ASSERT_TRUE(cl.contains(0x100));
    ASSERT_FALSE(cl.contains(0x00));  // conflict miss
}

TEST(CacheLevel_AssociativeNoConflict) {
    // 2-way set associative: can hold 2 blocks per set
    // 512 bytes, 64-byte blocks, 2-way = 4 sets
    CacheLevel cl("L1", 512, 64, 2, 1, "lru");

    cl.fill(0x00, false);   // set 0, way 0
    cl.fill(0x100, false);  // set 0, way 1 — both fit
    ASSERT_TRUE(cl.contains(0x00));
    ASSERT_TRUE(cl.contains(0x100));
}

TEST(CacheLevel_Invalidate) {
    CacheLevel cl("L1", 256, 64, 1, 1, "lru");
    cl.fill(0x100, false);
    ASSERT_TRUE(cl.contains(0x100));
    cl.invalidate(0x100);
    ASSERT_FALSE(cl.contains(0x100));
}
