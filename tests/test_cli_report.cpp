// test_cli_report.cpp — Tests for config parsing, CLI, and report generation
#include "test_framework.h"
#include "config.h"
#include "cli.h"
#include "report.h"
#include "memory_hierarchy.h"
#include "trace_loader.h"
#include <fstream>
#include <sstream>
#include <cstring>

// ============================================================================
// Size Parsing Tests
// ============================================================================

TEST(Config_ParseSizeBytes) {
    ASSERT_EQ(parse_size("64"), (size_t)64);
    ASSERT_EQ(parse_size("64B"), (size_t)64);
    ASSERT_EQ(parse_size("128b"), (size_t)128);
}

TEST(Config_ParseSizeKB) {
    ASSERT_EQ(parse_size("32KB"), (size_t)32768);
    ASSERT_EQ(parse_size("32kb"), (size_t)32768);
    ASSERT_EQ(parse_size("32K"), (size_t)32768);
    ASSERT_EQ(parse_size("1KB"), (size_t)1024);
}

TEST(Config_ParseSizeMB) {
    ASSERT_EQ(parse_size("8MB"), (size_t)(8 * 1024 * 1024));
    ASSERT_EQ(parse_size("1MB"), (size_t)(1024 * 1024));
    ASSERT_EQ(parse_size("8M"), (size_t)(8 * 1024 * 1024));
}

TEST(Config_ParseSizeGB) {
    ASSERT_EQ(parse_size("1GB"), (size_t)(1024ULL * 1024 * 1024));
    ASSERT_EQ(parse_size("1G"), (size_t)(1024ULL * 1024 * 1024));
}

TEST(Config_ParseSizeWithSpace) {
    ASSERT_EQ(parse_size("32 KB"), (size_t)32768);
}

TEST(Config_ParseSizeError_Empty) {
    bool caught = false;
    try { parse_size(""); } catch (const std::invalid_argument&) { caught = true; }
    ASSERT_TRUE(caught);
}

TEST(Config_ParseSizeError_NoNumber) {
    bool caught = false;
    try { parse_size("KB"); } catch (const std::invalid_argument&) { caught = true; }
    ASSERT_TRUE(caught);
}

TEST(Config_ParseSizeError_BadSuffix) {
    bool caught = false;
    try { parse_size("32XB"); } catch (const std::invalid_argument&) { caught = true; }
    ASSERT_TRUE(caught);
}

// ============================================================================
// Size Formatting Tests
// ============================================================================

TEST(Config_FormatSizeBytes) {
    ASSERT_EQ(format_size(64), std::string("64 B"));
    ASSERT_EQ(format_size(0), std::string("0 B"));
}

TEST(Config_FormatSizeKB) {
    ASSERT_EQ(format_size(32768), std::string("32 KB"));
    ASSERT_EQ(format_size(1024), std::string("1 KB"));
}

TEST(Config_FormatSizeMB) {
    ASSERT_EQ(format_size(8 * 1024 * 1024), std::string("8 MB"));
}

TEST(Config_FormatSizeGB) {
    ASSERT_EQ(format_size(1024ULL * 1024 * 1024), std::string("1 GB"));
}

// ============================================================================
// JSON Config File Tests
// ============================================================================

TEST(Config_LoadJsonFile) {
    std::string path = "d:/Website/cache-sim/traces/test_config.json";
    {
        std::ofstream f(path);
        f << "{\n"
          << "  \"l1_size\": \"64KB\",\n"
          << "  \"l1_assoc\": 4,\n"
          << "  \"l1_policy\": \"fifo\",\n"
          << "  \"mem_latency\": 300,\n"
          << "  \"gen_pattern\": \"random\"\n"
          << "}\n";
    }

    SimConfig config;
    load_config_file(path, config);

    ASSERT_EQ(config.l1_size, (size_t)(64 * 1024));
    ASSERT_EQ(config.l1_assoc, (size_t)4);
    ASSERT_EQ(config.l1_policy, std::string("fifo"));
    ASSERT_EQ(config.mem_latency, (uint64_t)300);
    ASSERT_EQ(config.gen_pattern, std::string("random"));

    // Unchanged defaults
    ASSERT_EQ(config.l2_size, (size_t)(256 * 1024));

    std::remove(path.c_str());
}

TEST(Config_LoadJsonError_NoFile) {
    bool caught = false;
    SimConfig config;
    try { load_config_file("nonexistent.json", config); }
    catch (const std::runtime_error&) { caught = true; }
    ASSERT_TRUE(caught);
}

// ============================================================================
// CLI Argument Parsing Tests
// ============================================================================

// Helper to simulate argc/argv from a command string.
struct ArgHelper {
    std::vector<std::string> strs;
    std::vector<char*> ptrs;

    ArgHelper(std::initializer_list<const char*> args) {
        for (auto a : args) {
            strs.emplace_back(a);
        }
        for (auto& s : strs) {
            ptrs.push_back(&s[0]);
        }
    }

    int argc() const { return static_cast<int>(ptrs.size()); }
    char** argv() { return ptrs.data(); }
};

TEST(CLI_DefaultConfig) {
    ArgHelper args({"cache-sim"});
    SimConfig config;
    parse_args(args.argc(), args.argv(), config);

    ASSERT_EQ(config.l1_size, (size_t)(32 * 1024));
    ASSERT_EQ(config.l1_assoc, (size_t)8);
    ASSERT_EQ(config.l1_policy, std::string("lru"));
    ASSERT_EQ(config.block_size, (size_t)64);
    ASSERT_FALSE(config.show_help);
    ASSERT_FALSE(config.verbose);
}

TEST(CLI_Help) {
    ArgHelper args({"cache-sim", "--help"});
    SimConfig config;
    parse_args(args.argc(), args.argv(), config);
    ASSERT_TRUE(config.show_help);
}

TEST(CLI_HelpShort) {
    ArgHelper args({"cache-sim", "-h"});
    SimConfig config;
    parse_args(args.argc(), args.argv(), config);
    ASSERT_TRUE(config.show_help);
}

TEST(CLI_L1Config) {
    ArgHelper args({"cache-sim", "--l1-size", "64KB", "--l1-assoc", "4", "--l1-policy", "fifo"});
    SimConfig config;
    parse_args(args.argc(), args.argv(), config);

    ASSERT_EQ(config.l1_size, (size_t)(64 * 1024));
    ASSERT_EQ(config.l1_assoc, (size_t)4);
    ASSERT_EQ(config.l1_policy, std::string("fifo"));
}

TEST(CLI_TraceFile) {
    ArgHelper args({"cache-sim", "--trace", "my_trace.trace"});
    SimConfig config;
    parse_args(args.argc(), args.argv(), config);
    ASSERT_EQ(config.trace_file, std::string("my_trace.trace"));
}

TEST(CLI_Generate) {
    ArgHelper args({"cache-sim", "--generate", "--gen-count", "5000", "--gen-pattern", "random"});
    SimConfig config;
    parse_args(args.argc(), args.argv(), config);

    ASSERT_TRUE(config.generate);
    ASSERT_EQ(config.gen_count, (size_t)5000);
    ASSERT_EQ(config.gen_pattern, std::string("random"));
}

TEST(CLI_OutputFormat) {
    ArgHelper args({"cache-sim", "--output", "json"});
    SimConfig config;
    parse_args(args.argc(), args.argv(), config);
    ASSERT_EQ(config.output_format, std::string("json"));
}

TEST(CLI_Verbose) {
    ArgHelper args({"cache-sim", "--verbose"});
    SimConfig config;
    parse_args(args.argc(), args.argv(), config);
    ASSERT_TRUE(config.verbose);
}

TEST(CLI_BlockSizeAndMemLatency) {
    ArgHelper args({"cache-sim", "--block-size", "128", "--mem-latency", "500"});
    SimConfig config;
    parse_args(args.argc(), args.argv(), config);
    ASSERT_EQ(config.block_size, (size_t)128);
    ASSERT_EQ(config.mem_latency, (uint64_t)500);
}

TEST(CLI_UnknownArg) {
    ArgHelper args({"cache-sim", "--bogus"});
    SimConfig config;
    bool caught = false;
    try { parse_args(args.argc(), args.argv(), config); }
    catch (const std::invalid_argument&) { caught = true; }
    ASSERT_TRUE(caught);
}

TEST(CLI_MissingValue) {
    ArgHelper args({"cache-sim", "--l1-size"});
    SimConfig config;
    bool caught = false;
    try { parse_args(args.argc(), args.argv(), config); }
    catch (const std::invalid_argument&) { caught = true; }
    ASSERT_TRUE(caught);
}

TEST(CLI_FullConfig) {
    ArgHelper args({"cache-sim",
        "--l1-size", "16KB", "--l1-assoc", "2", "--l1-policy", "fifo",
        "--l2-size", "128KB", "--l2-assoc", "4", "--l2-policy", "lru",
        "--l3-size", "4MB", "--l3-assoc", "8", "--l3-policy", "lru",
        "--block-size", "64", "--mem-latency", "100",
        "--generate", "--gen-count", "1000", "--gen-pattern", "sequential",
        "--output", "csv", "--verbose"
    });
    SimConfig config;
    parse_args(args.argc(), args.argv(), config);

    ASSERT_EQ(config.l1_size, (size_t)(16 * 1024));
    ASSERT_EQ(config.l1_assoc, (size_t)2);
    ASSERT_EQ(config.l1_policy, std::string("fifo"));
    ASSERT_EQ(config.l2_size, (size_t)(128 * 1024));
    ASSERT_EQ(config.l3_size, (size_t)(4 * 1024 * 1024));
    ASSERT_EQ(config.l3_assoc, (size_t)8);
    ASSERT_EQ(config.mem_latency, (uint64_t)100);
    ASSERT_TRUE(config.generate);
    ASSERT_EQ(config.gen_count, (size_t)1000);
    ASSERT_EQ(config.gen_pattern, std::string("sequential"));
    ASSERT_EQ(config.output_format, std::string("csv"));
    ASSERT_TRUE(config.verbose);
}

// ============================================================================
// Report Generation Tests (output to stringstream)
// ============================================================================

// Helper: build a small hierarchy and run a trace
static MemoryHierarchy build_test_hierarchy() {
    std::vector<CacheLevelConfig> levels = {
        {"L1", 256, 64, 2, 1, "lru"},
        {"L2", 1024, 64, 4, 10, "lru"},
        {"L3", 4096, 64, 8, 40, "lru"}
    };
    return MemoryHierarchy(levels, 200);
}

static void run_small_trace(MemoryHierarchy& mh) {
    mh.read(0x1000);  // cold miss
    mh.read(0x2000);  // cold miss
    mh.read(0x1000);  // hit L1
    mh.write(0x3000); // cold miss
    mh.read(0x3000);  // hit L1
}

TEST(Report_TableContainsHeaders) {
    auto mh = build_test_hierarchy();
    run_small_trace(mh);

    SimConfig config;
    std::ostringstream oss;
    report_table(mh, config, oss);
    std::string output = oss.str();

    // Should contain key elements
    ASSERT_TRUE(output.find("Cache Simulation Results") != std::string::npos);
    ASSERT_TRUE(output.find("Level") != std::string::npos);
    ASSERT_TRUE(output.find("Hit Rate") != std::string::npos);
    ASSERT_TRUE(output.find("L1") != std::string::npos);
    ASSERT_TRUE(output.find("AMAT") != std::string::npos);
}

TEST(Report_JsonValid) {
    auto mh = build_test_hierarchy();
    run_small_trace(mh);

    SimConfig config;
    std::ostringstream oss;
    report_json(mh, config, oss);
    std::string output = oss.str();

    // Basic structural checks
    ASSERT_TRUE(output.find("{") != std::string::npos);
    ASSERT_TRUE(output.find("\"levels\"") != std::string::npos);
    ASSERT_TRUE(output.find("\"hits\"") != std::string::npos);
    ASSERT_TRUE(output.find("\"misses\"") != std::string::npos);
    ASSERT_TRUE(output.find("\"amat\"") != std::string::npos);
    ASSERT_TRUE(output.find("\"summary\"") != std::string::npos);
}

TEST(Report_CsvHeaders) {
    auto mh = build_test_hierarchy();
    run_small_trace(mh);

    SimConfig config;
    std::ostringstream oss;
    report_csv(mh, config, oss);
    std::string output = oss.str();

    // First line should be CSV header
    ASSERT_TRUE(output.find("level,size_bytes,associativity,policy,hits,misses,accesses,hit_rate") != std::string::npos);
    // Should contain level data
    ASSERT_TRUE(output.find("L1,") != std::string::npos);
    ASSERT_TRUE(output.find("L2,") != std::string::npos);
    ASSERT_TRUE(output.find("L3,") != std::string::npos);
}

TEST(Report_CsvRowCount) {
    auto mh = build_test_hierarchy();
    run_small_trace(mh);

    SimConfig config;
    std::ostringstream oss;
    report_csv(mh, config, oss);
    std::string output = oss.str();

    // Count newlines: header + 3 data rows = 4 lines
    size_t newlines = 0;
    for (char c : output) {
        if (c == '\n') newlines++;
    }
    ASSERT_EQ(newlines, (size_t)4);
}

TEST(Report_VerboseOutput) {
    auto mh = build_test_hierarchy();
    std::ostringstream oss;

    auto result = mh.read(0x1000);
    print_verbose_access(1, 'R', 0x1000, result, mh.num_levels(), oss);
    std::string output = oss.str();

    ASSERT_TRUE(output.find("MEMORY MISS") != std::string::npos);
    ASSERT_TRUE(output.find("1000") != std::string::npos);

    // Now hit
    std::ostringstream oss2;
    auto result2 = mh.read(0x1000);
    print_verbose_access(2, 'R', 0x1000, result2, mh.num_levels(), oss2);
    std::string output2 = oss2.str();
    ASSERT_TRUE(output2.find("L1 HIT") != std::string::npos);
}

TEST(Report_TableShowsCorrectStats) {
    auto mh = build_test_hierarchy();
    // 5 accesses: 3 cold misses + 2 hits
    run_small_trace(mh);

    SimConfig config;
    std::ostringstream oss;
    report_table(mh, config, oss);
    std::string output = oss.str();

    // Total accesses should be 5
    ASSERT_TRUE(output.find("5") != std::string::npos);
}

TEST(Report_JsonShowsStats) {
    auto mh = build_test_hierarchy();
    run_small_trace(mh);

    SimConfig config;
    std::ostringstream oss;
    report_json(mh, config, oss);
    std::string output = oss.str();

    // total_accesses = 5
    ASSERT_TRUE(output.find("\"total_accesses\": 5") != std::string::npos);
}
