#pragma once

#include <cmath>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace TestHarness {

using TestFunc = void (*)();

struct TestCase {
    std::string name;
    TestFunc func;
};

inline std::vector<TestCase>& Registry()
{
    static std::vector<TestCase> tests;
    return tests;
}

struct Registrar {
    Registrar(std::string name, TestFunc func)
    {
        Registry().push_back({std::move(name), func});
    }
};

inline void Require(bool condition, const char* expression, const char* file, int line)
{
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
    if (std::fabs(lhs - rhs) > epsilon)
    {
        std::ostringstream message;
        message << file << ":" << line << " expected " << lhs_expr << " ~= " << rhs_expr
                << " but got " << lhs << " and " << rhs;
        throw std::runtime_error(message.str());
    }
}

}  // namespace TestHarness

#define TEST_CASE(name) \
    static void name(); \
    static TestHarness::Registrar name##_registrar(#name, &name); \
    static void name()

#define REQUIRE(expr) TestHarness::Require((expr), #expr, __FILE__, __LINE__)
#define REQUIRE_NEAR(lhs, rhs, epsilon) \
    TestHarness::RequireNear((lhs), (rhs), (epsilon), #lhs, #rhs, __FILE__, __LINE__)
