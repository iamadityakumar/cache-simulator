#pragma once

#include "config.h"
#include "memory_hierarchy.h"
#include "trace_loader.h"
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

// ============================================================================
// Sweep Configuration — defines the parameter space to explore
// ============================================================================

struct SweepConfig {
    // Dimensions to sweep (Cartesian product of all non-empty vectors)
    std::vector<size_t>      l1_sizes;        // e.g., {16KB, 32KB, 64KB}
    std::vector<size_t>      l2_sizes;        // e.g., {128KB, 256KB, 512KB}
    std::vector<size_t>      l3_sizes;        // e.g., {4MB, 8MB, 16MB}
    std::vector<std::string> policies;        // e.g., {"lru", "fifo"}
    std::vector<size_t>      associativities; // e.g., {4, 8, 16}
};

// Returns a default sweep configuration with sensible ranges.
SweepConfig default_sweep_config();

// ============================================================================
// Sweep Result — one row of output from a parameter sweep
// ============================================================================

struct SweepResult {
    // Configuration used
    size_t      l1_size;
    size_t      l2_size;
    size_t      l3_size;
    size_t      associativity;
    std::string policy;

    // Performance metrics
    double   l1_hit_rate;
    double   l2_hit_rate;
    double   l3_hit_rate;
    double   amat;
    uint64_t total_accesses;
};

// ============================================================================
// Sweep API
// ============================================================================

// Run a full parameter sweep over the Cartesian product of all dimensions.
// For each combination, builds a MemoryHierarchy, replays the trace, and
// records the results.  Returns results sorted by AMAT ascending.
std::vector<SweepResult> run_sweep(const SweepConfig& sweep_cfg,
                                   const SimConfig& base_config,
                                   const std::vector<TraceEntry>& trace);

// Find the optimal configuration (lowest AMAT) from sweep results.
// Precondition: results must not be empty.
SweepResult find_optimal(const std::vector<SweepResult>& results);

// ============================================================================
// Sweep Reporting
// ============================================================================

// Pretty-printed table of sweep results with an optimal-config highlight.
void report_sweep_table(const std::vector<SweepResult>& results,
                        std::ostream& out = std::cout);

// CSV output of sweep results (machine-readable).
void report_sweep_csv(const std::vector<SweepResult>& results,
                      std::ostream& out = std::cout);
