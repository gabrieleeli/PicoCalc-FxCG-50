#pragma once
// Multi-pool memory architecture.
//
//   Pool::SRAM            standard heap (newlib malloc) in the 520 KiB SRAM
//   Pool::PSRAM_BOARD     8 MiB APS6404L on the Pimoroni Pico Plus 2 W (QMI CS1),
//                         managed by a TLSF allocator
//   Pool::PSRAM_PICOCALC  PSRAM fitted on the PicoCalc mainboard: interface only,
//                         no driver until the ClockworkPi wiring is verified
//
// The pools are distinct banks with different speed/semantics: they are never
// presented as one contiguous 16 MiB region. The same API works on the Pico 2 W
// (only SRAM is available): application code never needs #ifdefs.
//
// Placement:
//   * mem::alloc / mem::tryAlloc with an explicit Pool or a Policy;
//   * std::pmr containers through mem::resource(Pool|Policy);
//   * global operator new (src/mem_new.cpp): allocations of at least
//     FXAI_PSRAM_NEW_THRESHOLD bytes use PREFER_PSRAM, smaller ones PREFER_SRAM,
//     unless a mem::PlacementScope is active on the current core.
// Allocation is thread-safe on both cores (spin lock + IRQ masking on device).

#include <cstddef>
#include <cstdint>
#include <memory_resource>
#include <new>

namespace mem {

enum class Pool : std::uint8_t { SRAM = 0, PSRAM_BOARD = 1, PSRAM_PICOCALC = 2 };
inline constexpr std::size_t kPoolCount = 3;

enum class Policy : std::uint8_t {
    PREFER_PSRAM, // PSRAM_BOARD, then SRAM
    SRAM_ONLY,
    PSRAM_ONLY,   // PSRAM_BOARD only (fails on boards without PSRAM)
    PREFER_SRAM,  // SRAM (keeping a safety reserve), then PSRAM_BOARD
};

inline constexpr std::size_t kDefaultAlign = __STDCPP_DEFAULT_NEW_ALIGNMENT__;
// SRAM kept free for small critical allocations (exception objects, stdio,
// SDK internals) when a request may fall back to PSRAM.
inline constexpr std::size_t kSramReserveBytes = 16u * 1024u;

struct PoolStats {
    const char* name{};
    bool available{};
    std::size_t total{};        // bytes managed by the pool
    std::size_t used{};         // bytes in use (allocator overhead included)
    std::size_t free{};         // total - used
    std::size_t largestFree{};  // largest single allocation expected to succeed now
    std::size_t peakUsed{};     // high-water mark of `used`
    std::size_t failures{};     // failed allocation requests on this pool
    std::size_t allocations{};  // live allocations made through mem::
};

// Allocation API. alloc() throws std::bad_alloc, tryAlloc() returns nullptr.
void* alloc(Pool pool, std::size_t size, std::size_t align = kDefaultAlign);
void* alloc(Policy policy, std::size_t size, std::size_t align = kDefaultAlign);
void* tryAlloc(Pool pool, std::size_t size, std::size_t align = kDefaultAlign) noexcept;
void* tryAlloc(Policy policy, std::size_t size, std::size_t align = kDefaultAlign) noexcept;
void free(void* p) noexcept;

PoolStats stats(Pool pool) noexcept;
bool available(Pool pool) noexcept;
const char* name(Pool pool) noexcept;
// Pool that owns `p` (SRAM for anything that is not a registered PSRAM range).
Pool poolOf(const void* p) noexcept;
// Largest allocation the policy can currently satisfy (0 if none).
std::size_t largestFree(Policy policy) noexcept;
// True if `bytes` can be allocated under `policy` while leaving `reserve` bytes.
bool canAllocate(std::size_t bytes, Policy policy, std::size_t reserve = 0) noexcept;

// std::pmr adapters (allocation failure throws std::bad_alloc, as pmr requires).
std::pmr::memory_resource* resource(Pool pool) noexcept;
std::pmr::memory_resource* resource(Policy policy) noexcept;

// Overrides the operator new placement on the current core while alive.
class PlacementScope {
public:
    explicit PlacementScope(Policy policy) noexcept;
    ~PlacementScope();
    PlacementScope(const PlacementScope&) = delete;
    PlacementScope& operator=(const PlacementScope&) = delete;
private:
    std::uint8_t previous_;
};

// Policy operator new uses for a request of `size` bytes on this core.
Policy newPolicyFor(std::size_t size) noexcept;
std::size_t newThreshold() noexcept;

// Boot-time configuration (device): called once from main() on core 0.
void init() noexcept;
bool registerPsramBoard(void* cachedBase, std::size_t bytes) noexcept;
// Runs the integrity walk of every TLSF pool (diagnostics/tests).
bool checkIntegrity() noexcept;
// Requests that failed on every pool allowed by their policy ("out of memory").
std::size_t outOfMemoryEvents() noexcept;
// mem::free() calls with pointers that no pool owns (ignored, never crash).
std::size_t invalidFrees() noexcept;

// Allocation helpers with RAII ownership (memory released through mem::free).
struct Deleter { void operator()(void* p) const noexcept { mem::free(p); } };

namespace testing {
// Host-only simulation hooks (no-ops on the device):
//  simulateSram(bytes):  SRAM pool becomes a TLSF arena of `bytes` (0 = real heap)
//  simulatePsram(bytes): PSRAM_BOARD becomes a TLSF arena of `bytes` (0 = absent)
void simulateSram(std::size_t bytes);
void simulatePsram(std::size_t bytes);
void reset();
} // namespace testing

} // namespace mem
