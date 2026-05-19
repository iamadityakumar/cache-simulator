#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <stdexcept>

// Represents a single memory access from a trace.
struct TraceEntry {
    char     type;      // 'R' = read, 'W' = write
    uint64_t address;   // Byte address
};

// Error thrown when a trace file contains invalid content.
class TraceParseError : public std::runtime_error {
public:
    TraceParseError(const std::string& msg, size_t line_number)
        : std::runtime_error("Trace parse error at line " + std::to_string(line_number) + ": " + msg)
        , line_number_(line_number) {}

    size_t line_number() const { return line_number_; }

private:
    size_t line_number_;
};

// ============================================================================
// Trace File Parser
// ============================================================================

// Parse a .trace file from disk.
// Format per line: <R|W> <hex_address>
//   R 0x7fff5a3c1000
//   W 0x00400560
// Lines starting with '#' are comments. Blank lines are ignored.
// Throws TraceParseError on malformed input.
std::vector<TraceEntry> load_trace_file(const std::string& filepath);

// Parse trace entries from a string (same format as file).
// Useful for testing without writing to disk.
std::vector<TraceEntry> parse_trace_string(const std::string& content);

// ============================================================================
// Synthetic Trace Generator
// ============================================================================

// Pattern types for synthetic trace generation.
enum class TracePattern {
    SEQUENTIAL,   // Linear stride-based scan
    RANDOM,       // Uniform random within address range
    LOCALITY      // Hotspot-biased with temporal + spatial locality
};

// Convert string name to TracePattern enum.
// Accepts: "sequential", "random", "locality" (case-insensitive).
// Throws std::invalid_argument on unknown pattern.
TracePattern parse_pattern(const std::string& name);

// Configuration for synthetic trace generation.
struct TraceGenConfig {
    TracePattern pattern     = TracePattern::LOCALITY;
    size_t       count       = 10000;      // Number of accesses to generate
    uint64_t     start_addr  = 0x1000;     // Starting address of range
    uint64_t     addr_range  = 0x100000;   // Size of address space (bytes)
    size_t       block_size  = 64;         // Alignment granularity (bytes)
    double       write_ratio = 0.2;        // Fraction of writes (0.0–1.0)

    // Sequential-specific
    size_t       stride      = 64;         // Stride in bytes for sequential pattern

    // Locality-specific
    size_t       num_hotspots  = 4;        // Number of hotspot regions
    double       hotspot_ratio = 0.8;      // Fraction of accesses targeting hotspots
    uint64_t     hotspot_size  = 4096;     // Size of each hotspot region (bytes)

    // RNG seed (0 = use random device)
    uint64_t     seed        = 0;
};

// Generate a synthetic trace according to the given configuration.
std::vector<TraceEntry> generate_trace(const TraceGenConfig& config);

// Write a trace to a file in the standard .trace format.
void write_trace_file(const std::string& filepath, const std::vector<TraceEntry>& trace);
