#include "limits.hpp"

#include "mem_pools.hpp"

#include <algorithm>
#include <cmath>

namespace fx::limits {
namespace {
constexpr std::size_t kHeavyReserve = 32u * 1024u; // left free for UI/temporaries

std::size_t clampCap(std::size_t cap, std::size_t derived) noexcept { return std::min(cap, derived); }
} // namespace

const Profile& profile() noexcept { return kProfile; }

std::size_t heavyBudget() noexcept {
    // Heavy data prefers PSRAM; without PSRAM the SRAM heap minus its reserve
    // is all there is (mem::largestFree already subtracts the SRAM reserve).
    const std::size_t psramFree = mem::available(mem::Pool::PSRAM_BOARD) ? mem::stats(mem::Pool::PSRAM_BOARD).free : 0;
    const auto sram = mem::stats(mem::Pool::SRAM);
    std::size_t sramFree = sram.total == 0 ? sram.largestFree : sram.free; // host heap: unbounded
    sramFree = sramFree > mem::kSramReserveBytes ? sramFree - mem::kSramReserveBytes : 0;
    std::size_t budget = psramFree > sramFree ? psramFree : sramFree;
    if (psramFree && sramFree) budget = psramFree + sramFree / 2; // SRAM spill is allowed but discouraged
    return budget > kHeavyReserve ? budget - kHeavyReserve : 0;
}

bool fitsHeavy(std::size_t bytes) noexcept {
    return bytes <= heavyBudget() && mem::canAllocate(bytes, mem::Policy::PREFER_PSRAM);
}

std::size_t matrixMaxDim() noexcept {
    // Inversion keeps the input, an n x 2n augmented copy and the result:
    // about 4 n^2 doubles; rows are separate allocations so the total budget,
    // not the largest block, is the binding constraint.
    const double n = std::floor(std::sqrt(static_cast<double>(heavyBudget()) / (4.0 * sizeof(double))));
    const auto derived = n < 1.0 ? std::size_t{1} : static_cast<std::size_t>(n);
    return clampCap(kProfile.matrixMaxDim, derived);
}

std::size_t listMaxLength() noexcept {
    // Growing a vector needs old + new storage at once (2x), contiguous.
    const std::size_t largest = mem::largestFree(mem::Policy::PREFER_PSRAM);
    const std::size_t derived = std::min(heavyBudget(), largest) / (2 * sizeof(double));
    return clampCap(kProfile.listMaxLength, derived);
}

std::size_t historyCapacity() noexcept {
    // ~96 bytes per entry (expression string + value + vector slot); history
    // may use at most a quarter of the heavy budget.
    const std::size_t derived = heavyBudget() / 4 / 96;
    return std::max<std::size_t>(1, clampCap(kProfile.historyEntries, derived));
}

std::size_t editorMaxTokens() noexcept {
    // AST nodes are small and hot: they prefer SRAM (PREFER_SRAM, ~112 bytes
    // each with the shared_ptr control block). Use at most half the SRAM left.
    const auto sram = mem::stats(mem::Pool::SRAM);
    const std::size_t sramFree = sram.total == 0 ? sram.largestFree : sram.free;
    std::size_t derived = sramFree > mem::kSramReserveBytes ? (sramFree - mem::kSramReserveBytes) / 2 / 112 : 0;
    if (mem::available(mem::Pool::PSRAM_BOARD)) derived += mem::stats(mem::Pool::PSRAM_BOARD).free / 8 / 112;
    return std::max<std::size_t>(16, clampCap(kProfile.editorMaxTokens, derived));
}

std::size_t graphMaxFunctions() noexcept {
    // Each function needs a sample buffer of at least one float per column (320).
    const std::size_t derived = heavyBudget() / (320 * sizeof(float) * 2);
    return std::max<std::size_t>(1, clampCap(kProfile.graphMaxFunctions, derived));
}

std::size_t graphSamplesPerPixel(std::size_t functions, std::size_t columns) noexcept {
    if (functions == 0 || columns == 0) return 1;
    // min/max per column are derived from the samples; keep <= 1/4 of the budget.
    const std::size_t perSample = functions * columns * sizeof(float);
    const std::size_t derived = perSample ? heavyBudget() / 4 / perSample : 1;
    return std::max<std::size_t>(1, clampCap(kProfile.graphSamplesPerPixel, derived));
}

std::size_t graph3dGrid(std::size_t viewportPixels) noexcept {
    // Vertex (16 B) + projected vertex (12 B) per grid point; the optional
    // z-buffer (2 B/pixel) is accounted for separately.
    const std::size_t zbuffer = graph3dZBuffer(viewportPixels) ? viewportPixels * sizeof(std::uint16_t) : 0;
    const std::size_t budget = heavyBudget() / 2;
    std::size_t grid = kProfile.graph3dMaxGrid;
    while (grid > 4 && (grid + 1) * (grid + 1) * 28 + zbuffer > budget) grid -= 4;
    return grid;
}

bool graph3dZBuffer(std::size_t viewportPixels) noexcept {
    return kProfile.graph3dZBuffer && fitsHeavy(viewportPixels * sizeof(std::uint16_t) * 2);
}

std::size_t windowSnapshots(std::size_t frameBytes) noexcept {
    if (kProfile.windowSnapshots == 0 || frameBytes == 0 || !mem::available(mem::Pool::PSRAM_BOARD)) return 0;
    const std::size_t derived = mem::stats(mem::Pool::PSRAM_BOARD).free / 4 / frameBytes;
    return clampCap(kProfile.windowSnapshots, derived);
}

std::size_t aiRequestCapacity() noexcept {
    return std::max<std::size_t>(1024, clampCap(kProfile.aiRequestBytes, heavyBudget() / 8));
}

std::size_t aiResponseCapacity() noexcept {
    return std::max<std::size_t>(2048, clampCap(kProfile.aiResponseBytes, heavyBudget() / 4));
}

} // namespace fx::limits
