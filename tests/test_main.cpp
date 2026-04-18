#include "TestHarness.hpp"

#include <exception>
#include <iostream>

int main()
{
    // Track totals so the process can return a non-zero exit code on failure.
    int passed = 0;
    int failed = 0;

    // Run tests in registration order to keep output deterministic.
    for (const auto& test : TestHarness::Registry())
    {
        try
        {
            // Execute the test body. Any failed REQUIRE throws.
            test.func();
            std::cout << "[PASS] " << test.name << '\n';
            ++passed;
        }
        catch (const std::exception& error)
        {
            // Surface the assertion message for ordinary test failures.
            std::cerr << "[FAIL] " << test.name << ": " << error.what() << '\n';
            ++failed;
        }
        catch (...)
        {
            // Keep unknown failures visible rather than silently aborting.
            std::cerr << "[FAIL] " << test.name << ": unknown exception\n";
            ++failed;
        }
    }

    // Emit the same summary format our CTest integration expects to see.
    std::cout << passed << " passed, " << failed << " failed\n";
    // Return success only when every registered test passed.
    return failed == 0 ? 0 : 1;
}
