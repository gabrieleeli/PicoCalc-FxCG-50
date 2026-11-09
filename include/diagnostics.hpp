#pragma once
// Self-tests for the validation on the PicoCalc (SYSTEM > DIAG). Every result
// is a line of text shown on the screen and printed on the USB serial port
// ("[DIAG] ...") so that it can be sent back as a report.
//  - v1.8.0: the microSD card (detection, initialisation, mounting, write /
//    read / verify / delete with timings) and GRAPH (sampling and G-Solve
//    times of a fixed set of graphs);
//  - v1.9.0: memory and hardware of the Pimoroni Pico Plus 2 W: PSRAM driver
//    state and pattern test, allocator stress on one and two cores, PSRAM
//    content and QMI configuration across a flash write, LCD update time.
// The user's data is not touched: the card test uses its own folder
// PICOCALC/DIAG and removes it, the graph test its own functions, the memory
// tests their own blocks and the 4 KB diagnostic flash sector.
#include "file_store.hpp"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace storage { class FlashDevice; }
namespace fx {
struct Workspace;
namespace app { class Platform; }
}

namespace fx::diag {
struct Line {
    bool ok;
    std::string text;
};
using Clock = std::function<std::uint32_t()>;   // milliseconds

std::vector<Line> testStorage(files::FileSystem* fs, const Clock& now);
std::vector<Line> testGraph(Workspace& ws, const Clock& now);

// Memory report: board, PSRAM driver (ID, timing, boot self-test), pools,
// stacks, flash layout. Fails when the board has PSRAM and it is disabled.
std::vector<Line> testMemory(app::Platform& platform);
// Pattern test (data bus, random samples, cached/uncached paths) on a block
// of the PSRAM pool; skipped on boards without PSRAM.
std::vector<Line> testPsram(const Clock& now);
// Random allocations on every pool/policy with pattern checks, then the TLSF
// integrity walk; with dualCore the same stress runs on core 1 at the same
// time (device only).
std::vector<Line> testAllocator(const Clock& now, bool dualCore);
// Fills a PSRAM block (larger than the XIP cache), erases and programs the
// diagnostic flash sector, then checks the flash data, the PSRAM content
// (cached and, on the device, uncached alias) and that PSRAM still accepts writes.
std::vector<Line> testFlashPsram(storage::FlashDevice* sector, const Clock& now);
// Framebuffer placement and duration of a full LCD update.
std::vector<Line> testDisplay(app::Platform& platform);

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
