// test_main.cpp — Test runner entry point
#include "test_framework.h"

int main() {
    std::cout << "Running " << get_tests().size() << " tests...\n\n";
    for (auto& t : get_tests()) {
        g_current_test = t.name;
        int fail_before = g_fail;
        t.func();
        if (g_fail == fail_before) {
            std::cout << "  PASS: " << t.name << "\n";
            g_pass++;
        }
    }
    std::cout << "\n" << g_pass << " passed, " << g_fail << " failed.\n";
    return g_fail > 0 ? 1 : 0;
}
