#include "config.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

// ============================================================================
// Size Parsing
// ============================================================================

size_t parse_size(const std::string& size_str) {
    if (size_str.empty()) {
        throw std::invalid_argument("parse_size: empty string");
    }

    // Find where the numeric part ends
    size_t num_end = 0;
    while (num_end < size_str.size() &&
           (std::isdigit(static_cast<unsigned char>(size_str[num_end])) || size_str[num_end] == '.')) {
        num_end++;
    }

    if (num_end == 0) {
        throw std::invalid_argument("parse_size: no numeric value in '" + size_str + "'");
    }

    double value = std::stod(size_str.substr(0, num_end));
    if (value < 0) {
        throw std::invalid_argument("parse_size: negative value in '" + size_str + "'");
    }

    // Extract and normalize suffix
    std::string suffix = size_str.substr(num_end);
    // Trim whitespace from suffix
    size_t start = suffix.find_first_not_of(" \t");
    if (start != std::string::npos) {
        suffix = suffix.substr(start);
    } else {
        suffix = "";
    }
    // Convert to uppercase
    std::transform(suffix.begin(), suffix.end(), suffix.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });

    size_t multiplier = 1;
    if (suffix.empty() || suffix == "B") {
        multiplier = 1;
    } else if (suffix == "KB" || suffix == "K") {
        multiplier = 1024;
    } else if (suffix == "MB" || suffix == "M") {
        multiplier = 1024 * 1024;
    } else if (suffix == "GB" || suffix == "G") {
        multiplier = 1024ULL * 1024 * 1024;
    } else {
        throw std::invalid_argument("parse_size: unknown suffix '" + suffix + "' in '" + size_str + "'");
    }

    return static_cast<size_t>(value * multiplier);
}

std::string format_size(size_t bytes) {
    if (bytes >= 1024ULL * 1024 * 1024) {
        double gb = static_cast<double>(bytes) / (1024.0 * 1024 * 1024);
        std::ostringstream oss;
        if (gb == static_cast<size_t>(gb))
            oss << static_cast<size_t>(gb) << " GB";
        else
            oss << gb << " GB";
        return oss.str();
    } else if (bytes >= 1024 * 1024) {
        double mb = static_cast<double>(bytes) / (1024.0 * 1024);
        std::ostringstream oss;
        if (mb == static_cast<size_t>(mb))
            oss << static_cast<size_t>(mb) << " MB";
        else
            oss << mb << " MB";
        return oss.str();
    } else if (bytes >= 1024) {
        double kb = static_cast<double>(bytes) / 1024.0;
        std::ostringstream oss;
        if (kb == static_cast<size_t>(kb))
            oss << static_cast<size_t>(kb) << " KB";
        else
            oss << kb << " KB";
        return oss.str();
    } else {
        return std::to_string(bytes) + " B";
    }
}

// ============================================================================
// JSON Config File Loader (minimal flat-object parser)
// ============================================================================

// Trim whitespace from both ends.
static std::string cfg_trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}



void load_config_file(const std::string& filepath, SimConfig& config) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open config file: " + filepath);
    }

    // Read entire file
    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());

    // Simple key-value extraction from JSON-like format.
    // We look for "key": value patterns.
    // This handles the flat config object we need without a full JSON parser.
    size_t pos = 0;
    while (pos < content.size()) {
        // Find next quoted key
        size_t key_start = content.find('"', pos);
        if (key_start == std::string::npos) break;
        size_t key_end = content.find('"', key_start + 1);
        if (key_end == std::string::npos) break;

        std::string key = content.substr(key_start + 1, key_end - key_start - 1);

        // Find colon
        size_t colon = content.find(':', key_end + 1);
        if (colon == std::string::npos) break;

        // Extract value (until comma, closing brace, or newline)
        size_t val_start = colon + 1;
        // Skip whitespace
        while (val_start < content.size() && std::isspace(static_cast<unsigned char>(content[val_start])))
            val_start++;

        std::string value;
        if (val_start < content.size() && content[val_start] == '"') {
            // String value
            size_t val_end = content.find('"', val_start + 1);
            if (val_end == std::string::npos) break;
            value = content.substr(val_start + 1, val_end - val_start - 1);
            pos = val_end + 1;
        } else {
            // Number or boolean value
            size_t val_end = content.find_first_of(",}\n\r", val_start);
            if (val_end == std::string::npos) val_end = content.size();
            value = cfg_trim(content.substr(val_start, val_end - val_start));
            pos = val_end + 1;
        }

        // Apply to config
        if (key == "l1_size")          config.l1_size     = parse_size(value);
        else if (key == "l1_assoc")    config.l1_assoc    = std::stoull(value);
        else if (key == "l1_policy")   config.l1_policy   = value;
        else if (key == "l1_latency")  config.l1_latency  = std::stoull(value);
        else if (key == "l2_size")     config.l2_size     = parse_size(value);
        else if (key == "l2_assoc")    config.l2_assoc    = std::stoull(value);
        else if (key == "l2_policy")   config.l2_policy   = value;
        else if (key == "l2_latency")  config.l2_latency  = std::stoull(value);
        else if (key == "l3_size")     config.l3_size     = parse_size(value);
        else if (key == "l3_assoc")    config.l3_assoc    = std::stoull(value);
        else if (key == "l3_policy")   config.l3_policy   = value;
        else if (key == "l3_latency")  config.l3_latency  = std::stoull(value);
        else if (key == "block_size")  config.block_size  = parse_size(value);
        else if (key == "mem_latency") config.mem_latency = std::stoull(value);
        else if (key == "trace_file")  config.trace_file  = value;
        else if (key == "gen_count")   config.gen_count   = std::stoull(value);
        else if (key == "gen_pattern") config.gen_pattern = value;
        else if (key == "output")      config.output_format = value;
        else if (key == "verbose")     config.verbose     = (value == "true" || value == "1");
        // Unknown keys are silently ignored for forward compatibility
    }
}
