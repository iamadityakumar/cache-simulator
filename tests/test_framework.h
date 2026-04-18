#pragma once
// Minimal test framework — no external dependencies

#include <iostream>
#include <vector>
#include <string>
#include <functional>

struct TestCase {
    std::string name;
    std::function<void()> func;
};

inline std::vector<TestCase>& get_tests() {
    static std::vector<TestCase> tests;
    return tests;
}

inline int g_pass = 0;
inline int g_fail = 0;
inline std::string g_current_test;

#define TEST(tname) \
    static void test_##tname(); \
    static bool reg_##tname = (get_tests().push_back({#tname, test_##tname}), true); \
    static void test_##tname()

#define ASSERT_TRUE(expr) \
    do { if (!(expr)) { \
        std::cerr << "  FAIL: " << g_current_test << " line " << __LINE__ \
                  << ": ASSERT_TRUE(" << #expr << ")\n"; \
        g_fail++; return; \
    }} while(0)

#define ASSERT_FALSE(expr) \
    do { if ((expr)) { \
        std::cerr << "  FAIL: " << g_current_test << " line " << __LINE__ \
                  << ": ASSERT_FALSE(" << #expr << ")\n"; \
        g_fail++; return; \
    }} while(0)

#define ASSERT_EQ(a, b) \
    do { auto _a = (a); auto _b = (b); if (_a != _b) { \
        std::cerr << "  FAIL: " << g_current_test << " line " << __LINE__ \
                  << ": ASSERT_EQ(" << #a << ", " << #b << ") got " << _a << " vs " << _b << "\n"; \
        g_fail++; return; \
    }} while(0)
