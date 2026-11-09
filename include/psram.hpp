#pragma once
// APS6404L QSPI PSRAM driver for the Pimoroni Pico Plus 2 W (QMI window 1,
// chip select XIP_CS1 on the GPIO given by the SDK board header: GPIO47).
//
// Initialisation sequence ported from SparkFun Electronics, sfe_psram.c
// (sparkfun/sparkfun-pico, MIT License, (c) 2024 SparkFun Electronics), which
// in turn derives from the CircuitPython/Raspberry Pi RP2350 PSRAM bring-up
// (raspberrypi/pico-sdk-rp2350 issue #12). Steps:
//   1. route the CS pin to XIP_CS1;
//   2. QMI direct mode: QPI exit (0xF5), Read ID (0x9F) -> KGD must be 0x5D;
//   3. Reset Enable (0x66) + Reset (0x99), Enter Quad mode (0x35);
//   4. M1 timing from clk_sys: CLKDIV for SCK <= 109 MHz, MAX_SELECT <= 8 us
//      (tCEM, refresh), MIN_DESELECT >= 50 ns, PAGEBREAK 1024 (1 KiB pages);
//   5. M1 read 0xEB (quad, 6 dummy clocks) / write 0x38 (quad);
//   6. XIP_CTRL.WRITABLE_M1.
// Only SDK register definitions (hardware/structs/qmi.h, xip_ctrl.h) are used.
//
// On the Pico 2 W (no PSRAM) every function is a cheap no-op and init()
// reports "not fitted". A failed detection/self-test leaves the PSRAM pool
// unregistered: the firmware keeps running from SRAM only.

#include "psram_selftest.hpp"

#include <cstddef>
#include <cstdint>

namespace psram {

struct Info {
    bool fitted{};          // the board is expected to carry PSRAM
    bool detected{};        // Read ID returned KGD 0x5D
    bool enabled{};         // self-test passed and the pool is registered
    std::uint8_t manufacturer{}; // 0x0D = AP Memory
    std::uint8_t kgd{};
    std::uint8_t eid{};
    std::size_t sizeBytes{};
    std::uint32_t sysClockHz{};
    std::uint32_t clockDivider{};
    std::uint32_t sckHz{};
    std::uint32_t maxSelect{};
    std::uint32_t minDeselect{};
    std::uint32_t timingReapplies{};     // re-applied after clock changes/flash operations
    std::uint32_t configChecks{};        // verifyConfiguration() calls
    std::uint32_t configRepairs{};       // checks that found clobbered M1/XIP_CTRL state
    SelfTestResult bootSelfTest{};
    const char* error{};
};

// Detect, configure and test the PSRAM; on success register mem::Pool::PSRAM_BOARD.
bool init() noexcept;
const Info& info() noexcept;
bool available() noexcept;

// Recompute the M1 timing for the current clk_sys (call after changing it).
void reapplyTiming() noexcept;
// After any flash erase/program: verify that QMI M1 (timing/rfmt/rcmd/wfmt/wcmd)
// and XIP_CTRL.WRITABLE_M1 still hold our configuration; repair them if not.
// Returns true when the configuration was intact.
bool verifyConfiguration() noexcept;

// Non-destructive diagnostics: runs data-bus/random/cache tests inside a
// temporary buffer allocated from the PSRAM pool (address-bus test skipped).
SelfTestResult runDiagnostics(std::size_t bufferBytes, std::uint32_t seed) noexcept;

// Write back dirty XIP cache lines to the PSRAM (no-op without PSRAM).
void cleanCache() noexcept;

// Internal: SRAM-resident variant of verifyConfiguration() for use inside
// flash_safe_execute() (interrupts masked, other core locked out).
bool verifyConfigurationFromSafeZone();

} // namespace psram
