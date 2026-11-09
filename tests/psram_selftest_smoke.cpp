// Host tests for the PSRAM pattern self-test: a healthy memory passes, while
// simulated hardware faults (stuck data bit, stuck/shorted address line, lost
// cache write-back, stale cached reads) are all detected.
#include "psram_selftest.hpp"
#include "test_support.hpp"

#include <cstdint>
#include <vector>

namespace {
struct GoodMem {
    std::vector<std::uint32_t> cells;
    explicit GoodMem(std::size_t words) : cells(words, 0) {}
    std::size_t words() const noexcept { return cells.size(); }
    void writeUncached(std::size_t i, std::uint32_t v) noexcept { cells[i] = v; }
    std::uint32_t readUncached(std::size_t i) noexcept { return cells[i]; }
    void writeCached(std::size_t i, std::uint32_t v) noexcept { cells[i] = v; }
    std::uint32_t readCached(std::size_t i) noexcept { return cells[i]; }
    void cleanCache() noexcept {}
    void invalidateWords(std::size_t, std::size_t) noexcept {}
};

struct StuckDataBit : GoodMem {
    using GoodMem::GoodMem;
    std::uint32_t readUncached(std::size_t i) noexcept { return cells[i] | 0x80u; } // D7 stuck high
};

struct StuckAddressLine : GoodMem {
    std::size_t mask;
    StuckAddressLine(std::size_t words, unsigned line) : GoodMem(words), mask(~(std::size_t{1} << line)) {}
    void writeUncached(std::size_t i, std::uint32_t v) noexcept { cells[i & mask] = v; } // A<line> stuck low
    std::uint32_t readUncached(std::size_t i) noexcept { return cells[i & mask]; }
    void writeCached(std::size_t i, std::uint32_t v) noexcept { cells[i & mask] = v; }
    std::uint32_t readCached(std::size_t i) noexcept { return cells[i & mask]; }
};

// Models an XIP cache whose clean operation never reaches the device.
struct LostWriteBack : GoodMem {
    std::vector<std::uint32_t> cache;
    explicit LostWriteBack(std::size_t words) : GoodMem(words), cache(words, 0) {}
    void writeCached(std::size_t i, std::uint32_t v) noexcept { cache[i] = v; }
    std::uint32_t readCached(std::size_t i) noexcept { return cache[i]; }
};

// Models a cache whose invalidate operation is ignored (stale lines served).
struct StaleCache : GoodMem {
    std::vector<std::uint32_t> cache;
    std::vector<bool> valid;
    explicit StaleCache(std::size_t words) : GoodMem(words), cache(words, 0), valid(words, false) {}
    void writeCached(std::size_t i, std::uint32_t v) noexcept { cache[i] = v; valid[i] = true; }
    std::uint32_t readCached(std::size_t i) noexcept { return valid[i] ? cache[i] : cells[i]; }
    void cleanCache() noexcept { for (std::size_t i = 0; i < cells.size(); ++i) if (valid[i]) cells[i] = cache[i]; }
};
} // namespace

int main() {
    constexpr std::size_t kWords = 1u << 16; // 256 KiB window
    psram::SelfTestOptions opt;
    opt.samples = 2048;

    GoodMem good(kWords);
    auto r = psram::runPatternTests(good, opt);
    FX_CHECK(r.passed && r.dataBus && r.addressBus && r.randomPattern && r.cachedPath && r.samples == 2048);

    StuckDataBit data(kWords);
    r = psram::runPatternTests(data, opt);
    FX_CHECK(!r.passed && !r.dataBus && r.failedStep != nullptr);

    for (unsigned line : {0u, 5u, 15u}) {
        StuckAddressLine addr(kWords, line);
        r = psram::runPatternTests(addr, opt);
        FX_CHECK(!r.passed && r.dataBus && !r.addressBus);
    }
    // Without the address-bus step (partial window) aliasing is still caught by
    // the random samples written before being read back.
    psram::SelfTestOptions partial = opt;
    partial.addressBus = false;
    StuckAddressLine aliasing(kWords, 12);
    r = psram::runPatternTests(aliasing, partial);
    FX_CHECK(!r.passed && !r.randomPattern);

    LostWriteBack lost(kWords);
    r = psram::runPatternTests(lost, opt);
    FX_CHECK(!r.passed && r.randomPattern && !r.cachedPath);

    StaleCache stale(kWords);
    r = psram::runPatternTests(stale, opt);
    FX_CHECK(!r.passed && !r.cachedPath);

    GoodMem odd(3000); // non power-of-two window: address-bus test refuses to run
    r = psram::runPatternTests(odd, opt);
    FX_CHECK(!r.passed && !r.addressBus);
    r = psram::runPatternTests(odd, partial);
    FX_CHECK(r.passed);

    return fxtest::finish("psram_selftest_smoke");
}
