#pragma once

#include <cstdint>
#include <string>
#include <stdexcept>

// ============================================================================
// Size Parsing — "32KB" → 32768, "8MB" → 8388608
// ============================================================================

// Parse a human-readable size string into bytes.
// Supported suffixes (case-insensitive): B, KB, MB, GB
// If no suffix, treats the value as bytes.
// Examples: "32KB" → 32768, "8MB" → 8388608, "64" → 64
// Throws std::invalid_argument on malformed input.
size_t parse_size(const std::string& size_str);

// Format bytes into a human-readable string (inverse of parse_size).
// Examples: 32768 → "32 KB", 8388608 → "8 MB", 64 → "64 B"
std::string format_size(size_t bytes);

// ============================================================================
// Simulation Configuration
// ============================================================================

struct SimConfig {
    // L1 cache
    size_t      l1_size     = 32 * 1024;        // 32 KB
    size_t      l1_assoc    = 8;
    std::string l1_policy   = "lru";
    uint64_t    l1_latency  = 1;                // cycles

    // L2 cache
    size_t      l2_size     = 256 * 1024;       // 256 KB
    size_t      l2_assoc    = 8;
    std::string l2_policy   = "lru";
    uint64_t    l2_latency  = 10;

    // L3 cache
    size_t      l3_size     = 8 * 1024 * 1024;  // 8 MB
    size_t      l3_assoc    = 16;
    std::string l3_policy   = "lru";
    uint64_t    l3_latency  = 40;

    // Common
    size_t      block_size  = 64;               // bytes per cache line
    uint64_t    mem_latency = 200;              // main memory latency (cycles)

    // Trace source
    std::string trace_file;                     // path to .trace file
    bool        generate    = false;            // generate synthetic trace
    size_t      gen_count   = 10000;            // number of accesses to generate
    std::string gen_pattern = "locality";       // sequential / random / locality
    uint64_t    gen_seed    = 0;                // RNG seed (0 = random)

    // Output
    std::string output_format = "table";        // table / json / csv
    bool        verbose     = false;            // per-access logging

    // Advanced modes (Phase 5)
    bool        sweep       = false;            // parameter sweep mode
    bool        compare     = false;            // LRU vs FIFO comparison

    // Meta
    bool        show_help   = false;
    std::string config_file;                    // JSON config file path
};

// Load configuration from a simple JSON file.
// Supports a flat JSON object with string and number values.
// Overlays values onto an existing config (only specified keys are updated).
// Throws std::runtime_error on file I/O errors.
// Throws std::invalid_argument on parse errors.
void load_config_file(const std::string& filepath, SimConfig& config);
