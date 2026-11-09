#pragma once
// Check macro of the v1.9.0 memory tests: counted like CHECK (test_util.hpp),
// active with NDEBUG, and safe to use from several threads (the allocator
// stress runs two). fxtest::finish() prints the same summary line as
// testutil::finish(): "<suite>: N checks, M failures".
#include <atomic>
#include <cstdio>
#include <cstdlib>

namespace fxtest {
inline std::atomic<int>& checks() { static std::atomic<int> c{0}; return c; }
inline std::atomic<int>& failures() { static std::atomic<int> f{0}; return f; }
inline int finish(const char* suite) {
    std::printf("%s: %d checks, %d failures\n", suite, checks().load(), failures().load());
    return failures().load() == 0 ? 0 : 1;
}
// Deterministic xorshift PRNG so stress tests are reproducible.
struct Rng {
    unsigned long long s{0x9E3779B97F4A7C15ull};
    unsigned long long next() noexcept { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return s; }
    unsigned below(unsigned n) noexcept { return n ? static_cast<unsigned>(next() % n) : 0u; }
};
} // namespace fxtest

#define FX_CHECK(expr)                                                                  \
    do {                                                                                \
        ::fxtest::checks().fetch_add(1, std::memory_order_relaxed);                     \
        if (!(expr)) {                                                                  \
            ::fxtest::failures().fetch_add(1, std::memory_order_relaxed);               \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr);         \
        }                                                                               \
    } while (0)
