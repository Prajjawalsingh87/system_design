// mini_test.hpp: a tiny header-only test framework.
//
// No dependencies, no network fetches at build time. A test file looks like:
//
//     #include "../testing/mini_test.hpp"
//
//     TEST(addition_works) {
//         CHECK(1 + 1 == 2);
//         CHECK_EQ(2 + 2, 4);
//         CHECK_THROWS(throw std::runtime_error("x"), std::runtime_error);
//     }
//
//     MINI_TEST_MAIN()
//
// The binary exits with 0 when every test passes and 1 otherwise, so CI can
// simply run it. CHECK* macros record a failure and keep going; REQUIRE* macros
// stop the current test (use them when later lines would crash otherwise).
#pragma once

#include <cstdio>
#include <exception>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace mini_test {

struct TestCase {
    const char* name;
    void (*fn)();
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}

struct Registrar {
    Registrar(const char* name, void (*fn)()) { registry().push_back({name, fn}); }
};

// Number of failed checks in the currently running test.
inline int& current_failures() {
    static int failures = 0;
    return failures;
}

// Thrown by REQUIRE* to abort the current test.
struct AbortTest {};

// Prints a value if it supports operator<<, otherwise prints a placeholder.
template <typename T, typename = void>
struct is_printable : std::false_type {};
template <typename T>
struct is_printable<T, std::void_t<decltype(std::declval<std::ostream&>() << std::declval<const T&>())>>
    : std::true_type {};

template <typename T>
std::string to_string(const T& value) {
    if constexpr (std::is_same_v<T, bool>) {
        return value ? "true" : "false";
    } else if constexpr (is_printable<T>::value) {
        std::ostringstream out;
        out << value;
        return out.str();
    } else {
        return "<unprintable>";
    }
}

inline void report_failure(const char* file, int line, const std::string& message) {
    ++current_failures();
    std::printf("    %s:%d: FAILED: %s\n", file, line, message.c_str());
}

template <typename A, typename B>
bool check_eq(const A& actual, const B& expected, const char* actual_expr, const char* expected_expr,
              const char* file, int line) {
    if (actual == expected) return true;
    report_failure(file, line,
                   std::string(actual_expr) + " == " + expected_expr + "  (got " + to_string(actual) +
                       ", expected " + to_string(expected) + ")");
    return false;
}

inline int run_all() {
    int failed_tests = 0;
    for (const TestCase& test : registry()) {
        current_failures() = 0;
        std::printf("[ RUN  ] %s\n", test.name);
        std::fflush(stdout);  // keep the test name in CI logs even if the test crashes
        try {
            test.fn();
        } catch (const AbortTest&) {
            // Failure already reported by REQUIRE.
        } catch (const std::exception& e) {
            report_failure(__FILE__, __LINE__, std::string("unexpected exception: ") + e.what());
        } catch (...) {
            report_failure(__FILE__, __LINE__, "unexpected non-standard exception");
        }
        if (current_failures() == 0) {
            std::printf("[  OK  ] %s\n", test.name);
        } else {
            std::printf("[ FAIL ] %s (%d failed check(s))\n", test.name, current_failures());
            ++failed_tests;
        }
        std::fflush(stdout);
    }
    const int total = static_cast<int>(registry().size());
    std::printf("\n%d/%d tests passed\n", total - failed_tests, total);
    return failed_tests == 0 ? 0 : 1;
}

}  // namespace mini_test

#define MINI_TEST_CONCAT_INNER(a, b) a##b
#define MINI_TEST_CONCAT(a, b) MINI_TEST_CONCAT_INNER(a, b)

#define TEST(name)                                                                        \
    static void name();                                                                   \
    static const ::mini_test::Registrar MINI_TEST_CONCAT(name, _registrar)(#name, &name); \
    static void name()

#define CHECK(cond)                                                                \
    do {                                                                           \
        if (!(cond)) ::mini_test::report_failure(__FILE__, __LINE__, "CHECK(" #cond ")"); \
    } while (0)

#define REQUIRE(cond)                                                                       \
    do {                                                                                    \
        if (!(cond)) {                                                                      \
            ::mini_test::report_failure(__FILE__, __LINE__, "REQUIRE(" #cond ")");          \
            throw ::mini_test::AbortTest{};                                                 \
        }                                                                                   \
    } while (0)

#define CHECK_EQ(actual, expected) \
    (void)::mini_test::check_eq((actual), (expected), #actual, #expected, __FILE__, __LINE__)

#define REQUIRE_EQ(actual, expected)                                                            \
    do {                                                                                        \
        if (!::mini_test::check_eq((actual), (expected), #actual, #expected, __FILE__, __LINE__)) \
            throw ::mini_test::AbortTest{};                                                     \
    } while (0)

#define CHECK_THROWS(expr, exception_type)                                                         \
    do {                                                                                           \
        bool mini_test_caught = false;                                                             \
        try {                                                                                      \
            (void)(expr);                                                                          \
        } catch (const exception_type&) {                                                          \
            mini_test_caught = true;                                                               \
        } catch (...) {                                                                            \
        }                                                                                          \
        if (!mini_test_caught)                                                                     \
            ::mini_test::report_failure(__FILE__, __LINE__,                                        \
                                        "CHECK_THROWS(" #expr ", " #exception_type ") did not throw"); \
    } while (0)

#define MINI_TEST_MAIN() \
    int main() { return ::mini_test::run_all(); }
