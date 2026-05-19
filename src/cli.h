#pragma once

#include "config.h"
#include <string>

// Parse command-line arguments into a SimConfig.
// Handles --key value and --flag style arguments.
// Throws std::invalid_argument on unrecognized flags or missing values.
void parse_args(int argc, char* argv[], SimConfig& config);

// Print usage/help message to stdout.
void print_help();
