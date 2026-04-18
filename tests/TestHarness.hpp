#pragma once

#include <cmath>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace TestHarness {

// Each test body is stored as a plain function pointer so the harness can
// register tests at static initialization time without needing a framework.
using TestFunc = void (*)();

struct TestCase {
    // Human-readable name printed in pass/fail output.
    std::string name;
    // Function that runs the test body.
    TestFunc func;
};

inline std::vector<TestCase>& Registry()
{
    // Keep one process-wide registry that every TEST_CASE appends into.
    static std::vector<TestCase> tests;
    return tests;
}

struct Registrar {
    Registrar(std::string name, TestFunc func)
    {
        // Constructing a registrar during static initialization is what makes
        // a TEST_CASE discoverable by the simple main() runner.
        Registry().push_back({std::move(name), func});
    }
};

inline void Require(bool condition, const char* expression, const char* file, int line)
{
    // Mirror the behavior of a unit test assertion: if the condition is false,
    // build a useful failure message and stop this test by throwing.
    if (!condition)
    {
        std::ostringstream message;
        message << file << ":" << line << " requirement failed: " << expression;
        throw std::runtime_error(message.str());
    }
}

inline void RequireNear(float lhs, float rhs, float epsilon, const char* lhs_expr, const char* rhs_expr,
    const char* file, int line)
{
    // Floating-point comparisons need tolerance because exact bitwise equality
    // is too strict for camera math and particle simulation values.
    if (std::fabs(lhs - rhs) > epsilon)
    {
        std::ostringstream message;
        message << file << ":" << line << " expected " << lhs_expr << " ~= " << rhs_expr
                << " but got " << lhs << " and " << rhs;
        throw std::runtime_error(message.str());
    }
}

}  // namespace TestHarness

// Declare the test function, register it automatically, then define it.
#define TEST_CASE(name) \
    static void name(); \
    static TestHarness::Registrar name##_registrar(#name, &name); \
    static void name()

// Boolean assertion helper used by most tests.
#define REQUIRE(expr) TestHarness::Require((expr), #expr, __FILE__, __LINE__)
// Approximate-equality helper used for float-heavy tests.
#define REQUIRE_NEAR(lhs, rhs, epsilon) \
    TestHarness::RequireNear((lhs), (rhs), (epsilon), #lhs, #rhs, __FILE__, __LINE__)
