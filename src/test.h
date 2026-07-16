#pragma once

#include "log.h"

#include <cassert>
#include <format>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string_view>
#include <vector>

namespace test {

#ifdef TEST

struct TestCase {
    std::string_view name;
    void (*f)();
};

class Registry {
public:
    Registry() {}

    void register_test_case(std::string_view name, void (*f)()) { test_cases.emplace_back(name, f); }

    int run_tests(std::ofstream& gha_summary) {
        std::cerr << std::format("running {} test(s)\n", this->test_cases.size()) << std::endl;
        int passed = 0, failed = 0;
        std::vector<std::pair<std::string_view, bool>> results;
        results.reserve(this->test_cases.size());
        for (TestCase& test_case : this->test_cases) {
            std::cerr << test_case.name << " ... ";
            // capture stdout and stderr
            std::ostringstream captured_stdout, captured_stderr;
            std::streambuf* saved_stdout = std::cout.rdbuf(captured_stdout.rdbuf());
            std::streambuf* saved_stderr = std::cerr.rdbuf(captured_stderr.rdbuf());
            // run test
            bool ok = run_test(test_case);
            // restore stdout and stderr
            std::cout.rdbuf(saved_stdout);
            std::cerr.rdbuf(saved_stderr);

            if (ok) {
                ++passed;
                results.push_back(std::make_pair(test_case.name, true));
                std::cerr << (isatty(STDERR_FILENO) ? (COLOR_GREEN "ok" COLOR_RESET) : "ok") << std::endl;
            } else {
                ++failed;
                results.push_back(std::make_pair(test_case.name, false));
                std::cerr << (isatty(STDERR_FILENO) ? (COLOR_RED "FAILED" COLOR_RESET) : "FAILED") << std::endl;
                std::cerr << "---- captured stdout ----" << std::endl;
                std::cerr << captured_stdout.str() << std::endl;
                std::cerr << "---- captured stderr ----" << std::endl;
                std::cerr << captured_stderr.str() << std::endl;
            }
        }
        std::cerr << std::endl;
        std::cerr << std::format("{} passed, {} failed", passed, failed) << std::endl;
        gha_summary << "## Test Summary" << std::endl << std::endl;
        gha_summary << "| Status | Test Name |" << std::endl;
        gha_summary << "| :---: | :--- |" << std::endl;
        for (const auto& result : results) {
            gha_summary << "| " << (result.second ? "✅" : "❌") << " | " << result.first << " |" << std::endl;
        }
        if (failed > 0) {
            return 6;
        }
        return 0;
    }

private:
    std::vector<TestCase> test_cases;

    bool run_test(TestCase& test_case) {
        try {
            test_case.f();
        } catch (const std::exception& e) {
            std::cerr << std::format("exception of type {} thrown: , what(): {}", typeid(e).name(), e.what())
                      << std::endl;
            return false;
        } catch (...) {
            std::cerr << "test case threw something unknown, aborting" << std::endl;
            throw;
        }
        return true;
    }
};

inline Registry& global_registry() {
    static Registry instance;
    return instance;
}

inline int register_test_case(std::string_view name, void (*f)()) {
    global_registry().register_test_case(name, f);
    return 0;
}

inline int run_tests(std::ofstream& gha_summary) {
    return global_registry().run_tests(gha_summary);
}

class AssertionFailed : public std::exception {

public:
    AssertionFailed(std::string_view file, int line, std::string msg) : file(file), line(line), msg(msg) {}
    const char* what() const noexcept override {
        if (_what.empty()) {
            _what = std::format("{}:{}: assertion failed: {}", file, line, msg);
        }
        return _what.c_str();
    }

private:
    std::string_view file;
    int line;
    std::string msg;
    mutable std::string _what;
};

#define TEST_CASE(name) TEST_CASE_WITH_ID(name, __COUNTER__)

#define TEST_CASE_WITH_ID(name, id) TEST_CASE_IMPL(name, id)

#define TEST_CASE_IMPL(name, id)                                                                                       \
    static void test_case_##id();                                                                                      \
    static const int register_test_##id = test::register_test_case((name), test_case_##id);                            \
    static void test_case_##id()

#define ASSERT_EQ(a, b)                                                                                                \
    do {                                                                                                               \
        const auto& _a = (a);                                                                                          \
        const auto& _b = (b);                                                                                          \
        if (!((_a) == (_b))) {                                                                                         \
            throw test::AssertionFailed(__FILE__, __LINE__, std::format("{} == {} ({} != {})", #a, #b, _a, _b));       \
        }                                                                                                              \
    } while (0)

#define ASSERT_NEQ(a, b)                                                                                               \
    do {                                                                                                               \
        const auto& _a = (a);                                                                                          \
        const auto& _b = (b);                                                                                          \
        if ((_a) == (_b)) {                                                                                            \
            throw test::AssertionFailed(__FILE__, __LINE__, std::format("{} != {} ({} == {})", #a, #b, _a, _b));       \
        }                                                                                                              \
    } while (0)

#define ASSERT_TRUE(b)                                                                                                 \
    do {                                                                                                               \
        if (!(b)) {                                                                                                    \
            throw test::AssertionFailed(__FILE__, __LINE__, std::format("{} is not true", #b));                        \
        }                                                                                                              \
    } while (0)

#else
#define TEST_CASE(name) TEST_CASE_WITH_ID(name, __COUNTER__)
#define TEST_CASE_WITH_ID(name, id) TEST_CASE_IMPL(name, id)
#define TEST_CASE_IMPL(name, id) [[maybe_unused]] static void test_case_##id()
#define ASSERT_EQ(a, b) (void)66
#define ASSERT_NEQ(a, b) (void)66
#define ASSERT_TRUE(b) (void)66
#endif

} // namespace test
