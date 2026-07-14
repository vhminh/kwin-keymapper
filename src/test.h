#pragma once

#include <cassert>
#include <string_view>
#include <vector>

#ifdef TEST

struct TestCase {
    std::string_view name;
    void (*f)();
};

class Registry {
public:
    Registry() {}

    void register_test_case(std::string_view name, void (*f)()) { test_cases.emplace_back(name, f); }

    void run() {
        for (TestCase& test_case : this->test_cases) {
            test_case.f();
        }
    }

private:
    std::vector<TestCase> test_cases;
};

inline Registry& global_registry() {
    static Registry instance;
    return instance;
}

inline int register_test_case(std::string_view name, void (*f)()) {
    global_registry().register_test_case(name, f);
    return 0;
}

inline void run_tests() {
    global_registry().run();
}

#define TEST_CASE(name) TEST_CASE_WITH_ID(name, __COUNTER__)

#define TEST_CASE_WITH_ID(name, id) TEST_CASE_IMPL(name, id)

#define TEST_CASE_IMPL(name, id)                                                                                       \
    static void test_case_##id();                                                                                      \
    static const int register_test_##id = register_test_case((name), test_case_##id);                                  \
    static void test_case_##id()

#else
#define TEST_CASE(name) TEST_CASE_WITH_ID(name, __COUNTER__)
#define TEST_CASE_WITH_ID(name, id) TEST_CASE_IMPL(name, id)
#define TEST_CASE_IMPL(name, id) [[maybe_unused]] static void test_case_##id()
#endif
