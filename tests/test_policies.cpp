// test_policies.cpp - Unit tests for LRU and FIFO replacement policies
#include "test_framework.h"
#include "policies.h"

// ============================================================================
// LRU Policy Tests
// ============================================================================

TEST(LRU_EmptyMiss) {
    LRUPolicy lru(4);
    ASSERT_FALSE(lru.access(0x100));
    ASSERT_FALSE(lru.contains(0x100));
    ASSERT_EQ(lru.size(), (size_t)0);
}

TEST(LRU_InsertAndHit) {
    LRUPolicy lru(4);
    auto evicted = lru.insert(0xA);
    ASSERT_FALSE(evicted.has_value());
    ASSERT_TRUE(lru.contains(0xA));
    ASSERT_TRUE(lru.access(0xA));
    ASSERT_EQ(lru.size(), (size_t)1);
}

TEST(LRU_EvictsLeastRecentlyUsed) {
    LRUPolicy lru(3);
    lru.insert(0xA);  // order: A
    lru.insert(0xB);  // order: B, A
    lru.insert(0xC);  // order: C, B, A

    // Access A to make it MRU — order becomes: A, C, B
    lru.access(0xA);

    // Insert D — should evict B (LRU)
    auto evicted = lru.insert(0xD);
    ASSERT_TRUE(evicted.has_value());
    ASSERT_EQ(evicted.value(), (uint64_t)0xB);
    ASSERT_FALSE(lru.contains(0xB));
    ASSERT_TRUE(lru.contains(0xA));
    ASSERT_TRUE(lru.contains(0xC));
    ASSERT_TRUE(lru.contains(0xD));
}

TEST(LRU_InsertDuplicate) {
    LRUPolicy lru(2);
    lru.insert(0xA);
    lru.insert(0xB);
    // Re-insert A — should NOT evict, just move to MRU
    auto evicted = lru.insert(0xA);
    ASSERT_FALSE(evicted.has_value());
    ASSERT_EQ(lru.size(), (size_t)2);
}

TEST(LRU_Remove) {
    LRUPolicy lru(4);
    lru.insert(0xA);
    lru.insert(0xB);
    lru.remove(0xA);
    ASSERT_FALSE(lru.contains(0xA));
    ASSERT_TRUE(lru.contains(0xB));
    ASSERT_EQ(lru.size(), (size_t)1);
}

TEST(LRU_EvictionOrder) {
    LRUPolicy lru(2);
    lru.insert(0x1);  // [1]
    lru.insert(0x2);  // [2, 1]

    auto e1 = lru.insert(0x3);  // evict 1 -> [3, 2]
    ASSERT_EQ(e1.value(), (uint64_t)0x1);

    auto e2 = lru.insert(0x4);  // evict 2 -> [4, 3]
    ASSERT_EQ(e2.value(), (uint64_t)0x2);

    lru.access(0x3);  // touch 3 -> [3, 4]
    auto e3 = lru.insert(0x5);  // evict 4 -> [5, 3]
    ASSERT_EQ(e3.value(), (uint64_t)0x4);
}

// ============================================================================
// FIFO Policy Tests
// ============================================================================

TEST(FIFO_EmptyMiss) {
    FIFOPolicy fifo(4);
    ASSERT_FALSE(fifo.access(0x100));
    ASSERT_FALSE(fifo.contains(0x100));
    ASSERT_EQ(fifo.size(), (size_t)0);
}

TEST(FIFO_InsertAndHit) {
    FIFOPolicy fifo(4);
    auto evicted = fifo.insert(0xA);
    ASSERT_FALSE(evicted.has_value());
    ASSERT_TRUE(fifo.contains(0xA));
    ASSERT_TRUE(fifo.access(0xA));
    ASSERT_EQ(fifo.size(), (size_t)1);
}

TEST(FIFO_EvictsOldestInserted) {
    FIFOPolicy fifo(3);
    fifo.insert(0xA);  // oldest
    fifo.insert(0xB);
    fifo.insert(0xC);

    // Access A — FIFO should NOT change order
    fifo.access(0xA);

    // Insert D — should evict A (oldest, regardless of access)
    auto evicted = fifo.insert(0xD);
    ASSERT_TRUE(evicted.has_value());
    ASSERT_EQ(evicted.value(), (uint64_t)0xA);
    ASSERT_FALSE(fifo.contains(0xA));
}

TEST(FIFO_AccessDoesNotReorder) {
    FIFOPolicy fifo(2);
    fifo.insert(0xA);  // oldest
    fifo.insert(0xB);

    fifo.access(0xA);
    fifo.access(0xA);
    fifo.access(0xA);

    auto evicted = fifo.insert(0xC);
    ASSERT_EQ(evicted.value(), (uint64_t)0xA);
}

TEST(FIFO_InsertDuplicate) {
    FIFOPolicy fifo(2);
    fifo.insert(0xA);
    fifo.insert(0xB);
    auto evicted = fifo.insert(0xA);
    ASSERT_FALSE(evicted.has_value());
    ASSERT_EQ(fifo.size(), (size_t)2);
}

TEST(FIFO_Remove) {
    FIFOPolicy fifo(4);
    fifo.insert(0xA);
    fifo.insert(0xB);
    fifo.remove(0xA);
    ASSERT_FALSE(fifo.contains(0xA));
    ASSERT_TRUE(fifo.contains(0xB));
    ASSERT_EQ(fifo.size(), (size_t)1);
}

// ============================================================================
// Factory Tests
// ============================================================================

TEST(Factory_LRU) {
    auto p = make_policy("lru", 4);
    ASSERT_EQ(p->name(), std::string("LRU"));
}

TEST(Factory_FIFO) {
    auto p = make_policy("fifo", 4);
    ASSERT_EQ(p->name(), std::string("FIFO"));
}
