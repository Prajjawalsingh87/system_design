# testing/

[mini_test.hpp](mini_test.hpp) is a tiny header-only test framework, so the tests need no downloads at build time. It provides `TEST(name)`, `CHECK`, `CHECK_EQ`, `CHECK_THROWS`, `REQUIRE`, `REQUIRE_EQ` and `MINI_TEST_MAIN()`. A test binary exits with 1 if any check fails.

Any file named `*_test.cpp` is compiled **and run** by CI.
