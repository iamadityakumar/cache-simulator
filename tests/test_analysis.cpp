// test_analysis.cpp — Tests for sweep engine and compare mode (Phase 5)
#include "test_framework.h"
#include "sweep.h"
#include "compare.h"
#include "config.h"
#include "trace_loader.h"
#include <sstream>
#include <cmath>

// ============================================================================
// Helper: generate a small deterministic trace for testing
// ============================================================================

static std::vector<TraceEntry> make_test_trace() {
    TraceGenConfig gen;
    gen.pattern    = TracePattern::LOCALITY;
    gen.count      = 500;
    gen.seed       = 42;
    gen.block_size = 64;
    return generate_trace(gen);
}

// ============================================================================
// Sweep Tests
// ============================================================================

TEST(Sweep_SingleConfig) {
    // Sweep with exactly one value per dimension → 1 result
    SweepConfig sc;
    sc.l1_sizes        = {32 * 1024};
    sc.l2_sizes        = {256 * 1024};
    sc.l3_sizes        = {4 * 1024 * 1024};
    sc.policies        = {"lru"};
    sc.associativities = {8};

    SimConfig base;
    auto trace = make_test_trace();
    auto results = run_sweep(sc, base, trace);

    ASSERT_EQ(results.size(), (size_t)1);
    ASSERT_EQ(results[0].l1_size, (size_t)(32 * 1024));
    ASSERT_EQ(results[0].policy, std::string("lru"));
    ASSERT_TRUE(results[0].total_accesses == 500);
}

TEST(Sweep_MultiConfig) {
    // 2 L1 sizes × 1 L2 × 1 L3 × 2 policies × 1 assoc = 4 results
    SweepConfig sc;
    sc.l1_sizes        = {16 * 1024, 32 * 1024};
    sc.l2_sizes        = {256 * 1024};
    sc.l3_sizes        = {4 * 1024 * 1024};
    sc.policies        = {"lru", "fifo"};
    sc.associativities = {8};

    SimConfig base;
    auto trace = make_test_trace();
    auto results = run_sweep(sc, base, trace);

    ASSERT_EQ(results.size(), (size_t)4);
}

TEST(Sweep_SortedByAMAT) {
    SweepConfig sc;
    sc.l1_sizes        = {16 * 1024, 64 * 1024};
    sc.l2_sizes        = {256 * 1024};
    sc.l3_sizes        = {4 * 1024 * 1024};
    sc.policies        = {"lru"};
    sc.associativities = {8};

    SimConfig base;
    auto trace = make_test_trace();
    auto results = run_sweep(sc, base, trace);

    ASSERT_TRUE(results.size() >= 2);
    // Results should be sorted ascending by AMAT
    for (size_t i = 1; i < results.size(); i++) {
        ASSERT_TRUE(results[i].amat >= results[i - 1].amat);
    }
}

TEST(Sweep_FindOptimal) {
    SweepConfig sc;
    sc.l1_sizes        = {16 * 1024, 32 * 1024, 64 * 1024};
    sc.l2_sizes        = {256 * 1024};
    sc.l3_sizes        = {4 * 1024 * 1024};
    sc.policies        = {"lru"};
    sc.associativities = {8};

    SimConfig base;
    auto trace = make_test_trace();
    auto results = run_sweep(sc, base, trace);

    auto optimal = find_optimal(results);
    // Optimal should be the same as results[0] (lowest AMAT)
    ASSERT_TRUE(std::abs(optimal.amat - results[0].amat) < 0.001);
}

TEST(Sweep_TableOutput) {
    SweepConfig sc;
    sc.l1_sizes        = {32 * 1024};
    sc.l2_sizes        = {256 * 1024};
    sc.l3_sizes        = {4 * 1024 * 1024};
    sc.policies        = {"lru"};
    sc.associativities = {8};

    SimConfig base;
    auto trace = make_test_trace();
    auto results = run_sweep(sc, base, trace);

    std::ostringstream oss;
    report_sweep_table(results, oss);
    std::string output = oss.str();

    ASSERT_TRUE(output.find("Parameter Sweep Results") != std::string::npos);
    ASSERT_TRUE(output.find("AMAT") != std::string::npos);
    ASSERT_TRUE(output.find("OPTIMAL") != std::string::npos);
    ASSERT_TRUE(output.find("L1") != std::string::npos);
    ASSERT_TRUE(output.find("#1") != std::string::npos);
}

TEST(Sweep_CsvOutput) {
    SweepConfig sc;
    sc.l1_sizes        = {16 * 1024, 32 * 1024};
    sc.l2_sizes        = {256 * 1024};
    sc.l3_sizes        = {4 * 1024 * 1024};
    sc.policies        = {"lru"};
    sc.associativities = {8};

    SimConfig base;
    auto trace = make_test_trace();
    auto results = run_sweep(sc, base, trace);

    std::ostringstream oss;
    report_sweep_csv(results, oss);
    std::string output = oss.str();

    // Header + 2 data rows = 3 newlines
    size_t newlines = 0;
    for (char c : output) {
        if (c == '\n') newlines++;
    }
    ASSERT_EQ(newlines, (size_t)3);

    // Header should contain key column names
    ASSERT_TRUE(output.find("rank,l1_size") != std::string::npos);
    ASSERT_TRUE(output.find("amat") != std::string::npos);
}

// ============================================================================
// Compare Tests
// ============================================================================

TEST(Compare_LRUvsFIFO) {
    SimConfig config;
    config.l1_size  = 1024;
    config.l1_assoc = 4;
    config.l2_size  = 4096;
    config.l2_assoc = 4;
    config.l3_size  = 16384;
    config.l3_assoc = 4;
    config.block_size = 64;

    auto trace = make_test_trace();
    auto result = run_compare(config, trace);

    // Both should have processed the same number of accesses
    ASSERT_EQ(result.lru_result.total_accesses, result.fifo_result.total_accesses);
    ASSERT_EQ(result.lru_result.total_accesses, (uint64_t)500);

    // Policies should be correct
    ASSERT_EQ(result.lru_result.policy, std::string("lru"));
    ASSERT_EQ(result.fifo_result.policy, std::string("fifo"));
}

TEST(Compare_WinnerDetermined) {
    SimConfig config;
    config.l1_size  = 1024;
    config.l1_assoc = 4;
    config.l2_size  = 4096;
    config.l2_assoc = 4;
    config.l3_size  = 16384;
    config.l3_assoc = 4;
    config.block_size = 64;

    auto trace = make_test_trace();
    auto result = run_compare(config, trace);

    // Winner must be either LRU or FIFO
    ASSERT_TRUE(result.winner == "LRU" || result.winner == "FIFO");

    // amat_diff should be consistent with winner
    if (result.winner == "LRU") {
        ASSERT_TRUE(result.amat_diff >= 0.0);
    } else {
        ASSERT_TRUE(result.amat_diff < 0.0);
    }
}

TEST(Compare_ReportOutput) {
    SimConfig config;
    config.l1_size  = 2048;
    config.l1_assoc = 4;
    config.l2_size  = 8192;
    config.l2_assoc = 4;
    config.l3_size  = 32768;
    config.l3_assoc = 4;
    config.block_size = 64;

    auto trace = make_test_trace();
    auto result = run_compare(config, trace);

    std::ostringstream oss;
    report_compare(result, oss);
    std::string output = oss.str();

    ASSERT_TRUE(output.find("LRU vs FIFO") != std::string::npos);
    ASSERT_TRUE(output.find("WINNER") != std::string::npos);
    ASSERT_TRUE(output.find("AMAT") != std::string::npos);
    ASSERT_TRUE(output.find("L1 Hit Rate") != std::string::npos);
    ASSERT_TRUE(output.find("L2 Hit Rate") != std::string::npos);
    ASSERT_TRUE(output.find("L3 Hit Rate") != std::string::npos);
}

TEST(Compare_SameTrace) {
    // Both sides should see the exact same accesses
    SimConfig config;
    auto trace = make_test_trace();
    auto result = run_compare(config, trace);

    ASSERT_EQ(result.lru_result.total_accesses, result.fifo_result.total_accesses);
}
