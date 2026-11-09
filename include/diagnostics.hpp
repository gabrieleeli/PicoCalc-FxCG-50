#pragma once
// On-device diagnostics (System -> Test). Each test returns a few text lines
// for the screen (also printed on USB stdio). Tests that need hardware report
// "solo su dispositivo" in host builds; the allocator stress kernel is portable
// and host-tested.

#include <cstddef>
#include <cstdint>

namespace fx::diag {

enum class Test : std::uint8_t {
    PsramInfo,
    PsramPattern,
    AllocatorStress,
    DualCoreStress,
    FlashPsram,
    Framebuffer,
    WiFi,
    Storage,
    SdCard,
    Count
};
inline constexpr std::size_t kTestCount = static_cast<std::size_t>(Test::Count);

struct Result {
    bool ran{};
    bool passed{};
    std::uint8_t lineCount{};
    char lines[8][52]{};
    std::uint32_t elapsedUs{};
};

const char* name(Test test) noexcept;
Result run(Test test) noexcept;

struct StressStats {
    std::size_t operations{};
    std::size_t allocations{};
    std::size_t failures{};    // allocation requests refused (expected under pressure)
    std::size_t corruptions{}; // pattern mismatches (must be 0)
    std::size_t peakLive{};
};
// Random alloc/free with pattern verification on every policy available.
StressStats allocatorStress(std::uint32_t seed, std::size_t operations, std::size_t maxBlock) noexcept;

} // namespace fx::diag
