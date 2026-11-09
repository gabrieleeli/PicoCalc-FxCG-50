#pragma once
// Runtime services shared by both cores:
//  * core 0 stack: the dedicated 32 KB STACK region of the linker script
//    (src/platform/pico/memmap_picocalc.ld), used from reset and guarded by
//    the Cortex-M33 MSPLIM register (PICO_USE_STACK_GUARDS installs it);
//  * optional core-1 worker on the 4 KB SCRATCH_X stack (also MSPLIM-guarded),
//    used by the dual-core allocator diagnostic;
//  * canary painting of both stacks for high-water measurement;
//  * HardFault handler that survives a stack-limit violation (it drops MSPLIM
//    and switches to a per-core emergency stack), records the fault in the
//    watchdog scratch registers and reboots cleanly; the next boot reports it;
//  * flash safety helper: flash writes lock core 1 out only while it runs.

#include <cstddef>
#include <cstdint>

namespace fx::runtime {

struct StackUsage {
    std::size_t sizeBytes{};
    std::size_t highWaterBytes{}; // deepest use observed through the canary pattern
    bool guarded{};               // hardware stack limit (MSPLIM) active
};

// Paints the unused part of the core 0 stack (call once, early in main()).
void paintStacks() noexcept;
StackUsage core0Stack() noexcept;
StackUsage core1Stack() noexcept;

// Core 1 worker (used by the dual-core allocator diagnostic).
bool launchCore1(void (*entry)()) noexcept; // false if already running
bool core1Running() noexcept;
void stopCore1() noexcept;                  // hard reset of core 1

enum class FaultKind : std::uint8_t { None = 0, StackOverflow = 1, HardFault = 2 };
struct FaultRecord {
    FaultKind kind{FaultKind::None};
    std::uint8_t core{};
    std::uint32_t cfsr{};
    std::uint32_t pc{};
    std::uint32_t lr{};
};
// Fault recorded before the last watchdog reboot (consumed by the call).
bool takeLastFault(FaultRecord& out) noexcept;
const char* describe(FaultKind kind) noexcept;

} // namespace fx::runtime
