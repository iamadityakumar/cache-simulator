#include "compare.h"
#include <cmath>
#include <iomanip>
#include <sstream>

// ============================================================================
// Helpers (local)
// ============================================================================

static std::string cmp_fmt_pct(double rate) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << (rate * 100.0) << "%";
    return oss.str();
}

static std::string cmp_rpad(const std::string& s, size_t w) {
    if (s.size() >= w) return s;
    return s + std::string(w - s.size(), ' ');
}

static std::string cmp_lpad(const std::string& s, size_t w) {
    if (s.size() >= w) return s;
    return std::string(w - s.size(), ' ') + s;
}

// ============================================================================
// Run Compare — LRU vs FIFO
// ============================================================================

CompareResult run_compare(const SimConfig& config,
                          const std::vector<TraceEntry>& trace) {
    // Build two hierarchies: one all-LRU, one all-FIFO
    std::vector<CacheLevelConfig> lru_levels = {
        {"L1", config.l1_size, config.block_size, config.l1_assoc,
         config.l1_latency, "lru"},
        {"L2", config.l2_size, config.block_size, config.l2_assoc,
         config.l2_latency, "lru"},
        {"L3", config.l3_size, config.block_size, config.l3_assoc,
         config.l3_latency, "lru"}
    };

    std::vector<CacheLevelConfig> fifo_levels = {
        {"L1", config.l1_size, config.block_size, config.l1_assoc,
         config.l1_latency, "fifo"},
        {"L2", config.l2_size, config.block_size, config.l2_assoc,
         config.l2_latency, "fifo"},
        {"L3", config.l3_size, config.block_size, config.l3_assoc,
         config.l3_latency, "fifo"}
    };

    MemoryHierarchy lru_mh(lru_levels, config.mem_latency);
    MemoryHierarchy fifo_mh(fifo_levels, config.mem_latency);

    // Replay the same trace through both
    for (const auto& entry : trace) {
        lru_mh.access(entry.address, entry.type);
        fifo_mh.access(entry.address, entry.type);
    }

    // Collect LRU result
    CompareResult result;
    {
        const auto& s = lru_mh.stats();
        result.lru_result.l1_size        = config.l1_size;
        result.lru_result.l2_size        = config.l2_size;
        result.lru_result.l3_size        = config.l3_size;
        result.lru_result.associativity  = config.l1_assoc;
        result.lru_result.policy         = "lru";
        result.lru_result.l1_hit_rate    = s.level(0).hit_rate();
        result.lru_result.l2_hit_rate    = s.level(1).hit_rate();
        result.lru_result.l3_hit_rate    = s.level(2).hit_rate();
        result.lru_result.amat           = s.amat();
        result.lru_result.total_accesses = s.total_accesses();
    }

    // Collect FIFO result
    {
        const auto& s = fifo_mh.stats();
        result.fifo_result.l1_size        = config.l1_size;
        result.fifo_result.l2_size        = config.l2_size;
        result.fifo_result.l3_size        = config.l3_size;
        result.fifo_result.associativity  = config.l1_assoc;
        result.fifo_result.policy         = "fifo";
        result.fifo_result.l1_hit_rate    = s.level(0).hit_rate();
        result.fifo_result.l2_hit_rate    = s.level(1).hit_rate();
        result.fifo_result.l3_hit_rate    = s.level(2).hit_rate();
        result.fifo_result.amat           = s.amat();
        result.fifo_result.total_accesses = s.total_accesses();
    }

    // Compute comparison
    result.amat_diff = result.fifo_result.amat - result.lru_result.amat;
    result.winner    = (result.lru_result.amat <= result.fifo_result.amat)
                           ? "LRU" : "FIFO";

    return result;
}

// ============================================================================
// Compare Report — side-by-side table
// ============================================================================

void report_compare(const CompareResult& result, std::ostream& out) {
    out << "\n";
    out << "  ============================================================\n";
    out << "               LRU vs FIFO — Side-by-Side Comparison\n";
    out << "  ============================================================\n\n";

    // Configuration summary
    out << "  Configuration:\n";
    out << "    L1: " << format_size(result.lru_result.l1_size)
        << "  " << result.lru_result.associativity << "-way\n";
    out << "    L2: " << format_size(result.lru_result.l2_size)
        << "  " << result.lru_result.associativity << "-way\n";
    out << "    L3: " << format_size(result.lru_result.l3_size)
        << "  " << result.lru_result.associativity << "-way\n";
    out << "    Accesses: " << result.lru_result.total_accesses << "\n\n";

    // Side-by-side table
    out << "  " << cmp_rpad("Metric", 22)
        << cmp_lpad("LRU", 14)
        << cmp_lpad("FIFO", 14)
        << cmp_lpad("Diff", 14) << "\n";
    out << "  " << std::string(64, '-') << "\n";

    // L1 Hit Rate
    {
        double diff = result.fifo_result.l1_hit_rate - result.lru_result.l1_hit_rate;
        std::ostringstream diff_oss;
        diff_oss << std::fixed << std::setprecision(2) << std::showpos
                 << (diff * 100.0) << "%";
        out << "  " << cmp_rpad("L1 Hit Rate", 22)
            << cmp_lpad(cmp_fmt_pct(result.lru_result.l1_hit_rate), 14)
            << cmp_lpad(cmp_fmt_pct(result.fifo_result.l1_hit_rate), 14)
            << cmp_lpad(diff_oss.str(), 14) << "\n";
    }

    // L2 Hit Rate
    {
        double diff = result.fifo_result.l2_hit_rate - result.lru_result.l2_hit_rate;
        std::ostringstream diff_oss;
        diff_oss << std::fixed << std::setprecision(2) << std::showpos
                 << (diff * 100.0) << "%";
        out << "  " << cmp_rpad("L2 Hit Rate", 22)
            << cmp_lpad(cmp_fmt_pct(result.lru_result.l2_hit_rate), 14)
            << cmp_lpad(cmp_fmt_pct(result.fifo_result.l2_hit_rate), 14)
            << cmp_lpad(diff_oss.str(), 14) << "\n";
    }

    // L3 Hit Rate
    {
        double diff = result.fifo_result.l3_hit_rate - result.lru_result.l3_hit_rate;
        std::ostringstream diff_oss;
        diff_oss << std::fixed << std::setprecision(2) << std::showpos
                 << (diff * 100.0) << "%";
        out << "  " << cmp_rpad("L3 Hit Rate", 22)
            << cmp_lpad(cmp_fmt_pct(result.lru_result.l3_hit_rate), 14)
            << cmp_lpad(cmp_fmt_pct(result.fifo_result.l3_hit_rate), 14)
            << cmp_lpad(diff_oss.str(), 14) << "\n";
    }

    // AMAT
    {
        std::ostringstream lru_oss, fifo_oss, diff_oss;
        lru_oss  << std::fixed << std::setprecision(2) << result.lru_result.amat;
        fifo_oss << std::fixed << std::setprecision(2) << result.fifo_result.amat;
        diff_oss << std::fixed << std::setprecision(2) << std::showpos
                 << result.amat_diff;
        out << "  " << cmp_rpad("AMAT (cycles)", 22)
            << cmp_lpad(lru_oss.str(), 14)
            << cmp_lpad(fifo_oss.str(), 14)
            << cmp_lpad(diff_oss.str(), 14) << "\n";
    }

    out << "  " << std::string(64, '-') << "\n\n";

    // Winner announcement
    out << "  >>> WINNER: " << result.winner << " <<<\n";
    if (result.winner == "LRU") {
        std::ostringstream adv;
        adv << std::fixed << std::setprecision(2) << std::abs(result.amat_diff);
        out << "    LRU outperforms FIFO by " << adv.str()
            << " cycles AMAT.\n";
    } else {
        std::ostringstream adv;
        adv << std::fixed << std::setprecision(2) << std::abs(result.amat_diff);
        out << "    FIFO outperforms LRU by " << adv.str()
            << " cycles AMAT.\n";
    }

    out << "\n  ============================================================\n\n";
}
