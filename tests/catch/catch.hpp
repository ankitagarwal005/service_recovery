// Minimal Catch2-v2 compatible single-header (offline build stub)
#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <stdexcept>
#include <sstream>

namespace Catch {
struct TestCase { std::string name, tags; std::function<void()> fn; };

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> r; return r;
}

struct Registrar {
    Registrar(const char* n, const char* t, std::function<void()> f) {
        registry().push_back({n, t, std::move(f)});
    }
};

struct AssertionFailed : std::exception {
    std::string msg;
    explicit AssertionFailed(std::string m) : msg(std::move(m)) {}
    const char* what() const noexcept override { return msg.c_str(); }
};

inline int run_all() {
    int passed = 0, failed = 0;
    for (auto& tc : registry()) {
        try {
            tc.fn();
            std::cout << "  [PASS] " << tc.name << "\n";
            ++passed;
        } catch (const AssertionFailed& e) {
            std::cout << "  [FAIL] " << tc.name << "\n         " << e.msg << "\n";
            ++failed;
        } catch (const std::exception& e) {
            std::cout << "  [FAIL] " << tc.name << "\n         unexpected: " << e.what() << "\n";
            ++failed;
        }
    }
    std::cout << "\n" << passed << " passed, " << failed << " failed (" << (passed+failed) << " total)\n";
    return failed == 0 ? 0 : 1;
}
} // namespace Catch

#ifdef CATCH_CONFIG_MAIN
int main(int, char**) {
    std::cout << "=== Service Recovery Scheduler — Unit Tests ===\n\n";
    return Catch::run_all();
}
#endif

#define CATCH_LINE_CAT(a, b) CATCH_LINE_CAT2(a, b)
#define CATCH_LINE_CAT2(a, b) a##b

#define TEST_CASE(name, tags) \
    static void CATCH_LINE_CAT(catch_test_, __LINE__)(); \
    namespace { static ::Catch::Registrar CATCH_LINE_CAT(catch_reg_, __LINE__)(name, tags, CATCH_LINE_CAT(catch_test_, __LINE__)); } \
    static void CATCH_LINE_CAT(catch_test_, __LINE__)()

#define REQUIRE(expr) \
    do { if (!(expr)) { \
        std::ostringstream _oss; \
        _oss << "REQUIRE(" #expr ") at line " << __LINE__; \
        throw ::Catch::AssertionFailed(_oss.str()); \
    } } while(0)

#define CHECK(expr) REQUIRE(expr)

#define REQUIRE_THROWS_AS(expr, ExType) \
    do { \
        bool _caught = false; \
        try { (void)(expr); } catch (const ExType&) { _caught = true; } catch (...) {} \
        if (!_caught) { \
            std::ostringstream _oss; \
            _oss << "REQUIRE_THROWS_AS(" #expr ", " #ExType ") did not throw at line " << __LINE__; \
            throw ::Catch::AssertionFailed(_oss.str()); \
        } \
    } while(0)
