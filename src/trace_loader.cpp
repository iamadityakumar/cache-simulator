#include "trace_loader.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <random>
#include <sstream>

// ============================================================================
// Internal helpers
// ============================================================================

// Trim leading and trailing whitespace from a string.
static std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

// Parse a single trace line into a TraceEntry.
// Returns true on success, false if line should be skipped (blank/comment).
// Throws TraceParseError on malformed input.
static bool parse_line(const std::string& raw_line, size_t line_number, TraceEntry& entry) {
    std::string line = trim(raw_line);

    // Skip blank lines and comments
    if (line.empty() || line[0] == '#') {
        return false;
    }

    // Expect format: <R|W> <hex_address>
    std::istringstream iss(line);
    std::string type_str;
    std::string addr_str;

    if (!(iss >> type_str >> addr_str)) {
        throw TraceParseError("expected '<R|W> <address>', got: '" + line + "'", line_number);
    }

    // Check for extra tokens on the line
    std::string extra;
    if (iss >> extra) {
        // Allow inline comments after the address
        if (extra[0] != '#') {
            throw TraceParseError("unexpected token '" + extra + "' after address", line_number);
        }
    }

    // Parse type
    if (type_str.size() != 1) {
        throw TraceParseError("access type must be 'R' or 'W', got: '" + type_str + "'", line_number);
    }
    char type_char = static_cast<char>(std::toupper(static_cast<unsigned char>(type_str[0])));
    if (type_char != 'R' && type_char != 'W') {
        throw TraceParseError("access type must be 'R' or 'W', got: '" + type_str + "'", line_number);
    }
    entry.type = type_char;

    // Parse address (hex, with or without 0x prefix)
    try {
        size_t pos = 0;
        entry.address = std::stoull(addr_str, &pos, 16);
        if (pos != addr_str.size()) {
            throw TraceParseError("invalid hex address: '" + addr_str + "'", line_number);
        }
    } catch (const std::invalid_argument&) {
        throw TraceParseError("invalid hex address: '" + addr_str + "'", line_number);
    } catch (const std::out_of_range&) {
        throw TraceParseError("address out of range: '" + addr_str + "'", line_number);
    }

    return true;
}

// ============================================================================
// Trace File Parser
// ============================================================================

std::vector<TraceEntry> load_trace_file(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open trace file: " + filepath);
    }

    std::vector<TraceEntry> entries;
    std::string line;
    size_t line_number = 0;

    while (std::getline(file, line)) {
        line_number++;
        TraceEntry entry;
        if (parse_line(line, line_number, entry)) {
            entries.push_back(entry);
        }
    }

    if (entries.empty()) {
        throw TraceParseError("trace file is empty or contains no valid entries", 0);
    }

    return entries;
}

std::vector<TraceEntry> parse_trace_string(const std::string& content) {
    std::vector<TraceEntry> entries;
    std::istringstream stream(content);
    std::string line;
    size_t line_number = 0;

    while (std::getline(stream, line)) {
        line_number++;
        TraceEntry entry;
        if (parse_line(line, line_number, entry)) {
            entries.push_back(entry);
        }
    }

    return entries;
}

// ============================================================================
// Pattern parsing
// ============================================================================

TracePattern parse_pattern(const std::string& name) {
    // Convert to lowercase for comparison
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (lower == "sequential") return TracePattern::SEQUENTIAL;
    if (lower == "random")     return TracePattern::RANDOM;
    if (lower == "locality")   return TracePattern::LOCALITY;

    throw std::invalid_argument("Unknown trace pattern: '" + name + "'. "
                                "Expected: sequential, random, or locality");
}

// ============================================================================
// Synthetic Trace Generator
// ============================================================================

// Generate a sequential (stride-based) trace.
static std::vector<TraceEntry> generate_sequential(const TraceGenConfig& config, std::mt19937_64& rng) {
    std::vector<TraceEntry> trace;
    trace.reserve(config.count);

    std::uniform_real_distribution<double> write_dist(0.0, 1.0);
    uint64_t addr = config.start_addr;

    for (size_t i = 0; i < config.count; i++) {
        TraceEntry entry;
        entry.type = (write_dist(rng) < config.write_ratio) ? 'W' : 'R';
        entry.address = addr;
        trace.push_back(entry);

        addr += config.stride;
        // Wrap around within the address range
        if (addr >= config.start_addr + config.addr_range) {
            addr = config.start_addr;
        }
    }

    return trace;
}

// Generate a random (uniform) trace.
static std::vector<TraceEntry> generate_random(const TraceGenConfig& config, std::mt19937_64& rng) {
    std::vector<TraceEntry> trace;
    trace.reserve(config.count);

    // Generate block-aligned addresses within the range
    uint64_t num_blocks = config.addr_range / config.block_size;
    if (num_blocks == 0) num_blocks = 1;

    std::uniform_int_distribution<uint64_t> addr_dist(0, num_blocks - 1);
    std::uniform_real_distribution<double> write_dist(0.0, 1.0);

    for (size_t i = 0; i < config.count; i++) {
        TraceEntry entry;
        entry.type = (write_dist(rng) < config.write_ratio) ? 'W' : 'R';
        entry.address = config.start_addr + addr_dist(rng) * config.block_size;
        trace.push_back(entry);
    }

    return trace;
}

// Generate a locality-biased trace.
// Hotspots are randomly placed regions. `hotspot_ratio` of accesses hit hotspots;
// the remainder are uniformly random across the full range.
static std::vector<TraceEntry> generate_locality(const TraceGenConfig& config, std::mt19937_64& rng) {
    std::vector<TraceEntry> trace;
    trace.reserve(config.count);

    // Create hotspot base addresses
    std::vector<uint64_t> hotspot_bases;
    {
        uint64_t max_base = config.addr_range > config.hotspot_size
                            ? config.addr_range - config.hotspot_size
                            : 0;
        uint64_t num_positions = max_base / config.block_size;
        if (num_positions == 0) num_positions = 1;

        std::uniform_int_distribution<uint64_t> base_dist(0, num_positions);
        for (size_t h = 0; h < config.num_hotspots; h++) {
            hotspot_bases.push_back(config.start_addr + base_dist(rng) * config.block_size);
        }
    }

    // Distribution helpers
    uint64_t num_blocks_total = config.addr_range / config.block_size;
    if (num_blocks_total == 0) num_blocks_total = 1;
    uint64_t hotspot_blocks = config.hotspot_size / config.block_size;
    if (hotspot_blocks == 0) hotspot_blocks = 1;

    std::uniform_int_distribution<uint64_t> global_dist(0, num_blocks_total - 1);
    std::uniform_int_distribution<uint64_t> local_dist(0, hotspot_blocks - 1);
    std::uniform_int_distribution<size_t> hotspot_select(0, config.num_hotspots > 0 ? config.num_hotspots - 1 : 0);
    std::uniform_real_distribution<double> locality_coin(0.0, 1.0);
    std::uniform_real_distribution<double> write_dist(0.0, 1.0);

    for (size_t i = 0; i < config.count; i++) {
        TraceEntry entry;
        entry.type = (write_dist(rng) < config.write_ratio) ? 'W' : 'R';

        if (locality_coin(rng) < config.hotspot_ratio && !hotspot_bases.empty()) {
            // Access within a randomly selected hotspot
            size_t h = hotspot_select(rng);
            entry.address = hotspot_bases[h] + local_dist(rng) * config.block_size;
        } else {
            // Global random access
            entry.address = config.start_addr + global_dist(rng) * config.block_size;
        }

        trace.push_back(entry);
    }

    return trace;
}

std::vector<TraceEntry> generate_trace(const TraceGenConfig& config) {
    if (config.count == 0) {
        throw std::invalid_argument("TraceGenConfig: count must be > 0");
    }
    if (config.block_size == 0) {
        throw std::invalid_argument("TraceGenConfig: block_size must be > 0");
    }
    if (config.addr_range == 0) {
        throw std::invalid_argument("TraceGenConfig: addr_range must be > 0");
    }
    if (config.write_ratio < 0.0 || config.write_ratio > 1.0) {
        throw std::invalid_argument("TraceGenConfig: write_ratio must be in [0.0, 1.0]");
    }

    // Seed the RNG
    std::mt19937_64 rng;
    if (config.seed != 0) {
        rng.seed(config.seed);
    } else {
        std::random_device rd;
        rng.seed(rd());
    }

    switch (config.pattern) {
        case TracePattern::SEQUENTIAL:
            return generate_sequential(config, rng);
        case TracePattern::RANDOM:
            return generate_random(config, rng);
        case TracePattern::LOCALITY:
            return generate_locality(config, rng);
        default:
            throw std::invalid_argument("Unknown trace pattern");
    }
}

// ============================================================================
// Trace Writer
// ============================================================================

void write_trace_file(const std::string& filepath, const std::vector<TraceEntry>& trace) {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file for writing: " + filepath);
    }

    file << "# Trace file generated by cache-sim\n";
    file << "# Format: <R|W> <hex_address>\n";
    file << "# Entries: " << trace.size() << "\n\n";

    for (const auto& entry : trace) {
        file << entry.type << " 0x" << std::hex << entry.address << "\n";
    }
}
