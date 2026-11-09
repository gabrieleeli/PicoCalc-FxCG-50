#include "limits.hpp"

#include "mem_pools.hpp"

#include <algorithm>
#include <cmath>

namespace fx::limits {
namespace {
constexpr std::size_t kHeavyReserve = 32u * 1024u; // left free for UI/temporaries

std::size_t clampCap(std::size_t cap, std::size_t derived) noexcept { return std::min(cap, derived); }
} // namespace

const RamProfile& ram() noexcept {
    return mem::available(mem::Pool::PSRAM_BOARD) ? kPsramProfile : kSramProfile;
}

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

bool graph3dZBuffer(std::size_t viewportPixels) noexcept {
    return ram().graph3dZBuffer && fitsHeavy(viewportPixels * sizeof(std::uint16_t) * 2);
}

std::size_t graph3dGrid(std::size_t viewportPixels) noexcept {
    // Vertex (16 B) + projected vertex (12 B) per grid point; the optional
    // z-buffer (2 B/pixel) is accounted for separately.
    const std::size_t zbuffer = graph3dZBuffer(viewportPixels) ? viewportPixels * sizeof(std::uint16_t) : 0;
    const std::size_t budget = heavyBudget() / 2;
    std::size_t grid = ram().graph3dMaxGrid;
    while (grid > 4 && (grid + 1) * (grid + 1) * 28 + zbuffer > budget) grid -= 4;
    return grid;
}

std::size_t matrixMaxDim() noexcept {
    // Inversion keeps the input, an n x 2n augmented copy and the result:
    // about 4 n^2 doubles; rows are separate allocations so the total budget,
    // not the largest block, is the binding constraint.
    const double n = std::floor(std::sqrt(static_cast<double>(heavyBudget()) / (4.0 * sizeof(double))));
    const auto derived = n < 1.0 ? std::size_t{1} : static_cast<std::size_t>(n);
    return clampCap(kMatrixMaxDim, derived);
}

std::size_t listMaxLength() noexcept {
    // Growing a vector needs old + new storage at once (2x), contiguous.
    const std::size_t largest = mem::largestFree(mem::Policy::PREFER_PSRAM);
    const std::size_t derived = std::min(heavyBudget(), largest) / (2 * sizeof(double));
    return clampCap(board::config::kHasPsram ? std::size_t{1000000} : std::size_t{999}, derived);
}

std::size_t historyCapacity() noexcept {
    // ~96 bytes per entry (expression string + value + vector slot); history
    // may use at most a quarter of the heavy budget.
    const std::size_t cap = board::config::kHasPsram ? 10000 : 30;
    const std::size_t derived = heavyBudget() / 4 / 96;
    return std::max<std::size_t>(1, clampCap(cap, derived));
}

} // namespace fx::limits
