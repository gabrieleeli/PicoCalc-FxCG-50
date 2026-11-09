#pragma once
// Portable PSRAM pattern tests. The algorithms are templates over a memory
// accessor so that the same code runs on the device (volatile XIP pointers,
// cached and uncached aliases) and on the host (plain buffers or fault models
// that simulate stuck data/address lines).
//
// Accessor concept (word = 32-bit, index = word offset from the window base):
//   std::size_t words() const;
//   void writeUncached(std::size_t i, std::uint32_t v);
//   std::uint32_t readUncached(std::size_t i);
//   void writeCached(std::size_t i, std::uint32_t v);
//   std::uint32_t readCached(std::size_t i);
//   void cleanCache();                            // write back dirty lines
//   void invalidateWords(std::size_t i, std::size_t n); // drop cached copies
//
// Data bus: walking ones/zeros on one word (uncached, i.e. real QSPI cycles).
// Address bus: M. Barr's power-of-two offset test (needs a power-of-two window;
//   detects address lines stuck high/low or shorted -> aliasing).
// Random samples: xorshift32 values on samples spread over the whole window:
//   pass 1 uncached write/read, pass 2 cached write + clean + uncached read
//   (write-back path), pass 3 uncached write + invalidate + cached read.

#include <cstddef>
#include <cstdint>

namespace psram {

struct SelfTestResult {
    bool passed{};
    bool dataBus{};
    bool addressBus{};
    bool randomPattern{};
    bool cachedPath{};
    const char* failedStep{};
    std::size_t failWordIndex{};
    std::uint32_t expected{};
    std::uint32_t actual{};
    std::uint32_t samples{};
    std::uint32_t elapsedUs{};
};

struct SelfTestOptions {
    bool addressBus{true};       // requires exclusive use of the whole (power-of-two) window
    std::uint32_t samples{4096}; // random samples spread over the window
    std::uint32_t seed{0x2545F491u};
};

namespace detail {
inline std::uint32_t xorshift32(std::uint32_t& s) noexcept {
    s ^= s << 13; s ^= s >> 17; s ^= s << 5;
    return s;
}
inline bool fail(SelfTestResult& r, const char* step, std::size_t i, std::uint32_t exp, std::uint32_t act) noexcept {
    r.failedStep = step; r.failWordIndex = i; r.expected = exp; r.actual = act;
    return false;
}

template <class Mem>
bool dataBus(Mem& m, SelfTestResult& r) noexcept {
    for (std::uint32_t pattern = 1; pattern != 0; pattern <<= 1) {
        m.writeUncached(0, pattern);
        const std::uint32_t got = m.readUncached(0);
        if (got != pattern) return fail(r, "data bus (walking 1)", 0, pattern, got);
        m.writeUncached(0, ~pattern);
        const std::uint32_t inv = m.readUncached(0);
        if (inv != ~pattern) return fail(r, "data bus (walking 0)", 0, ~pattern, inv);
    }
    return true;
}

template <class Mem>
bool addressBus(Mem& m, SelfTestResult& r) noexcept {
    const std::size_t words = m.words();
    if (words < 2 || (words & (words - 1)) != 0) return fail(r, "address bus (window not power of two)", 0, 0, 0);
    const std::size_t mask = words - 1;
    constexpr std::uint32_t pattern = 0xAAAAAAAAu, anti = 0x55555555u;
    for (std::size_t off = 1; (off & mask) != 0; off <<= 1) m.writeUncached(off, pattern);
    m.writeUncached(0, anti);
    for (std::size_t off = 1; (off & mask) != 0; off <<= 1) {
        const std::uint32_t got = m.readUncached(off);
        if (got != pattern) return fail(r, "address bus (stuck high)", off, pattern, got);
    }
    m.writeUncached(0, pattern);
    for (std::size_t test = 1; (test & mask) != 0; test <<= 1) {
        m.writeUncached(test, anti);
        const std::uint32_t base = m.readUncached(0);
        if (base != pattern) return fail(r, "address bus (stuck low)", test, pattern, base);
        for (std::size_t off = 1; (off & mask) != 0; off <<= 1) {
            if (off == test) continue;
            const std::uint32_t got = m.readUncached(off);
            if (got != pattern) return fail(r, "address bus (shorted)", off, pattern, got);
        }
        m.writeUncached(test, pattern);
    }
    return true;
}

inline std::size_t sampleIndex(std::size_t words, std::uint32_t samples, std::uint32_t k, std::uint32_t jitter) noexcept {
    const std::size_t stride = words / samples ? words / samples : 1;
    const std::size_t idx = static_cast<std::size_t>(k) * stride + (jitter % stride);
    return idx < words ? idx : words - 1;
}

template <class Mem>
bool randomPattern(Mem& m, const SelfTestOptions& o, std::uint32_t samples, SelfTestResult& r) noexcept {
    const std::size_t words = m.words();
    // Pass 1: uncached write, uncached read-back after *all* writes (catches aliasing).
    std::uint32_t s = o.seed, jit = o.seed ^ 0x9E3779B9u;
    for (std::uint32_t k = 0; k < samples; ++k)
        m.writeUncached(sampleIndex(words, samples, k, xorshift32(jit)), xorshift32(s));
    s = o.seed; jit = o.seed ^ 0x9E3779B9u;
    for (std::uint32_t k = 0; k < samples; ++k) {
        const std::size_t i = sampleIndex(words, samples, k, xorshift32(jit));
        const std::uint32_t exp = xorshift32(s), got = m.readUncached(i);
        if (got != exp) return fail(r, "random pattern (uncached)", i, exp, got);
    }
    return true;
}

template <class Mem>
bool cachedPath(Mem& m, const SelfTestOptions& o, std::uint32_t samples, SelfTestResult& r) noexcept {
    const std::size_t words = m.words();
    // Pass 2: write through the cache, clean it, verify the device copy.
    std::uint32_t s = o.seed * 3u + 1u, jit = o.seed ^ 0x7F4A7C15u;
    for (std::uint32_t k = 0; k < samples; ++k)
        m.writeCached(sampleIndex(words, samples, k, xorshift32(jit)), ~xorshift32(s));
    m.cleanCache();
    s = o.seed * 3u + 1u; jit = o.seed ^ 0x7F4A7C15u;
    for (std::uint32_t k = 0; k < samples; ++k) {
        const std::size_t i = sampleIndex(words, samples, k, xorshift32(jit));
        const std::uint32_t exp = ~xorshift32(s), got = m.readUncached(i);
        if (got != exp) return fail(r, "cache write-back", i, exp, got);
    }
    // Pass 3: write the device directly, drop stale lines, read through the cache.
    s = o.seed * 7u + 3u; jit = o.seed ^ 0x7F4A7C15u;
    for (std::uint32_t k = 0; k < samples; ++k) {
        const std::size_t i = sampleIndex(words, samples, k, xorshift32(jit));
        m.writeUncached(i, xorshift32(s));
        m.invalidateWords(i, 1);
    }
    s = o.seed * 7u + 3u; jit = o.seed ^ 0x7F4A7C15u;
    for (std::uint32_t k = 0; k < samples; ++k) {
        const std::size_t i = sampleIndex(words, samples, k, xorshift32(jit));
        const std::uint32_t exp = xorshift32(s), got = m.readCached(i);
        if (got != exp) return fail(r, "cached read", i, exp, got);
    }
    return true;
}
} // namespace detail

template <class Mem>
SelfTestResult runPatternTests(Mem& m, const SelfTestOptions& o = {}) noexcept {
    SelfTestResult r;
    const std::size_t words = m.words();
    if (words == 0) { detail::fail(r, "empty window", 0, 0, 0); return r; }
    std::uint32_t samples = o.samples;
    if (samples > words) samples = static_cast<std::uint32_t>(words);
    if (samples == 0) samples = 1;
    r.samples = samples;
    r.dataBus = detail::dataBus(m, r);
    if (!r.dataBus) return r;
    if (o.addressBus) {
        r.addressBus = detail::addressBus(m, r);
        if (!r.addressBus) return r;
    } else {
        r.addressBus = true; // not requested (partial window)
    }
    r.randomPattern = detail::randomPattern(m, o, samples, r);
    if (!r.randomPattern) return r;
    r.cachedPath = detail::cachedPath(m, o, samples, r);
    r.passed = r.cachedPath;
    return r;
}

} // namespace psram
