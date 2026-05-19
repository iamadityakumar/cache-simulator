# cache-sim — CPU Cache Simulator

A high-performance, multi-level CPU cache simulator written in C++17. Simulates L1 → L2 → L3 → Main Memory hierarchies with configurable sizes, associativities, and replacement policies.

## Features

- **Multi-level cache hierarchy** — L1, L2, L3 with inclusive fill-on-miss
- **Replacement policies** — LRU (Least Recently Used) and FIFO (First In, First Out)
- **Set-associative caches** — Configurable associativity per level
- **Trace system** — Load `.trace` files or generate synthetic traces (sequential, random, locality)
- **Multiple output formats** — Pretty-printed table, JSON, CSV
- **Parameter sweep** — Explore cache size, associativity, and policy combinations ranked by AMAT
- **LRU vs FIFO comparison** — Side-by-side policy analysis with the same trace
- **Verbose mode** — Per-access hit/miss logging with latency breakdown
- **JSON config files** — Load simulation parameters from a config file
- **Portable** — No external dependencies; builds on Windows (MSVC), Linux (GCC/Clang), macOS

## Building

### Prerequisites

- C++17 compiler (MSVC 2019+, GCC 8+, Clang 7+)
- CMake 3.16+

### Build Steps

```bash
# Configure
cmake -B build -S .

# Build (Release)
cmake --build build --config Release

# The executables will be in build/Release/ (MSVC) or build/ (GCC/Clang)
```

### Run Tests

```bash
# Run the full test suite (144 tests)
./build/Release/cache-sim-tests      # Windows/MSVC
./build/cache-sim-tests              # Linux/macOS
```

## Usage

### Basic Simulation (Default Config)

```bash
# Generate a synthetic trace and simulate with default cache config
cache-sim --generate --gen-count 10000 --gen-pattern locality
```

Output:
```
  ============================================================
                    Cache Simulation Results
  ============================================================

  Configuration:
    Block Size:    64 B
    Mem Latency:   200 cycles

  Level  Size      Assoc   Policy        Hits    Misses  Hit Rate
  ---------------------------------------------------------------
  L1     32 KB     8-way   lru          5,623     4,377    56.23%
  L2     256 KB    8-way   lru             14     4,363     0.32%
  L3     8 MB      16-way  lru              0     4,363     0.00%
  ---------------------------------------------------------------

  Summary:
    Total Accesses:  10,000
    AMAT:            110.08 cycles

  ============================================================
```

### Using a Trace File

```bash
cache-sim --trace traces/sample.trace
cache-sim --trace traces/matrix_multiply.trace --output json
```

### Custom Cache Configuration

```bash
cache-sim --l1-size 64KB --l1-assoc 4 --l1-policy fifo \
          --l2-size 512KB --l2-assoc 8 --l2-policy lru \
          --l3-size 16MB --l3-assoc 16 --l3-policy lru \
          --block-size 64 --mem-latency 300 \
          --generate --gen-count 50000
```

### Parameter Sweep

Explore combinations of cache sizes, associativities, and policies:

```bash
cache-sim --sweep --generate --gen-count 5000 --gen-pattern locality
```

Output shows all configurations ranked by AMAT, with the optimal highlighted:
```
  >>> OPTIMAL CONFIGURATION <<<
    L1: 64 KB  |  L2: 512 KB  |  L3: 16 MB
    Associativity: 16-way  |  Policy: lru
    AMAT: 106.82 cycles
```

### LRU vs FIFO Comparison

```bash
cache-sim --compare --generate --gen-count 5000 --gen-pattern random
```

Output:
```
  ============================================================
               LRU vs FIFO — Side-by-Side Comparison
  ============================================================

  Metric                           LRU          FIFO          Diff
  ----------------------------------------------------------------
  L1 Hit Rate                    3.50%         3.50%        +0.00%
  AMAT (cycles)                218.32        218.32         +0.00
  ----------------------------------------------------------------

  >>> WINNER: LRU <<<
```

### JSON Config File

```bash
cache-sim --config my_config.json
```

Example `my_config.json`:
```json
{
  "l1_size": "64KB",
  "l1_assoc": 4,
  "l1_policy": "fifo",
  "l2_size": "512KB",
  "l2_assoc": 8,
  "l3_size": "16MB",
  "mem_latency": 300,
  "gen_pattern": "locality"
}
```

### CSV Output (Machine-Readable)

```bash
cache-sim --generate --output csv > results.csv
cache-sim --sweep --generate --output csv > sweep_results.csv
```

### Verbose Mode

```bash
cache-sim --generate --gen-count 10 --verbose
```

Output:
```
[       1] R 0x000000001040 -> MEMORY MISS  (251 cycles)
[       2] R 0x000000001080 -> MEMORY MISS  (251 cycles)
[       3] R 0x000000001040 -> L1 HIT  (1 cycles)
```

## CLI Reference

| Flag | Description | Default |
|------|-------------|---------|
| `--l1-size <SIZE>` | L1 cache size (e.g., `32KB`) | `32KB` |
| `--l1-assoc <N>` | L1 associativity | `8` |
| `--l1-policy <P>` | L1 replacement policy (`lru`/`fifo`) | `lru` |
| `--l1-latency <N>` | L1 hit latency (cycles) | `1` |
| `--l2-size <SIZE>` | L2 cache size | `256KB` |
| `--l2-assoc <N>` | L2 associativity | `8` |
| `--l2-policy <P>` | L2 replacement policy | `lru` |
| `--l2-latency <N>` | L2 hit latency (cycles) | `10` |
| `--l3-size <SIZE>` | L3 cache size | `8MB` |
| `--l3-assoc <N>` | L3 associativity | `16` |
| `--l3-policy <P>` | L3 replacement policy | `lru` |
| `--l3-latency <N>` | L3 hit latency (cycles) | `40` |
| `--block-size <N>` | Block size in bytes | `64` |
| `--mem-latency <N>` | Main memory latency (cycles) | `200` |
| `--trace <FILE>` | Path to `.trace` file | — |
| `--generate` | Generate synthetic trace | — |
| `--gen-count <N>` | Number of trace entries | `10000` |
| `--gen-pattern <P>` | Pattern: `sequential`/`random`/`locality` | `locality` |
| `--gen-seed <N>` | RNG seed (0 = random) | `0` |
| `--output <FMT>` | Output format: `table`/`json`/`csv` | `table` |
| `--verbose` | Show per-access details | `false` |
| `--config <FILE>` | Load JSON config file | — |
| `--sweep` | Run parameter sweep | `false` |
| `--compare` | Compare LRU vs FIFO | `false` |
| `-h`, `--help` | Show help message | — |

## Trace File Format

```
# Comments start with '#'
# Format: <R|W> <hex_address>
R 0x7fff5a3c1000
W 0x00400560
R 0x7fff5a3c1040
```

- `R` = read, `W` = write
- Addresses in hexadecimal (with or without `0x` prefix)
- Blank lines and comment lines are skipped

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                       main.cpp                          │
│         CLI → Config → Trace → Simulate → Report        │
├──────────┬──────────┬──────────┬──────────┬─────────────┤
│  cli.h   │ config.h │trace_    │ sweep.h  │  compare.h  │
│  cli.cpp │config.cpp│loader.h  │sweep.cpp │ compare.cpp │
│          │          │trace_    │          │             │
│          │          │loader.cpp│          │             │
├──────────┴──────────┴──────────┴──────────┴─────────────┤
│              memory_hierarchy.h/.cpp                     │
│           (L1 → L2 → L3 → Main Memory)                  │
├─────────────────┬───────────────────┬───────────────────┤
│  cache_level.h  │   cache_set.h     │    policies.h     │
│  cache_level.cpp│   cache_set.cpp   │    policies.cpp   │
│                 │                   │  (LRU / FIFO)     │
├─────────────────┴───────────────────┴───────────────────┤
│          stats.h/.cpp  |  report.h/.cpp                  │
│       (Statistics, AMAT)  (Table, JSON, CSV)             │
└─────────────────────────────────────────────────────────┘
```

## Project Structure

```
cache-sim/
├── CMakeLists.txt          # Build configuration
├── README.md               # This file
├── src/
│   ├── main.cpp            # Entry point
│   ├── cli.h/.cpp          # Command-line argument parser
│   ├── config.h/.cpp       # SimConfig, size parsing, JSON loader
│   ├── cache_line.h        # CacheLine struct
│   ├── cache_set.h/.cpp    # CacheSet (per-set tag management)
│   ├── cache_level.h/.cpp  # CacheLevel (address decomposition)
│   ├── policies.h/.cpp     # LRU + FIFO replacement policies
│   ├── stats.h/.cpp        # Statistics collector + AMAT
│   ├── memory_hierarchy.h/.cpp  # Multi-level cache orchestration
│   ├── trace_loader.h/.cpp # Trace parser + synthetic generator
│   ├── report.h/.cpp       # Output formatters (table/JSON/CSV)
│   ├── sweep.h/.cpp        # Parameter sweep engine
│   └── compare.h/.cpp      # LRU vs FIFO comparison
├── tests/
│   ├── test_framework.h    # Minimal test framework (no dependencies)
│   ├── test_main.cpp       # Test runner
│   ├── test_policies.cpp   # Replacement policy tests
│   ├── test_cache_level.cpp # Cache set + level tests
│   ├── test_hierarchy.cpp  # Memory hierarchy integration tests
│   ├── test_trace.cpp      # Trace parser + generator tests
│   ├── test_cli_report.cpp # CLI + report tests
│   ├── test_analysis.cpp   # Sweep + compare tests
│   └── test_edge_cases.cpp # Edge case tests
└── traces/
    ├── sample.trace        # Hand-crafted sample trace
    └── matrix_multiply.trace  # Matrix multiplication trace
```

## Test Suite

| Module | Tests | Description |
|--------|-------|-------------|
| Policies | 14 | LRU/FIFO insertion, eviction, removal |
| Cache Level | 15 | Address decomposition, fill, conflict misses |
| Hierarchy | 12 | Multi-level access, fill-on-miss, latency math |
| Trace | 26 | Parser, generator, patterns, validation |
| CLI & Report | 20 | Arg parsing, config files, output formats |
| Analysis | 10 | Sweep engine, compare mode |
| Edge Cases | 12 | Cold cache, single-entry, power-of-2, reset |
| **Total** | **109+** | |

## License

MIT
