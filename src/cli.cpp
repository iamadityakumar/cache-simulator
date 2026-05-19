#include "cli.h"
#include <iostream>
#include <stdexcept>
#include <string>

// Helper: get the next argument value, or throw if missing.
static std::string next_arg(int argc, char* argv[], int& i, const std::string& flag) {
    if (i + 1 >= argc) {
        throw std::invalid_argument("Missing value for " + flag);
    }
    return argv[++i];
}

void parse_args(int argc, char* argv[], SimConfig& config) {
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        // --- Help ---
        if (arg == "-h" || arg == "--help") {
            config.show_help = true;
            return;
        }

        // --- L1 ---
        else if (arg == "--l1-size")   config.l1_size   = parse_size(next_arg(argc, argv, i, arg));
        else if (arg == "--l1-assoc")  config.l1_assoc  = std::stoull(next_arg(argc, argv, i, arg));
        else if (arg == "--l1-policy") config.l1_policy  = next_arg(argc, argv, i, arg);
        else if (arg == "--l1-latency") config.l1_latency = std::stoull(next_arg(argc, argv, i, arg));

        // --- L2 ---
        else if (arg == "--l2-size")   config.l2_size   = parse_size(next_arg(argc, argv, i, arg));
        else if (arg == "--l2-assoc")  config.l2_assoc  = std::stoull(next_arg(argc, argv, i, arg));
        else if (arg == "--l2-policy") config.l2_policy  = next_arg(argc, argv, i, arg);
        else if (arg == "--l2-latency") config.l2_latency = std::stoull(next_arg(argc, argv, i, arg));

        // --- L3 ---
        else if (arg == "--l3-size")   config.l3_size   = parse_size(next_arg(argc, argv, i, arg));
        else if (arg == "--l3-assoc")  config.l3_assoc  = std::stoull(next_arg(argc, argv, i, arg));
        else if (arg == "--l3-policy") config.l3_policy  = next_arg(argc, argv, i, arg);
        else if (arg == "--l3-latency") config.l3_latency = std::stoull(next_arg(argc, argv, i, arg));

        // --- Common ---
        else if (arg == "--block-size")  config.block_size  = parse_size(next_arg(argc, argv, i, arg));
        else if (arg == "--mem-latency") config.mem_latency = std::stoull(next_arg(argc, argv, i, arg));

        // --- Trace ---
        else if (arg == "--trace")       config.trace_file  = next_arg(argc, argv, i, arg);
        else if (arg == "--generate")    config.generate    = true;
        else if (arg == "--gen-count")   config.gen_count   = std::stoull(next_arg(argc, argv, i, arg));
        else if (arg == "--gen-pattern") config.gen_pattern = next_arg(argc, argv, i, arg);
        else if (arg == "--gen-seed")    config.gen_seed    = std::stoull(next_arg(argc, argv, i, arg));

        // --- Output ---
        else if (arg == "--output")  config.output_format = next_arg(argc, argv, i, arg);
        else if (arg == "--verbose") config.verbose       = true;

        // --- Config file ---
        else if (arg == "--config") config.config_file = next_arg(argc, argv, i, arg);

        // --- Analysis modes (Phase 5 stubs) ---
        else if (arg == "--sweep")   config.sweep   = true;
        else if (arg == "--compare") config.compare = true;

        // --- Unknown ---
        else {
            throw std::invalid_argument("Unknown argument: " + arg);
        }
    }
}

void print_help() {
    std::cout <<
R"(cache-sim — CPU Cache Simulator

USAGE:
  cache-sim [OPTIONS]

CACHE CONFIGURATION:
  --l1-size <SIZE>       L1 cache size (e.g., 32KB)         [default: 32KB]
  --l1-assoc <N>         L1 associativity                    [default: 8]
  --l1-policy <POLICY>   L1 replacement policy (lru/fifo)    [default: lru]
  --l1-latency <N>       L1 hit latency (cycles)             [default: 1]
  --l2-size <SIZE>       L2 cache size                       [default: 256KB]
  --l2-assoc <N>         L2 associativity                    [default: 8]
  --l2-policy <POLICY>   L2 replacement policy               [default: lru]
  --l2-latency <N>       L2 hit latency (cycles)             [default: 10]
  --l3-size <SIZE>       L3 cache size                       [default: 8MB]
  --l3-assoc <N>         L3 associativity                    [default: 16]
  --l3-policy <POLICY>   L3 replacement policy               [default: lru]
  --l3-latency <N>       L3 hit latency (cycles)             [default: 40]
  --block-size <N>       Block size in bytes                  [default: 64]

MEMORY:
  --mem-latency <N>      Main memory latency (cycles)        [default: 200]

TRACE:
  --trace <FILE>         Path to trace file (.trace format)
  --generate             Generate synthetic trace instead
  --gen-count <N>        Number of addresses to generate     [default: 10000]
  --gen-pattern <TYPE>   Pattern: sequential/random/locality  [default: locality]
  --gen-seed <N>         RNG seed (0 = random)               [default: 0]

OUTPUT:
  --output <FORMAT>      Output format: table/json/csv       [default: table]
  --verbose              Show per-access details

CONFIG:
  --config <FILE>        Load configuration from JSON file

ANALYSIS:
  --sweep                Sweep over cache sizes, assoc, policies; rank by AMAT
  --compare              Run same config with LRU vs FIFO; side-by-side report

  -h, --help             Show this help message

EXAMPLES:
  cache-sim --generate --gen-pattern random --gen-count 50000
  cache-sim --trace traces/sample.trace --output json
  cache-sim --l1-size 64KB --l1-assoc 4 --l1-policy fifo --generate
  cache-sim --config my_config.json
)";
}
