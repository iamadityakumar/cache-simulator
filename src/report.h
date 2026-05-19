#pragma once

#include "config.h"
#include "memory_hierarchy.h"
#include <iostream>
#include <string>
#include <vector>

// Generate a pretty-printed table report to the given stream.
void report_table(const MemoryHierarchy& hierarchy,
                  const SimConfig& config,
                  std::ostream& out = std::cout);

// Generate a JSON report to the given stream.
void report_json(const MemoryHierarchy& hierarchy,
                 const SimConfig& config,
                 std::ostream& out = std::cout);

// Generate a CSV report to the given stream.
void report_csv(const MemoryHierarchy& hierarchy,
                const SimConfig& config,
                std::ostream& out = std::cout);

// Print a single verbose access line.
// Format: [access_num] type addr -> level (hit/miss) latency
void print_verbose_access(size_t access_num,
                          char type,
                          uint64_t address,
                          const HierarchyAccessResult& result,
                          size_t num_levels,
                          std::ostream& out = std::cout);
