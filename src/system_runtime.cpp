#include "system_runtime.hpp"

#include "board_config.hpp"

#if FXAI_ON_DEVICE
#include "hardware/regs/addressmap.h"
#include "hardware/structs/psm.h"
#include "hardware/structs/watchdog.h"
#include "hardware/sync.h"
#include "hardware/watchdog.h"
#include "pico/flash.h"
#include "pico/multicore.h"
#include "pico/platform.h"
#include "pico/time.h"
#endif

namespace fx::runtime {
namespace {
constexpr std::uint32_t kPaint = 0xA5A5A5A5u;

[[maybe_unused]] StackUsage measure(const std::uint32_t* stack, std::size_t words, bool guarded) noexcept {
    std::size_t untouched = 0;
    while (untouched < words && stack[untouched] == kPaint) ++untouched;
    return {words * sizeof(std::uint32_t), (words - untouched) * sizeof(std::uint32_t), guarded};
}
} // namespace

const char* describe(FaultKind kind) noexcept {
    switch (kind) {
    case FaultKind::None: return "nessuno";
    case FaultKind::StackOverflow: return "stack overflow (MSPLIM)";
    case FaultKind::HardFault: return "hard fault";
    }
    return "?";
}

#if FXAI_ON_DEVICE

namespace {
alignas(8) std::uint32_t g_appStack[board::config::kAppStackBytes / sizeof(std::uint32_t)];
alignas(8) std::uint32_t g_core1Stack[board::config::kCore1StackBytes / sizeof(std::uint32_t)];
bool g_appStackActive = false;
volatile bool g_core1Running = false;
void (*volatile g_core1Entry)() = nullptr;

// Flash safety helper: flash erase/program must stop the other core. Core 1
// only runs on demand, so the lockout is used exactly while it is alive.
std::uint32_t g_flashIrqState = 0;
bool g_lockedOutCore1 = false;

bool helperCoreInit(bool init) {
    if (init) multicore_lockout_victim_init();
    return true;
}
int helperEnter(std::uint32_t timeoutMs) {
    if (get_core_num() == 0 && g_core1Running) {
        if (!multicore_lockout_start_timeout_us(static_cast<std::uint64_t>(timeoutMs) * 1000u))
            return PICO_ERROR_TIMEOUT;
        g_lockedOutCore1 = true;
    } else if (get_core_num() != 0) {
        return PICO_ERROR_NOT_PERMITTED; // flash writes are only issued from core 0
    }
    g_flashIrqState = save_and_disable_interrupts();
    return PICO_OK;
}
int helperExit(std::uint32_t timeoutMs) {
    restore_interrupts_from_disabled(g_flashIrqState);
    if (g_lockedOutCore1) {
        g_lockedOutCore1 = false;
        return multicore_lockout_end_timeout_us(static_cast<std::uint64_t>(timeoutMs) * 1000u) ? PICO_OK
                                                                                               : PICO_ERROR_TIMEOUT;
    }
    return PICO_OK;
}
flash_safety_helper_t g_flashHelper = {helperCoreInit, helperEnter, helperExit};

void core1Trampoline() {
    flash_safe_execute_core_init(); // become a multicore-lockout victim
    g_core1Running = true;
    __sev();
    if (g_core1Entry) g_core1Entry();
    g_core1Running = false;
    for (;;) __wfe();
}

// --- HardFault ----------------------------------------------------------------
constexpr std::uint32_t kFaultMagic = 0xFA170000u;
} // namespace
} // namespace fx::runtime

extern "C" {
flash_safety_helper_t* get_flash_safety_helper(void) { return &fx::runtime::g_flashHelper; }

// Per-core emergency stacks for the HardFault handler (512 bytes each).
// Referenced only from the naked handler's inline assembly: keep the symbols
// emitted and global even under LTO.
__attribute__((used, externally_visible)) alignas(8) std::uint32_t fx_fault_stacks[2][128];

// Runs on the emergency stack with interrupts blocked by the fault priority.
// Only SRAM code and inline register accesses: the fault may have happened
// while XIP was unavailable.
__attribute__((used, externally_visible))
void __no_inline_not_in_flash_func(fx_hardfault_c)(const std::uint32_t* frame, std::uint32_t excReturn) {
    const std::uint32_t cfsr = *reinterpret_cast<volatile std::uint32_t*>(0xE000ED28u);
    const std::uint32_t core = *reinterpret_cast<volatile std::uint32_t*>(SIO_BASE) & 1u;
    const bool stackOverflow = (cfsr & (1u << 20)) != 0; // UFSR.STKOF
    std::uint32_t pc = 0, lr = 0;
    const auto f = reinterpret_cast<std::uintptr_t>(frame);
    // EXC_RETURN bit 2 == 0: frame on MSP. Only trust a fully stacked frame in SRAM.
    if (!stackOverflow && (excReturn & 4u) == 0 && f >= board::config::kSramBase &&
        f + 32u <= board::config::kSramBase + board::config::kSramBytes) {
        lr = frame[5];
        pc = frame[6];
    }
    const auto kind = static_cast<std::uint32_t>(stackOverflow ? fx::runtime::FaultKind::StackOverflow
                                                               : fx::runtime::FaultKind::HardFault);
    watchdog_hw->scratch[0] = fx::runtime::kFaultMagic | (kind << 8) | core;
    watchdog_hw->scratch[1] = cfsr;
    watchdog_hw->scratch[2] = pc;
    watchdog_hw->scratch[3] = lr;
    watchdog_hw->scratch[4] = 0; // normal boot, no bootrom vector
    hw_set_bits(&psm_hw->wdsel, PSM_WDSEL_BITS & ~(PSM_WDSEL_ROSC_BITS | PSM_WDSEL_XOSC_BITS));
    hw_set_bits(&watchdog_hw->ctrl, WATCHDOG_CTRL_TRIGGER_BITS);
    for (;;) __asm volatile("nop");
}

// Drop the stack limit first (a stack-limit violation leaves SP at MSPLIM, so
// any push would escalate to lockup), then move to this core's emergency stack.
// Lives in SRAM (.time_critical) so it still runs if the fault happens while
// XIP is disabled during a flash write; the vector table entry follows it.
__attribute__((naked, used, section(".time_critical.isr_hardfault"))) void isr_hardfault(void) {
    __asm volatile(
        "movs r0, #0\n"
        "msr  msplim, r0\n"
        "mrs  r0, msp\n"
        "mov  r1, lr\n"
        "ldr  r2, =0xd0000000\n" // SIO CPUID: 0 or 1
        "ldr  r2, [r2]\n"
        "adds r2, r2, #1\n"
        "lsls r2, r2, #9\n"      // (core + 1) * 512 = top of this core's stack
        "ldr  r3, =fx_fault_stacks\n"
        "add  r3, r3, r2\n"
        "mov  sp, r3\n"
        "b    fx_hardfault_c\n"
        ".ltorg\n"); // keep the literal pool next to the code (LTO may merge sections)
}
} // extern "C"

namespace fx::runtime {

[[noreturn]] void runOnAppStack(void (*entry)()) {
    for (auto& w : g_appStack) w = kPaint;
    const auto bottom = reinterpret_cast<std::uintptr_t>(&g_appStack[0]);
    const auto top = bottom + sizeof(g_appStack);
    g_appStackActive = true;
    // MSPLIM first (the new bottom is below the current SCRATCH_Y stack, so the
    // running code stays legal), then switch SP. Interrupt handlers use the new
    // stack from here on.
    __asm volatile(
        "msr msplim, %0\n"
        "mov sp, %1\n"
        "blx %2\n"
        "1: b 1b\n"
        :
        : "r"(bottom), "r"(top), "r"(entry)
        : "memory");
    __builtin_unreachable();
}

StackUsage core0Stack() noexcept {
    return measure(g_appStack, sizeof(g_appStack) / sizeof(g_appStack[0]), g_appStackActive);
}

StackUsage core1Stack() noexcept {
    return measure(g_core1Stack, sizeof(g_core1Stack) / sizeof(g_core1Stack[0]), PICO_USE_STACK_GUARDS != 0);
}

bool launchCore1(void (*entry)()) noexcept {
    if (g_core1Running || !entry) return false;
    for (auto& w : g_core1Stack) w = kPaint;
    g_core1Entry = entry;
    multicore_reset_core1();
    multicore_launch_core1_with_stack(core1Trampoline, g_core1Stack, sizeof(g_core1Stack));
    // Wait until core 1 registered itself as a lockout victim.
    const absolute_time_t until = make_timeout_time_ms(100);
    while (!g_core1Running && !time_reached(until)) __wfe();
    return g_core1Running;
}

bool core1Running() noexcept { return g_core1Running; }

void stopCore1() noexcept {
    multicore_reset_core1();
    g_core1Running = false;
}

bool takeLastFault(FaultRecord& out) noexcept {
    const std::uint32_t tag = watchdog_hw->scratch[0];
    if ((tag & 0xFFFF0000u) != kFaultMagic) return false;
    out.kind = static_cast<FaultKind>((tag >> 8) & 0xFFu);
    out.core = static_cast<std::uint8_t>(tag & 0xFFu);
    out.cfsr = watchdog_hw->scratch[1];
    out.pc = watchdog_hw->scratch[2];
    out.lr = watchdog_hw->scratch[3];
    watchdog_hw->scratch[0] = 0;
    return true;
}

#else // ------------------------------------------------------------------ host

namespace {
bool g_core1 = false;
}
[[noreturn]] void runOnAppStack(void (*entry)()) {
    entry();
    for (;;) {}
}
StackUsage core0Stack() noexcept { return {}; }
StackUsage core1Stack() noexcept { return {}; }
bool launchCore1(void (*entry)()) noexcept {
    if (g_core1 || !entry) return false;
    g_core1 = true;
    entry();
    g_core1 = false;
    return true;
}
bool core1Running() noexcept { return g_core1; }
void stopCore1() noexcept { g_core1 = false; }
bool takeLastFault(FaultRecord&) noexcept { return false; }

#endif

} // namespace fx::runtime
