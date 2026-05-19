cmake_minimum_required(VERSION 3.16)
project(cache-sim LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Main executable sources
set(SOURCES
    src/main.cpp
    src/policies.cpp
    src/cache_set.cpp
    src/cache_level.cpp
    src/stats.cpp
    src/memory_hierarchy.cpp
    src/trace_loader.cpp
    src/config.cpp
    src/cli.cpp
    src/report.cpp
    src/sweep.cpp
    src/compare.cpp
)

add_executable(cache-sim ${SOURCES})
target_include_directories(cache-sim PRIVATE src)

# Test executable
set(TEST_SOURCES
    tests/test_main.cpp
    tests/test_policies.cpp
    tests/test_cache_level.cpp
    tests/test_hierarchy.cpp
    tests/test_trace.cpp
    tests/test_cli_report.cpp
    tests/test_analysis.cpp
    tests/test_edge_cases.cpp
    src/policies.cpp
    src/cache_set.cpp
    src/cache_level.cpp
    src/stats.cpp
    src/memory_hierarchy.cpp
    src/trace_loader.cpp
    src/config.cpp
    src/cli.cpp
    src/report.cpp
    src/sweep.cpp
    src/compare.cpp
)

add_executable(cache-sim-tests ${TEST_SOURCES})
target_include_directories(cache-sim-tests PRIVATE src)

# Compiler warnings
if(MSVC)
    target_compile_options(cache-sim PRIVATE /W4)
    target_compile_options(cache-sim-tests PRIVATE /W4)
else()
    target_compile_options(cache-sim PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(cache-sim-tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
