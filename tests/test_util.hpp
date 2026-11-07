#pragma once
// Minimal test helpers: every failed check is reported (not only the first),
// and checks stay active in release builds (unlike assert under NDEBUG).
#include <cmath>
#include <cstdio>
#include <exception>
#include <string>

namespace testutil {
inline int& failures() { static int f = 0; return f; }
inline int& checks() { static int c = 0; return c; }
inline void report(bool ok, const char* expr, const char* file, int line, const std::string& extra = {}) {
    ++checks();
    if (!ok) { ++failures(); std::fprintf(stderr, "FAIL %s:%d: %s %s\n", file, line, expr, extra.c_str()); }
}
inline int finish(const char* suite) {
    std::printf("%s: %d checks, %d failures\n", suite, checks(), failures());
    return failures() == 0 ? 0 : 1;
}
} // namespace testutil

#define CHECK(cond) ::testutil::report(static_cast<bool>(cond), #cond, __FILE__, __LINE__)
#define CHECK_NEAR(a, b, tol) do { const double va_ = (a), vb_ = (b); \
    ::testutil::report(std::abs(va_ - vb_) <= (tol), #a " ~= " #b, __FILE__, __LINE__, \
    "(" + std::to_string(va_) + " vs " + std::to_string(vb_) + ")"); } while (0)
#define CHECK_EQ_STR(a, b) do { const std::string sa_ = (a), sb_ = (b); \
    ::testutil::report(sa_ == sb_, #a " == " #b, __FILE__, __LINE__, "('" + sa_ + "' vs '" + sb_ + "')"); } while (0)
#define CHECK_THROWS(stmt) do { bool thrown_ = false; try { (void)(stmt); } catch (const std::exception&) { thrown_ = true; } \
    ::testutil::report(thrown_, "throws: " #stmt, __FILE__, __LINE__); } while (0)
#define CHECK_THROWS_AS(stmt, type) do { bool thrown_ = false; try { (void)(stmt); } catch (const type&) { thrown_ = true; } catch (...) {} \
    ::testutil::report(thrown_, "throws " #type ": " #stmt, __FILE__, __LINE__); } while (0)
#define CHECK_NOTHROW(stmt) do { bool ok_ = true; std::string w_; try { (void)(stmt); } catch (const std::exception& e_) { ok_ = false; w_ = e_.what(); } \
    ::testutil::report(ok_, "nothrow: " #stmt, __FILE__, __LINE__, w_); } while (0)
