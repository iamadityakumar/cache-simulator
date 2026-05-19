// test_trace.cpp — Tests for trace loader/parser and synthetic generator
#include "test_framework.h"
#include "trace_loader.h"
#include <fstream>
#include <cmath>
#include <algorithm>
#include <set>

// ============================================================================
// Trace Parser — Valid Input Tests
// ============================================================================

TEST(Trace_ParseBasicRead) {
    auto entries = parse_trace_string("R 0x1000\n");
    ASSERT_EQ(entries.size(), (size_t)1);
    ASSERT_EQ(entries[0].type, 'R');
    ASSERT_EQ(entries[0].address, (uint64_t)0x1000);
}

TEST(Trace_ParseBasicWrite) {
    auto entries = parse_trace_string("W 0x2000\n");
    ASSERT_EQ(entries.size(), (size_t)1);
    ASSERT_EQ(entries[0].type, 'W');
    ASSERT_EQ(entries[0].address, (uint64_t)0x2000);
}

TEST(Trace_ParseMultipleEntries) {
    std::string trace =
        "R 0x1000\n"
        "W 0x2000\n"
        "R 0x3000\n"
        "W 0x4000\n";

    auto entries = parse_trace_string(trace);
    ASSERT_EQ(entries.size(), (size_t)4);
    ASSERT_EQ(entries[0].type, 'R');
    ASSERT_EQ(entries[0].address, (uint64_t)0x1000);
    ASSERT_EQ(entries[1].type, 'W');
    ASSERT_EQ(entries[1].address, (uint64_t)0x2000);
    ASSERT_EQ(entries[2].type, 'R');
    ASSERT_EQ(entries[2].address, (uint64_t)0x3000);
    ASSERT_EQ(entries[3].type, 'W');
    ASSERT_EQ(entries[3].address, (uint64_t)0x4000);
}

TEST(Trace_ParseSkipsComments) {
    std::string trace =
        "# This is a comment\n"
        "R 0x1000\n"
        "# Another comment\n"
        "W 0x2000\n";

    auto entries = parse_trace_string(trace);
    ASSERT_EQ(entries.size(), (size_t)2);
    ASSERT_EQ(entries[0].address, (uint64_t)0x1000);
    ASSERT_EQ(entries[1].address, (uint64_t)0x2000);
}

TEST(Trace_ParseSkipsBlankLines) {
    std::string trace =
        "\n"
        "R 0x1000\n"
        "\n"
        "\n"
        "W 0x2000\n"
        "\n";

    auto entries = parse_trace_string(trace);
    ASSERT_EQ(entries.size(), (size_t)2);
}

TEST(Trace_ParseLowercaseType) {
    auto entries = parse_trace_string("r 0x1000\nw 0x2000\n");
    ASSERT_EQ(entries.size(), (size_t)2);
    ASSERT_EQ(entries[0].type, 'R');
    ASSERT_EQ(entries[1].type, 'W');
}

TEST(Trace_ParseWithoutPrefix) {
    // Address without "0x" prefix — still valid hex
    auto entries = parse_trace_string("R 1000\n");
    ASSERT_EQ(entries.size(), (size_t)1);
    ASSERT_EQ(entries[0].address, (uint64_t)0x1000);
}

TEST(Trace_ParseLargeAddress) {
    auto entries = parse_trace_string("R 0x7fff5a3c1000\n");
    ASSERT_EQ(entries.size(), (size_t)1);
    ASSERT_EQ(entries[0].address, (uint64_t)0x7fff5a3c1000);
}

TEST(Trace_ParseInlineComment) {
    auto entries = parse_trace_string("R 0x1000 # inline comment\n");
    ASSERT_EQ(entries.size(), (size_t)1);
    ASSERT_EQ(entries[0].address, (uint64_t)0x1000);
}

TEST(Trace_ParseWhitespace) {
    // Extra leading/trailing whitespace
    auto entries = parse_trace_string("  R   0x1000  \n");
    ASSERT_EQ(entries.size(), (size_t)1);
    ASSERT_EQ(entries[0].address, (uint64_t)0x1000);
}

TEST(Trace_ParseEmpty) {
    auto entries = parse_trace_string("");
    ASSERT_EQ(entries.size(), (size_t)0);
}

TEST(Trace_ParseOnlyComments) {
    auto entries = parse_trace_string("# comment 1\n# comment 2\n");
    ASSERT_EQ(entries.size(), (size_t)0);
}

// ============================================================================
// Trace Parser — Error Handling Tests
// ============================================================================

TEST(Trace_ErrorBadType) {
    bool caught = false;
    try {
        parse_trace_string("X 0x1000\n");
    } catch (const TraceParseError& e) {
        caught = true;
        ASSERT_EQ(e.line_number(), (size_t)1);
    }
    ASSERT_TRUE(caught);
}

TEST(Trace_ErrorMissingAddress) {
    bool caught = false;
    try {
        parse_trace_string("R\n");
    } catch (const TraceParseError& e) {
        caught = true;
        ASSERT_EQ(e.line_number(), (size_t)1);
    }
    ASSERT_TRUE(caught);
}

TEST(Trace_ErrorBadAddress) {
    bool caught = false;
    try {
        parse_trace_string("R 0xZZZZ\n");
    } catch (const TraceParseError&) {
        caught = true;
    }
    ASSERT_TRUE(caught);
}

TEST(Trace_ErrorExtraToken) {
    bool caught = false;
    try {
        parse_trace_string("R 0x1000 extra\n");
    } catch (const TraceParseError&) {
        caught = true;
    }
    ASSERT_TRUE(caught);
}

TEST(Trace_ErrorMultiCharType) {
    bool caught = false;
    try {
        parse_trace_string("RW 0x1000\n");
    } catch (const TraceParseError&) {
        caught = true;
    }
    ASSERT_TRUE(caught);
}

TEST(Trace_ErrorEmptyFile) {
    // load_trace_file should throw when file has no valid entries
    // We test via a temp file
    std::string path = "d:/Website/cache-sim/traces/test_empty.trace";
    {
        std::ofstream f(path);
        f << "# Only comments\n";
    }
    bool caught = false;
    try {
        load_trace_file(path);
    } catch (const TraceParseError&) {
        caught = true;
    }
    ASSERT_TRUE(caught);
    // Clean up
    std::remove(path.c_str());
}

TEST(Trace_ErrorNonexistentFile) {
    bool caught = false;
    try {
        load_trace_file("d:/Website/cache-sim/traces/does_not_exist.trace");
    } catch (const std::runtime_error&) {
        caught = true;
    }
    ASSERT_TRUE(caught);
}

// ============================================================================
// Trace File I/O Round-Trip
// ============================================================================

TEST(Trace_WriteAndReadBack) {
    std::string path = "d:/Website/cache-sim/traces/test_roundtrip.trace";

    // Write a trace
    std::vector<TraceEntry> original = {
        {'R', 0x1000}, {'W', 0x2000}, {'R', 0x3000}, {'R', 0x1000}
    };
    write_trace_file(path, original);

    // Read it back
    auto loaded = load_trace_file(path);
    ASSERT_EQ(loaded.size(), original.size());
    for (size_t i = 0; i < original.size(); i++) {
        ASSERT_EQ(loaded[i].type, original[i].type);
        ASSERT_EQ(loaded[i].address, original[i].address);
    }

    // Clean up
    std::remove(path.c_str());
}

// ============================================================================
// Pattern Parsing
// ============================================================================

TEST(Trace_PatternParseSequential) {
    ASSERT_TRUE(parse_pattern("sequential") == TracePattern::SEQUENTIAL);
    ASSERT_TRUE(parse_pattern("SEQUENTIAL") == TracePattern::SEQUENTIAL);
}

TEST(Trace_PatternParseRandom) {
    ASSERT_TRUE(parse_pattern("random") == TracePattern::RANDOM);
}

TEST(Trace_PatternParseLocality) {
    ASSERT_TRUE(parse_pattern("locality") == TracePattern::LOCALITY);
}

TEST(Trace_PatternParseInvalid) {
    bool caught = false;
    try {
        parse_pattern("invalid");
    } catch (const std::invalid_argument&) {
        caught = true;
    }
    ASSERT_TRUE(caught);
}

// ============================================================================
// Synthetic Trace Generator Tests
// ============================================================================

TEST(TraceGen_SequentialCount) {
    TraceGenConfig cfg;
    cfg.pattern = TracePattern::SEQUENTIAL;
    cfg.count = 100;
    cfg.seed = 42;

    auto trace = generate_trace(cfg);
    ASSERT_EQ(trace.size(), (size_t)100);
}

TEST(TraceGen_SequentialStride) {
    TraceGenConfig cfg;
    cfg.pattern = TracePattern::SEQUENTIAL;
    cfg.count = 5;
    cfg.start_addr = 0x1000;
    cfg.stride = 64;
    cfg.write_ratio = 0.0;  // all reads for predictability
    cfg.seed = 42;

    auto trace = generate_trace(cfg);
    ASSERT_EQ(trace[0].address, (uint64_t)0x1000);
    ASSERT_EQ(trace[1].address, (uint64_t)0x1040);  // 0x1000 + 64
    ASSERT_EQ(trace[2].address, (uint64_t)0x1080);  // 0x1000 + 128
    ASSERT_EQ(trace[3].address, (uint64_t)0x10C0);  // 0x1000 + 192
    ASSERT_EQ(trace[4].address, (uint64_t)0x1100);  // 0x1000 + 256
}

TEST(TraceGen_SequentialWrapAround) {
    TraceGenConfig cfg;
    cfg.pattern = TracePattern::SEQUENTIAL;
    cfg.count = 10;
    cfg.start_addr = 0x0;
    cfg.addr_range = 256;  // 4 blocks of 64
    cfg.stride = 64;
    cfg.write_ratio = 0.0;
    cfg.seed = 42;

    auto trace = generate_trace(cfg);
    // After 4 strides (0, 64, 128, 192), next should wrap to 0
    ASSERT_EQ(trace[0].address, (uint64_t)0x0);
    ASSERT_EQ(trace[1].address, (uint64_t)64);
    ASSERT_EQ(trace[2].address, (uint64_t)128);
    ASSERT_EQ(trace[3].address, (uint64_t)192);
    ASSERT_EQ(trace[4].address, (uint64_t)0x0);  // wrap
}

TEST(TraceGen_RandomCount) {
    TraceGenConfig cfg;
    cfg.pattern = TracePattern::RANDOM;
    cfg.count = 500;
    cfg.seed = 42;

    auto trace = generate_trace(cfg);
    ASSERT_EQ(trace.size(), (size_t)500);
}

TEST(TraceGen_RandomAddressRange) {
    TraceGenConfig cfg;
    cfg.pattern = TracePattern::RANDOM;
    cfg.count = 1000;
    cfg.start_addr = 0x2000;
    cfg.addr_range = 0x1000;  // 4KB
    cfg.block_size = 64;
    cfg.seed = 42;

    auto trace = generate_trace(cfg);
    for (const auto& e : trace) {
        ASSERT_TRUE(e.address >= 0x2000);
        ASSERT_TRUE(e.address < 0x3000);
        // Should be block-aligned
        ASSERT_EQ(e.address % 64, (uint64_t)0);
    }
}

TEST(TraceGen_RandomWriteRatio) {
    TraceGenConfig cfg;
    cfg.pattern = TracePattern::RANDOM;
    cfg.count = 10000;
    cfg.write_ratio = 0.3;
    cfg.seed = 42;

    auto trace = generate_trace(cfg);
    size_t writes = 0;
    for (const auto& e : trace) {
        if (e.type == 'W') writes++;
    }
    // Should be approximately 30% ± tolerance
    double ratio = static_cast<double>(writes) / trace.size();
    ASSERT_TRUE(ratio > 0.25 && ratio < 0.35);
}

TEST(TraceGen_LocalityCount) {
    TraceGenConfig cfg;
    cfg.pattern = TracePattern::LOCALITY;
    cfg.count = 200;
    cfg.seed = 42;

    auto trace = generate_trace(cfg);
    ASSERT_EQ(trace.size(), (size_t)200);
}

TEST(TraceGen_LocalityHotspotBias) {
    // With high hotspot_ratio, most accesses should cluster
    TraceGenConfig cfg;
    cfg.pattern = TracePattern::LOCALITY;
    cfg.count = 10000;
    cfg.start_addr = 0x0;
    cfg.addr_range = 0x100000;  // 1MB
    cfg.block_size = 64;
    cfg.num_hotspots = 2;
    cfg.hotspot_ratio = 0.9;
    cfg.hotspot_size = 4096;  // 4KB each
    cfg.seed = 42;

    auto trace = generate_trace(cfg);

    // Count unique block addresses
    std::set<uint64_t> unique_addrs;
    for (const auto& e : trace) {
        unique_addrs.insert(e.address);
    }

    // With 90% locality in 2×4KB hotspots within 1MB,
    // the number of unique addresses should be much less than
    // the total possible (1MB/64 = 16384 blocks).
    // Hotspot blocks = 2 * 4096/64 = 128, plus ~10% global.
    ASSERT_TRUE(unique_addrs.size() < 2000);
}

TEST(TraceGen_AllReads) {
    TraceGenConfig cfg;
    cfg.pattern = TracePattern::RANDOM;
    cfg.count = 100;
    cfg.write_ratio = 0.0;
    cfg.seed = 42;

    auto trace = generate_trace(cfg);
    for (const auto& e : trace) {
        ASSERT_EQ(e.type, 'R');
    }
}

TEST(TraceGen_AllWrites) {
    TraceGenConfig cfg;
    cfg.pattern = TracePattern::RANDOM;
    cfg.count = 100;
    cfg.write_ratio = 1.0;
    cfg.seed = 42;

    auto trace = generate_trace(cfg);
    for (const auto& e : trace) {
        ASSERT_EQ(e.type, 'W');
    }
}

TEST(TraceGen_DeterministicSeed) {
    TraceGenConfig cfg;
    cfg.pattern = TracePattern::RANDOM;
    cfg.count = 50;
    cfg.seed = 12345;

    auto trace1 = generate_trace(cfg);
    auto trace2 = generate_trace(cfg);

    ASSERT_EQ(trace1.size(), trace2.size());
    for (size_t i = 0; i < trace1.size(); i++) {
        ASSERT_EQ(trace1[i].type, trace2[i].type);
        ASSERT_EQ(trace1[i].address, trace2[i].address);
    }
}

TEST(TraceGen_ValidationZeroCount) {
    TraceGenConfig cfg;
    cfg.count = 0;
    bool caught = false;
    try {
        generate_trace(cfg);
    } catch (const std::invalid_argument&) {
        caught = true;
    }
    ASSERT_TRUE(caught);
}

TEST(TraceGen_ValidationZeroBlockSize) {
    TraceGenConfig cfg;
    cfg.block_size = 0;
    bool caught = false;
    try {
        generate_trace(cfg);
    } catch (const std::invalid_argument&) {
        caught = true;
    }
    ASSERT_TRUE(caught);
}

TEST(TraceGen_ValidationBadWriteRatio) {
    TraceGenConfig cfg;
    cfg.write_ratio = 1.5;
    bool caught = false;
    try {
        generate_trace(cfg);
    } catch (const std::invalid_argument&) {
        caught = true;
    }
    ASSERT_TRUE(caught);
}
