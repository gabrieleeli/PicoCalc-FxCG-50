#pragma once
// Minimal check macro for the host suites: active even with NDEBUG and prints
// the failing expression before aborting the test process.
#include <cstdio>
#include <cstdlib>

#define FX_CHECK(expr)                                                                   \
    do {                                                                                 \
        if (!(expr)) {                                                                   \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #expr); \
            std::abort();                                                                \
        }                                                                                \
    } while (0)

namespace fxtest {
// Deterministic xorshift PRNG so stress tests are reproducible.
struct Rng {
    unsigned long long s{0x9E3779B97F4A7C15ull};
    unsigned long long next() noexcept { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return s; }
    unsigned below(unsigned n) noexcept { return n ? static_cast<unsigned>(next() % n) : 0u; }
};
} // namespace fxtest
