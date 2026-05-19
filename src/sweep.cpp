#include "sweep.h"
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <stdexcept>

// ============================================================================
// Default Sweep Configuration
// ============================================================================

SweepConfig default_sweep_config() {
    SweepConfig cfg;
    cfg.l1_sizes        = {16 * 1024, 32 * 1024, 64 * 1024};                          // 16KB, 32KB, 64KB
    cfg.l2_sizes        = {128 * 1024, 256 * 1024, 512 * 1024};                       // 128KB, 256KB, 512KB
    cfg.l3_sizes        = {4 * 1024 * 1024, 8 * 1024 * 1024, 16ULL * 1024 * 1024};   // 4MB, 8MB, 16MB
    cfg.policies        = {"lru", "fifo"};
    cfg.associativities = {4, 8, 16};
    return cfg;
}

// ============================================================================
// Helpers
// ============================================================================

// Format a percentage.
static std::string fmt_pct(double rate) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << (rate * 100.0) << "%";
    return oss.str();
}

// Format bytes to human-readable (reuse config.h's format_size).
// We include config.h through sweep.h -> config.h.

// Right-pad a string to a given width.
static std::string rpad(const std::string& s, size_t w) {
    if (s.size() >= w) return s;
    return s + std::string(w - s.size(), ' ');
}

// Left-pad a string to a given width.
static std::string lpad(const std::string& s, size_t w) {
    if (s.size() >= w) return s;
    return std::string(w - s.size(), ' ') + s;
}

// ============================================================================
// Run a single configuration and collect results
// ============================================================================

static SweepResult run_single(const SimConfig& base,
                               size_t l1_sz, size_t l2_sz, size_t l3_sz,
                               size_t assoc, const std::string& policy,
                               const std::vector<TraceEntry>& trace) {
    // Build hierarchy with the specified params
    std::vector<CacheLevelConfig> levels = {
        {"L1", l1_sz, base.block_size, assoc, base.l1_latency, policy},
        {"L2", l2_sz, base.block_size, assoc, base.l2_latency, policy},
        {"L3", l3_sz, base.block_size, assoc, base.l3_latency, policy}
    };

    MemoryHierarchy hierarchy(levels, base.mem_latency);

    // Replay trace
    for (const auto& entry : trace) {
        hierarchy.access(entry.address, entry.type);
    }

    // Collect results
    SweepResult result;
    result.l1_size        = l1_sz;
    result.l2_size        = l2_sz;
    result.l3_size        = l3_sz;
    result.associativity  = assoc;
    result.policy         = policy;

    const auto& stats = hierarchy.stats();
    result.l1_hit_rate    = stats.level(0).hit_rate();
    result.l2_hit_rate    = stats.level(1).hit_rate();
    result.l3_hit_rate    = stats.level(2).hit_rate();
    result.amat           = stats.amat();
    result.total_accesses = stats.total_accesses();

    return result;
}

// ============================================================================
// Sweep Engine
// ============================================================================

std::vector<SweepResult> run_sweep(const SweepConfig& sweep_cfg,
                                   const SimConfig& base_config,
                                   const std::vector<TraceEntry>& trace) {
    std::vector<SweepResult> results;

    // Cartesian product of all dimensions
    for (size_t l1_sz : sweep_cfg.l1_sizes) {
        for (size_t l2_sz : sweep_cfg.l2_sizes) {
            for (size_t l3_sz : sweep_cfg.l3_sizes) {
                for (size_t assoc : sweep_cfg.associativities) {
                    for (const auto& policy : sweep_cfg.policies) {
                        results.push_back(
                            run_single(base_config, l1_sz, l2_sz, l3_sz,
                                       assoc, policy, trace));
                    }
                }
            }
        }
    }

    // Sort by AMAT ascending (best first)
    std::sort(results.begin(), results.end(),
              [](const SweepResult& a, const SweepResult& b) {
                  return a.amat < b.amat;
              });

    return results;
}

SweepResult find_optimal(const std::vector<SweepResult>& results) {
    if (results.empty()) {
        throw std::invalid_argument("find_optimal: results must not be empty");
    }
    // Results are already sorted by AMAT ascending, so the first is optimal.
    return results[0];
}

// ============================================================================
// Sweep Table Report
// ============================================================================

void report_sweep_table(const std::vector<SweepResult>& results,
                        std::ostream& out) {
    out << "\n";
    out << "  ================================================================"
           "============================\n";
    out << "                           Parameter Sweep Results\n";
    out << "  ================================================================"
           "============================\n\n";

    out << "  " << rpad("Rank", 6)
        << rpad("L1", 9)
        << rpad("L2", 9)
        << rpad("L3", 9)
        << rpad("Assoc", 7)
        << rpad("Policy", 8)
        << lpad("L1 Hit%", 9)
        << lpad("L2 Hit%", 9)
        << lpad("L3 Hit%", 9)
        << lpad("AMAT", 12) << "\n";
    out << "  " << std::string(87, '-') << "\n";

    for (size_t i = 0; i < results.size(); i++) {
        const auto& r = results[i];
        std::string rank = "#" + std::to_string(i + 1);
        std::ostringstream amat_str;
        amat_str << std::fixed << std::setprecision(2) << r.amat;

        out << "  " << rpad(rank, 6)
            << rpad(format_size(r.l1_size), 9)
            << rpad(format_size(r.l2_size), 9)
            << rpad(format_size(r.l3_size), 9)
            << rpad(std::to_string(r.associativity) + "-way", 7)
            << rpad(r.policy, 8)
            << lpad(fmt_pct(r.l1_hit_rate), 9)
            << lpad(fmt_pct(r.l2_hit_rate), 9)
            << lpad(fmt_pct(r.l3_hit_rate), 9)
            << lpad(amat_str.str(), 12) << "\n";
    }

    out << "  " << std::string(87, '-') << "\n\n";

    // Highlight optimal
    if (!results.empty()) {
        const auto& best = results[0];
        out << "  >>> OPTIMAL CONFIGURATION <<<\n";
        out << "    L1: " << format_size(best.l1_size) << "  |  "
            << "L2: " << format_size(best.l2_size) << "  |  "
            << "L3: " << format_size(best.l3_size) << "\n";
        out << "    Associativity: " << best.associativity << "-way  |  "
            << "Policy: " << best.policy << "\n";

        std::ostringstream amat_out;
        amat_out << std::fixed << std::setprecision(2) << best.amat;
        out << "    AMAT: " << amat_out.str() << " cycles\n";
    }

    out << "\n  ============================================================"
           "================================\n\n";
}

// ============================================================================
// Sweep CSV Report
// ============================================================================

void report_sweep_csv(const std::vector<SweepResult>& results,
                      std::ostream& out) {
    out << "rank,l1_size,l2_size,l3_size,associativity,policy,"
           "l1_hit_rate,l2_hit_rate,l3_hit_rate,amat,total_accesses\n";

    for (size_t i = 0; i < results.size(); i++) {
        const auto& r = results[i];
        out << (i + 1) << ","
            << r.l1_size << ","
            << r.l2_size << ","
            << r.l3_size << ","
            << r.associativity << ","
            << r.policy << ","
            << std::fixed << std::setprecision(6)
            << r.l1_hit_rate << ","
            << r.l2_hit_rate << ","
            << r.l3_hit_rate << ","
            << std::setprecision(4) << r.amat << ","
            << r.total_accesses << "\n";
    }
}
