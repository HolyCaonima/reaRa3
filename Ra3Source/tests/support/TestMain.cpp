#include "support/TestHarness.h"

#include <exception>
#include <iostream>

namespace ra3::test {
namespace {

int g_failures = 0;

}  // namespace

std::vector<TestCase>& registry()
{
    static std::vector<TestCase> cases;
    return cases;
}

void reportFailure(const char* file, int line, const std::string& message)
{
    ++g_failures;
    std::cerr << file << "(" << line << "): " << message << "\n";
}

}  // namespace ra3::test

// Exit codes: 0 all passed, 1 some failed, 77 everything was skipped (CTest SKIP_RETURN_CODE).
int main()
{
    using namespace ra3::test;
    int passed = 0;
    int failed = 0;
    int skipped = 0;
    for (const TestCase& test : registry()) {
        const int failuresBefore = g_failures;
        try {
            test.function();
        } catch (const AbortTest&) {
        } catch (const SkipTest& skip) {
            ++skipped;
            std::cout << "[ SKIP ] " << test.name << ": " << skip.reason << "\n";
            continue;
        } catch (const std::exception& e) {
            reportFailure(__FILE__, __LINE__, std::string("unexpected exception: ") + e.what());
        }
        if (g_failures != failuresBefore) {
            ++failed;
            std::cout << "[ FAIL ] " << test.name << "\n";
        } else {
            ++passed;
            std::cout << "[  OK  ] " << test.name << "\n";
        }
    }
    std::cout << passed << " passed, " << failed << " failed, " << skipped << " skipped\n";
    if (failed > 0) {
        return 1;
    }
    return (passed == 0 && skipped > 0) ? 77 : 0;
}
