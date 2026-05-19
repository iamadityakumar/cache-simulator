#include "report.h"
#include <iomanip>
#include <sstream>

// ============================================================================
// Helpers
// ============================================================================

// Format a number with comma separators: 1234567 -> "1,234,567"
static std::string format_number(uint64_t n) {
    std::string s = std::to_string(n);
    int insert_pos = static_cast<int>(s.length()) - 3;
    while (insert_pos > 0) {
        s.insert(insert_pos, ",");
        insert_pos -= 3;
    }
    return s;
}

// Format a percentage: 0.7234 -> "72.34%"
static std::string format_percent(double rate) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << (rate * 100.0) << "%";
    return oss.str();
}

// Right-pad or right-align a string in a field.
static std::string pad_right(const std::string& s, size_t width) {
    if (s.size() >= width) return s;
    return s + std::string(width - s.size(), ' ');
}

static std::string pad_left(const std::string& s, size_t width) {
    if (s.size() >= width) return s;
    return std::string(width - s.size(), ' ') + s;
}

// ============================================================================
// Table Report
// ============================================================================

void report_table(const MemoryHierarchy& hierarchy,
                  const SimConfig& config,
                  std::ostream& out) {
    const auto& stats = hierarchy.stats();

    // Header
    out << "\n";
    out << "  ============================================================\n";
    out << "                    Cache Simulation Results\n";
    out << "  ============================================================\n\n";

    // Configuration summary
    out << "  Configuration:\n";
    out << "    Block Size:    " << format_size(config.block_size) << "\n";
    out << "    Mem Latency:   " << config.mem_latency << " cycles\n\n";

    // Per-level table
    out << "  " << pad_right("Level", 7)
        << pad_right("Size", 10)
        << pad_right("Assoc", 8)
        << pad_right("Policy", 8)
        << pad_left("Hits", 10)
        << pad_left("Misses", 10)
        << pad_left("Hit Rate", 10) << "\n";
    out << "  " << std::string(63, '-') << "\n";

    // Cache level configs for display
    struct LevelDisplay {
        std::string name;
        size_t size;
        size_t assoc;
        std::string policy;
    };
    std::vector<LevelDisplay> displays = {
        {"L1", config.l1_size, config.l1_assoc, config.l1_policy},
        {"L2", config.l2_size, config.l2_assoc, config.l2_policy},
        {"L3", config.l3_size, config.l3_assoc, config.l3_policy}
    };

    for (size_t i = 0; i < stats.num_levels() && i < displays.size(); i++) {
        const auto& ls = stats.level(i);
        const auto& d = displays[i];

        out << "  " << pad_right(d.name, 7)
            << pad_right(format_size(d.size), 10)
            << pad_right(std::to_string(d.assoc) + "-way", 8)
            << pad_right(d.policy, 8)
            << pad_left(format_number(ls.hits), 10)
            << pad_left(format_number(ls.misses), 10)
            << pad_left(format_percent(ls.hit_rate()), 10) << "\n";
    }

    out << "  " << std::string(63, '-') << "\n\n";

    // Summary
    out << "  Summary:\n";
    out << "    Total Accesses:  " << format_number(stats.total_accesses()) << "\n";
    out << "    AMAT:            " << std::fixed << std::setprecision(2) << stats.amat() << " cycles\n";
    out << "\n  ============================================================\n\n";
}

// ============================================================================
// JSON Report
// ============================================================================

void report_json(const MemoryHierarchy& hierarchy,
                 const SimConfig& config,
                 std::ostream& out) {
    const auto& stats = hierarchy.stats();

    out << "{\n";
    out << "  \"config\": {\n";
    out << "    \"block_size\": " << config.block_size << ",\n";
    out << "    \"mem_latency\": " << config.mem_latency << "\n";
    out << "  },\n";

    out << "  \"levels\": [\n";

    struct LevelInfo {
        std::string name;
        size_t size;
        size_t assoc;
        std::string policy;
    };
    std::vector<LevelInfo> infos = {
        {"L1", config.l1_size, config.l1_assoc, config.l1_policy},
        {"L2", config.l2_size, config.l2_assoc, config.l2_policy},
        {"L3", config.l3_size, config.l3_assoc, config.l3_policy}
    };

    for (size_t i = 0; i < stats.num_levels() && i < infos.size(); i++) {
        const auto& ls = stats.level(i);
        const auto& info = infos[i];
        out << "    {\n";
        out << "      \"name\": \"" << info.name << "\",\n";
        out << "      \"size\": " << info.size << ",\n";
        out << "      \"associativity\": " << info.assoc << ",\n";
        out << "      \"policy\": \"" << info.policy << "\",\n";
        out << "      \"hits\": " << ls.hits << ",\n";
        out << "      \"misses\": " << ls.misses << ",\n";
        out << "      \"accesses\": " << ls.accesses << ",\n";
        out << "      \"hit_rate\": " << std::fixed << std::setprecision(6) << ls.hit_rate() << "\n";
        out << "    }";
        if (i + 1 < stats.num_levels() && i + 1 < infos.size()) out << ",";
        out << "\n";
    }

    out << "  ],\n";
    out << "  \"summary\": {\n";
    out << "    \"total_accesses\": " << stats.total_accesses() << ",\n";
    out << "    \"amat\": " << std::fixed << std::setprecision(4) << stats.amat() << "\n";
    out << "  }\n";
    out << "}\n";
}

// ============================================================================
// CSV Report
// ============================================================================

void report_csv(const MemoryHierarchy& hierarchy,
                const SimConfig& config,
                std::ostream& out) {
    const auto& stats = hierarchy.stats();

    // Header row
    out << "level,size_bytes,associativity,policy,hits,misses,accesses,hit_rate\n";

    struct LevelInfo {
        std::string name;
        size_t size;
        size_t assoc;
        std::string policy;
    };
    std::vector<LevelInfo> infos = {
        {"L1", config.l1_size, config.l1_assoc, config.l1_policy},
        {"L2", config.l2_size, config.l2_assoc, config.l2_policy},
        {"L3", config.l3_size, config.l3_assoc, config.l3_policy}
    };

    for (size_t i = 0; i < stats.num_levels() && i < infos.size(); i++) {
        const auto& ls = stats.level(i);
        const auto& info = infos[i];
        out << info.name << ","
            << info.size << ","
            << info.assoc << ","
            << info.policy << ","
            << ls.hits << ","
            << ls.misses << ","
            << ls.accesses << ","
            << std::fixed << std::setprecision(6) << ls.hit_rate() << "\n";
    }
}

// ============================================================================
// Verbose Per-Access Output
// ============================================================================

void print_verbose_access(size_t access_num,
                          char type,
                          uint64_t address,
                          const HierarchyAccessResult& result,
                          size_t num_levels,
                          std::ostream& out) {
    (void)num_levels;  // reserved for future level-name formatting
    out << "[" << std::setw(8) << access_num << "] "
        << type << " 0x" << std::hex << std::setw(12) << std::setfill('0') << address
        << std::dec << std::setfill(' ')
        << " -> ";

    if (result.hit) {
        // Level names: 0=L1, 1=L2, 2=L3
        std::string level_name = "L" + std::to_string(result.hit_level + 1);
        out << level_name << " HIT";
    } else {
        out << "MEMORY MISS";
    }

    out << "  (" << result.total_latency << " cycles)\n";
}
