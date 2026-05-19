#pragma once

#include "config.h"
#include "memory_hierarchy.h"
#include "sweep.h"
#include "trace_loader.h"
#include <iostream>
#include <string>
#include <vector>

// ============================================================================
// Compare Result — side-by-side LRU vs FIFO comparison
// ============================================================================

struct CompareResult {
    SweepResult lru_result;     // Stats when all levels use LRU
    SweepResult fifo_result;    // Stats when all levels use FIFO
    double      amat_diff;      // fifo_amat - lru_amat (positive = LRU wins)
    std::string winner;         // "LRU" or "FIFO"
};

// ============================================================================
// Compare API
// ============================================================================

// Run the same trace through two identical hierarchies — one all-LRU,
// one all-FIFO — and compare results.
CompareResult run_compare(const SimConfig& config,
                          const std::vector<TraceEntry>& trace);

// ============================================================================
// Compare Reporting
// ============================================================================

// Print a formatted side-by-side comparison table.
void report_compare(const CompareResult& result,
                    std::ostream& out = std::cout);
