#include <iostream>
#include <vector>

#include "cli.h"
#include "compare.h"
#include "config.h"
#include "memory_hierarchy.h"
#include "report.h"
#include "sweep.h"
#include "trace_loader.h"

int main(int argc, char* argv[]) {
    SimConfig config;

    // --- Parse CLI arguments ---
    try {
        parse_args(argc, argv, config);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        std::cerr << "Run with --help for usage information.\n";
        return 1;
    }

    // --- Show help ---
    if (config.show_help) {
        print_help();
        return 0;
    }

    // --- Load config file (if specified) ---
    if (!config.config_file.empty()) {
        try {
            load_config_file(config.config_file, config);
        } catch (const std::exception& e) {
            std::cerr << "Error loading config file: " << e.what() << "\n";
            return 1;
        }
    }

    // --- Obtain trace ---
    std::vector<TraceEntry> trace;
    try {
        if (!config.trace_file.empty()) {
            // Load from file
            trace = load_trace_file(config.trace_file);
        } else {
            // Generate synthetic trace (default, or --generate flag)
            TraceGenConfig gen_cfg;
            gen_cfg.pattern   = parse_pattern(config.gen_pattern);
            gen_cfg.count     = config.gen_count;
            gen_cfg.seed      = config.gen_seed;
            trace = generate_trace(gen_cfg);

            if (config.verbose) {
                std::cout << "Generated " << trace.size() << " "
                          << config.gen_pattern << " trace entries.\n\n";
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error loading/generating trace: " << e.what() << "\n";
        return 1;
    }

    // --- Analysis modes (Phase 5) ---
    if (config.sweep) {
        SweepConfig sweep_cfg = default_sweep_config();
        auto results = run_sweep(sweep_cfg, config, trace);

        if (config.output_format == "csv") {
            report_sweep_csv(results);
        } else {
            report_sweep_table(results);
        }
        return 0;
    }

    if (config.compare) {
        auto result = run_compare(config, trace);
        report_compare(result);
        return 0;
    }

    // --- Build memory hierarchy ---
    std::vector<CacheLevelConfig> levels = {
        {"L1", config.l1_size, config.block_size, config.l1_assoc,
         config.l1_latency, config.l1_policy},
        {"L2", config.l2_size, config.block_size, config.l2_assoc,
         config.l2_latency, config.l2_policy},
        {"L3", config.l3_size, config.block_size, config.l3_assoc,
         config.l3_latency, config.l3_policy}
    };

    MemoryHierarchy hierarchy(levels, config.mem_latency);

    // --- Run simulation ---
    for (size_t i = 0; i < trace.size(); i++) {
        auto result = hierarchy.access(trace[i].address, trace[i].type);

        if (config.verbose) {
            print_verbose_access(i + 1, trace[i].type, trace[i].address,
                                 result, hierarchy.num_levels());
        }
    }

    // --- Output report ---
    if (config.output_format == "json") {
        report_json(hierarchy, config);
    } else if (config.output_format == "csv") {
        report_csv(hierarchy, config);
    } else {
        report_table(hierarchy, config);
    }

    return 0;
}
