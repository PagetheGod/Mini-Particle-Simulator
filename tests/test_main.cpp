#include "TestHarness.hpp"

#include <exception>
#include <iostream>

int main()
{
    int passed = 0;
    int failed = 0;

    for (const auto& test : TestHarness::Registry())
    {
        try
        {
            test.func();
            std::cout << "[PASS] " << test.name << '\n';
            ++passed;
        }
        catch (const std::exception& error)
        {
            std::cerr << "[FAIL] " << test.name << ": " << error.what() << '\n';
            ++failed;
        }
        catch (...)
        {
            std::cerr << "[FAIL] " << test.name << ": unknown exception\n";
            ++failed;
        }
    }

    std::cout << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
