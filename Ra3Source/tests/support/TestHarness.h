#pragma once

#include <sstream>
#include <string>
#include <vector>

namespace ra3::test {

using TestFunction = void (*)();

struct TestCase {
    const char* name;
    TestFunction function;
};

std::vector<TestCase>& registry();
void reportFailure(const char* file, int line, const std::string& message);

struct Registrar {
    Registrar(const char* name, TestFunction function) { registry().push_back({name, function}); }
};

struct AbortTest {};

struct SkipTest {
    std::string reason;
};

template <typename A, typename B>
void checkEqual(const A& a, const B& b, const char* exprA, const char* exprB, const char* file, int line)
{
    if (!(a == b)) {
        std::ostringstream out;
        out << "CHECK_EQ(" << exprA << ", " << exprB << "): " << a << " != " << b;
        reportFailure(file, line, out.str());
    }
}

}  // namespace ra3::test

#define RA3_TEST(name)                                                   \
    static void name();                                                  \
    static const ::ra3::test::Registrar name##Registrar{#name, &name};   \
    static void name()

#define RA3_CHECK(expr)                                                              \
    do {                                                                             \
        if (!(expr)) ::ra3::test::reportFailure(__FILE__, __LINE__, "CHECK(" #expr ")"); \
    } while (false)

#define RA3_REQUIRE(expr)                                                              \
    do {                                                                               \
        if (!(expr)) {                                                                 \
            ::ra3::test::reportFailure(__FILE__, __LINE__, "REQUIRE(" #expr ")");      \
            throw ::ra3::test::AbortTest{};                                            \
        }                                                                              \
    } while (false)

#define RA3_CHECK_EQ(a, b) ::ra3::test::checkEqual((a), (b), #a, #b, __FILE__, __LINE__)

// Requires a ra3::Result to hold a value; reports its error message otherwise.
#define RA3_REQUIRE_OK(result)                                                                         \
    do {                                                                                               \
        if (!(result).ok()) {                                                                          \
            ::ra3::test::reportFailure(__FILE__, __LINE__,                                             \
                                       std::string("REQUIRE_OK(" #result "): ") + (result).error().message); \
            throw ::ra3::test::AbortTest{};                                                            \
        }                                                                                              \
    } while (false)

#define RA3_SKIP(reason) throw ::ra3::test::SkipTest{reason}
