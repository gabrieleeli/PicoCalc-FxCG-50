// APS6404L PSRAM driver (QMI window 1 / XIP_CS1) for the Pimoroni Pico Plus 2 W.
//
// Port of the initialisation sequence of SparkFun Electronics' sfe_psram.c
// (https://github.com/sparkfun/sparkfun-pico, sparkfun_pico/sfe_psram.c,
// MIT License, Copyright (c) 2024 SparkFun Electronics). Differences:
//   * the CS pin comes from the SDK board header (PIMORONI_PICO_PLUS2_W_PSRAM_CS_PIN);
//   * the manufacturer ID byte is kept for diagnostics;
//   * the expected M1 register values are remembered so that the configuration
//     can be verified/repaired after flash operations and clock changes;
//   * a boot self-test gates the registration of the memory pool.
// Only SDK register definitions are used (hardware/structs/qmi.h, xip_ctrl.h).

#include "psram.hpp"

#include "board_config.hpp"
#include "mem_pools.hpp"

#include <cstdio>

#if FXAI_ON_DEVICE
#include "hardware/sync.h"
#include "hardware/xip_cache.h"
#include "pico/time.h"
#endif
#if FXAI_ON_DEVICE && FXAI_HAS_PSRAM
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/structs/qmi.h"
#include "hardware/structs/xip_ctrl.h"
#endif

namespace psram {
namespace {
Info g_info{};
} // namespace

#if FXAI_ON_DEVICE
const Info& info() noexcept { return g_info; }
bool available() noexcept { return g_info.enabled; }
#else
// Host builds: there is no PSRAM driver, but the tests and the simulator may
// register a simulated PSRAM_BOARD pool (mem::testing::simulatePsram); the
// state reported here follows that pool so that SYSTEM shows the same data.
const Info& info() noexcept {
    const bool pool = mem::available(mem::Pool::PSRAM_BOARD);
    g_info.fitted = board::config::kHasPsram;
    g_info.detected = g_info.enabled = pool;
    g_info.sizeBytes = pool ? mem::stats(mem::Pool::PSRAM_BOARD).total : 0;
    g_info.bootSelfTest.passed = pool;
    g_info.error = pool ? nullptr : (board::config::kHasPsram ? "build host: PSRAM non simulata" : "PSRAM non presente su questa scheda");
    return g_info;
}
bool available() noexcept { return mem::available(mem::Pool::PSRAM_BOARD); }
#endif

#if FXAI_ON_DEVICE && FXAI_HAS_PSRAM

namespace {
constexpr std::uint8_t kCmdQuadEnd = 0xF5;
constexpr std::uint8_t kCmdQuadEnable = 0x35;
constexpr std::uint8_t kCmdReadId = 0x9F;
constexpr std::uint8_t kCmdResetEnable = 0x66;
constexpr std::uint8_t kCmdReset = 0x99;
constexpr std::uint8_t kCmdQuadRead = 0xEB;
constexpr std::uint8_t kCmdQuadWrite = 0x38;
constexpr std::uint8_t kCmdNoop = 0xFF;
constexpr std::uint8_t kKgdPass = 0x5D;

// Timing constants from sfe_psram.c (APS6404L datasheet values at 3.3 V).
constexpr std::uint64_t kSecondToFs = 1000000000000000ull;
constexpr std::uint32_t kMaxSelectFs64 = 125000000u; // tCEM 8 us, MAX_SELECT counts 64 cycles
constexpr std::uint32_t kMinDeselectFs = 50000000u;  // tCPH 50 ns
constexpr std::uint32_t kMaxSckHz = 109000000u;

struct Expected {
    std::uint32_t timing, rfmt, rcmd, wfmt, wcmd;
};
Expected g_expected{};

struct IdBytes {
    std::uint8_t manufacturer, kgd, eid;
};

// Runs from SRAM with interrupts masked: while QMI direct mode is enabled no
// XIP (flash or PSRAM) access may be attempted, so this function must not call
// anything that lives in flash and must not touch constant tables in flash.
IdBytes __no_inline_not_in_flash_func(readIdDirect)() {
    IdBytes id{0, 0, 0};
    const std::uint32_t irq = save_and_disable_interrupts();
    qmi_hw->direct_csr = 30u << QMI_DIRECT_CSR_CLKDIV_LSB | QMI_DIRECT_CSR_EN_BITS;
    // Wait for the cooldown of the last XIP transfer before the first direct op.
    while ((qmi_hw->direct_csr & QMI_DIRECT_CSR_BUSY_BITS) != 0) {}

    // Exit QPI mode in case a previous boot (soft reset) left the chip in it.
    qmi_hw->direct_csr |= QMI_DIRECT_CSR_ASSERT_CS1N_BITS;
    qmi_hw->direct_tx = QMI_DIRECT_TX_OE_BITS | QMI_DIRECT_TX_IWIDTH_VALUE_Q << QMI_DIRECT_TX_IWIDTH_LSB | kCmdQuadEnd;
    while ((qmi_hw->direct_csr & QMI_DIRECT_CSR_BUSY_BITS) != 0) {}
    (void)qmi_hw->direct_rx;
    qmi_hw->direct_csr &= ~QMI_DIRECT_CSR_ASSERT_CS1N_BITS;

    // Read ID (SPI): command, 3 address bytes, MF ID, KGD, EID.
    qmi_hw->direct_csr |= QMI_DIRECT_CSR_ASSERT_CS1N_BITS;
    for (int i = 0; i < 7; ++i) {
        qmi_hw->direct_tx = (i == 0) ? kCmdReadId : kCmdNoop;
        while ((qmi_hw->direct_csr & QMI_DIRECT_CSR_TXEMPTY_BITS) == 0) {}
        while ((qmi_hw->direct_csr & QMI_DIRECT_CSR_BUSY_BITS) != 0) {}
        const auto value = static_cast<std::uint8_t>(qmi_hw->direct_rx);
        if (i == 4) id.manufacturer = value;
        else if (i == 5) id.kgd = value;
        else if (i == 6) id.eid = value;
    }
    qmi_hw->direct_csr &= ~(QMI_DIRECT_CSR_ASSERT_CS1N_BITS | QMI_DIRECT_CSR_EN_BITS);
    restore_interrupts(irq);
    return id;
}

// Reset Enable, Reset, Enter Quad mode — each in its own CS cycle.
void __no_inline_not_in_flash_func(resetAndEnterQpi)() {
    const std::uint32_t irq = save_and_disable_interrupts();
    qmi_hw->direct_csr = 30u << QMI_DIRECT_CSR_CLKDIV_LSB | QMI_DIRECT_CSR_EN_BITS;
    while ((qmi_hw->direct_csr & QMI_DIRECT_CSR_BUSY_BITS) != 0) {}
    for (int i = 0; i < 3; ++i) {
        qmi_hw->direct_csr |= QMI_DIRECT_CSR_ASSERT_CS1N_BITS;
        qmi_hw->direct_tx = (i == 0) ? kCmdResetEnable : ((i == 1) ? kCmdReset : kCmdQuadEnable);
        while ((qmi_hw->direct_csr & QMI_DIRECT_CSR_BUSY_BITS) != 0) {}
        qmi_hw->direct_csr &= ~QMI_DIRECT_CSR_ASSERT_CS1N_BITS;
        for (int j = 0; j < 20; ++j) __asm volatile("nop"); // >= tCPH / tRST
        (void)qmi_hw->direct_rx;
    }
    qmi_hw->direct_csr &= ~(QMI_DIRECT_CSR_ASSERT_CS1N_BITS | QMI_DIRECT_CSR_EN_BITS);
    restore_interrupts(irq);
}

struct TimingValues {
    std::uint32_t reg, divider, maxSelect, minDeselect;
};

TimingValues computeTiming(std::uint32_t sysHz) {
    std::uint32_t divider = (sysHz + kMaxSckHz - 1) / kMaxSckHz;
    if (divider < 1) divider = 1;
    if (divider > 255) divider = 255;
    const auto fsPerCycle = static_cast<std::uint32_t>(kSecondToFs / sysHz);
    std::uint32_t maxSelect = kMaxSelectFs64 / fsPerCycle;
    if (maxSelect > 63) maxSelect = 63;
    std::uint32_t minDeselect = (kMinDeselectFs + fsPerCycle - 1) / fsPerCycle;
    if (minDeselect > 31) minDeselect = 31;
    const std::uint32_t reg = QMI_M1_TIMING_PAGEBREAK_VALUE_1024 << QMI_M1_TIMING_PAGEBREAK_LSB |
                              3u << QMI_M1_TIMING_SELECT_HOLD_LSB | 1u << QMI_M1_TIMING_COOLDOWN_LSB |
                              1u << QMI_M1_TIMING_RXDELAY_LSB | maxSelect << QMI_M1_TIMING_MAX_SELECT_LSB |
                              minDeselect << QMI_M1_TIMING_MIN_DESELECT_LSB | divider << QMI_M1_TIMING_CLKDIV_LSB;
    return {reg, divider, maxSelect, minDeselect};
}

constexpr std::uint32_t kReadFormat =
    QMI_M1_RFMT_PREFIX_WIDTH_VALUE_Q << QMI_M1_RFMT_PREFIX_WIDTH_LSB |
    QMI_M1_RFMT_ADDR_WIDTH_VALUE_Q << QMI_M1_RFMT_ADDR_WIDTH_LSB |
    QMI_M1_RFMT_SUFFIX_WIDTH_VALUE_Q << QMI_M1_RFMT_SUFFIX_WIDTH_LSB |
    QMI_M1_RFMT_DUMMY_WIDTH_VALUE_Q << QMI_M1_RFMT_DUMMY_WIDTH_LSB |
    QMI_M1_RFMT_DUMMY_LEN_VALUE_24 << QMI_M1_RFMT_DUMMY_LEN_LSB |
    QMI_M1_RFMT_DATA_WIDTH_VALUE_Q << QMI_M1_RFMT_DATA_WIDTH_LSB |
    QMI_M1_RFMT_PREFIX_LEN_VALUE_8 << QMI_M1_RFMT_PREFIX_LEN_LSB |
    QMI_M1_RFMT_SUFFIX_LEN_VALUE_NONE << QMI_M1_RFMT_SUFFIX_LEN_LSB;
constexpr std::uint32_t kReadCommand = kCmdQuadRead << QMI_M1_RCMD_PREFIX_LSB | 0u << QMI_M1_RCMD_SUFFIX_LSB;
constexpr std::uint32_t kWriteFormat =
    QMI_M1_WFMT_PREFIX_WIDTH_VALUE_Q << QMI_M1_WFMT_PREFIX_WIDTH_LSB |
    QMI_M1_WFMT_ADDR_WIDTH_VALUE_Q << QMI_M1_WFMT_ADDR_WIDTH_LSB |
    QMI_M1_WFMT_SUFFIX_WIDTH_VALUE_Q << QMI_M1_WFMT_SUFFIX_WIDTH_LSB |
    QMI_M1_WFMT_DUMMY_WIDTH_VALUE_Q << QMI_M1_WFMT_DUMMY_WIDTH_LSB |
    QMI_M1_WFMT_DUMMY_LEN_VALUE_NONE << QMI_M1_WFMT_DUMMY_LEN_LSB |
    QMI_M1_WFMT_DATA_WIDTH_VALUE_Q << QMI_M1_WFMT_DATA_WIDTH_LSB |
    QMI_M1_WFMT_PREFIX_LEN_VALUE_8 << QMI_M1_WFMT_PREFIX_LEN_LSB |
    QMI_M1_WFMT_SUFFIX_LEN_VALUE_NONE << QMI_M1_WFMT_SUFFIX_LEN_LSB;
constexpr std::uint32_t kWriteCommand = kCmdQuadWrite << QMI_M1_WCMD_PREFIX_LSB | 0u << QMI_M1_WCMD_SUFFIX_LSB;

// Writes the whole M1 window configuration (SRAM-resident, IRQs masked).
void __no_inline_not_in_flash_func(applyM1)(std::uint32_t timing, bool writable) {
    const std::uint32_t irq = save_and_disable_interrupts();
    qmi_hw->m[1].timing = timing;
    qmi_hw->m[1].rfmt = kReadFormat;
    qmi_hw->m[1].rcmd = kReadCommand;
    qmi_hw->m[1].wfmt = kWriteFormat;
    qmi_hw->m[1].wcmd = kWriteCommand;
    if (writable) xip_ctrl_hw->ctrl |= XIP_CTRL_WRITABLE_M1_BITS;
    else xip_ctrl_hw->ctrl &= ~XIP_CTRL_WRITABLE_M1_BITS;
    restore_interrupts(irq);
}

// Callable from the flash safe zone (SRAM only, no flash calls, no printf).
bool __no_inline_not_in_flash_func(checkAndRepairM1)() {
    const bool intact = qmi_hw->m[1].timing == g_expected.timing && qmi_hw->m[1].rfmt == g_expected.rfmt &&
                        qmi_hw->m[1].rcmd == g_expected.rcmd && qmi_hw->m[1].wfmt == g_expected.wfmt &&
                        qmi_hw->m[1].wcmd == g_expected.wcmd &&
                        (xip_ctrl_hw->ctrl & XIP_CTRL_WRITABLE_M1_BITS) != 0;
    if (!intact) {
        qmi_hw->m[1].timing = g_expected.timing;
        qmi_hw->m[1].rfmt = g_expected.rfmt;
        qmi_hw->m[1].rcmd = g_expected.rcmd;
        qmi_hw->m[1].wfmt = g_expected.wfmt;
        qmi_hw->m[1].wcmd = g_expected.wcmd;
        xip_ctrl_hw->ctrl |= XIP_CTRL_WRITABLE_M1_BITS;
    }
    return intact;
}

// Self-test accessor over the cached (0x11...) and uncached (0x15...) aliases.
struct DeviceWindow {
    volatile std::uint32_t* cached;
    volatile std::uint32_t* uncached;
    std::size_t count;
    std::size_t words() const noexcept { return count; }
    void writeUncached(std::size_t i, std::uint32_t v) noexcept { uncached[i] = v; }
    std::uint32_t readUncached(std::size_t i) noexcept { return uncached[i]; }
    void writeCached(std::size_t i, std::uint32_t v) noexcept { cached[i] = v; }
    std::uint32_t readCached(std::size_t i) noexcept { return cached[i]; }
    void cleanCache() noexcept { xip_cache_clean_all(); }
    void invalidateWords(std::size_t i, std::size_t n) noexcept {
        const auto start = reinterpret_cast<std::uintptr_t>(cached + i) - XIP_BASE;
        const auto end = reinterpret_cast<std::uintptr_t>(cached + i + n) - XIP_BASE;
        const auto lineStart = start & ~static_cast<std::uintptr_t>(XIP_CACHE_LINE_SIZE - 1);
        const auto lineEnd = (end + XIP_CACHE_LINE_SIZE - 1) & ~static_cast<std::uintptr_t>(XIP_CACHE_LINE_SIZE - 1);
        xip_cache_invalidate_range(lineStart, lineEnd - lineStart);
    }
};

std::size_t sizeFromEid(std::uint8_t eid) {
    // sfe_psram.c: density in EID[7:5]; 0x26 is the APS6404L 64 Mbit code.
    const std::uint8_t sizeId = static_cast<std::uint8_t>(eid >> 5);
    if (eid == 0x26 || sizeId == 2) return 8u * 1024u * 1024u;
    if (sizeId == 1) return 4u * 1024u * 1024u;
    if (sizeId == 0) return 2u * 1024u * 1024u;
    return 0;
}
} // namespace

bool init() noexcept {
    g_info = Info{};
    g_info.fitted = true;
    constexpr unsigned csPin = static_cast<unsigned>(board::config::kPsramCsPin);
    gpio_set_function(csPin, GPIO_FUNC_XIP_CS1);

    const IdBytes id = readIdDirect();
    g_info.manufacturer = id.manufacturer;
    g_info.kgd = id.kgd;
    g_info.eid = id.eid;
    if (id.kgd != kKgdPass) {
        g_info.error = "PSRAM non rilevata (KGD diverso da 0x5D)";
        std::printf("[psram] not detected: MF=0x%02X KGD=0x%02X EID=0x%02X\n", id.manufacturer, id.kgd, id.eid);
        return false;
    }
    g_info.detected = true;
    g_info.sizeBytes = sizeFromEid(id.eid);
    if (g_info.sizeBytes == 0 || g_info.sizeBytes > board::config::kPsramWindowBytes) {
        g_info.error = "PSRAM: densita sconosciuta";
        std::printf("[psram] unknown density EID=0x%02X\n", id.eid);
        return false;
    }

    resetAndEnterQpi();
    g_info.sysClockHz = clock_get_hz(clk_sys);
    const TimingValues t = computeTiming(g_info.sysClockHz);
    g_expected = Expected{t.reg, kReadFormat, kReadCommand, kWriteFormat, kWriteCommand};
    g_info.clockDivider = t.divider;
    g_info.sckHz = g_info.sysClockHz / t.divider;
    g_info.maxSelect = t.maxSelect;
    g_info.minDeselect = t.minDeselect;
    applyM1(t.reg, true);

    // Destructive boot self-test over the whole chip (nothing uses it yet).
    DeviceWindow window{reinterpret_cast<volatile std::uint32_t*>(board::config::kPsramCachedBase),
                        reinterpret_cast<volatile std::uint32_t*>(board::config::kPsramUncachedBase),
                        g_info.sizeBytes / sizeof(std::uint32_t)};
    SelfTestOptions options;
    options.addressBus = true;
    options.samples = 8192;
    options.seed = static_cast<std::uint32_t>(time_us_32()) | 1u;
    const std::uint32_t start = time_us_32();
    g_info.bootSelfTest = runPatternTests(window, options);
    g_info.bootSelfTest.elapsedUs = time_us_32() - start;
    if (!g_info.bootSelfTest.passed) {
        applyM1(t.reg, false); // keep the window read-only: nothing may use it
        g_info.error = "PSRAM self-test fallito: avvio solo in SRAM";
        std::printf("[psram] self-test FAILED at %s, word %u: expected 0x%08lX got 0x%08lX\n",
                    g_info.bootSelfTest.failedStep ? g_info.bootSelfTest.failedStep : "?",
                    static_cast<unsigned>(g_info.bootSelfTest.failWordIndex),
                    static_cast<unsigned long>(g_info.bootSelfTest.expected),
                    static_cast<unsigned long>(g_info.bootSelfTest.actual));
        return false;
    }
    if (!mem::registerPsramBoard(reinterpret_cast<void*>(board::config::kPsramCachedBase), g_info.sizeBytes)) {
        g_info.error = "PSRAM: registrazione del pool fallita";
        return false;
    }
    g_info.enabled = true;
    std::printf("[psram] APS6404L MF=0x%02X KGD=0x%02X EID=0x%02X, %u KiB, SCK %lu Hz (div %lu), self-test %lu us\n",
                id.manufacturer, id.kgd, id.eid, static_cast<unsigned>(g_info.sizeBytes / 1024u),
                static_cast<unsigned long>(g_info.sckHz), static_cast<unsigned long>(t.divider),
                static_cast<unsigned long>(g_info.bootSelfTest.elapsedUs));
    return true;
}

void reapplyTiming() noexcept {
    if (!g_info.detected) return;
    g_info.sysClockHz = clock_get_hz(clk_sys);
    const TimingValues t = computeTiming(g_info.sysClockHz);
    g_expected.timing = t.reg;
    g_info.clockDivider = t.divider;
    g_info.sckHz = g_info.sysClockHz / t.divider;
    g_info.maxSelect = t.maxSelect;
    g_info.minDeselect = t.minDeselect;
    applyM1(t.reg, g_info.enabled);
    ++g_info.timingReapplies;
}

bool verifyConfiguration() noexcept {
    if (!g_info.enabled) return true;
    const std::uint32_t irq = save_and_disable_interrupts();
    const bool intact = checkAndRepairM1();
    restore_interrupts(irq);
    ++g_info.configChecks;
    if (!intact) {
        ++g_info.configRepairs;
        ++g_info.timingReapplies;
    }
    return intact;
}

void cleanCache() noexcept {
    if (g_info.enabled) xip_cache_clean_all();
}

SelfTestResult runDiagnostics(std::size_t bufferBytes, std::uint32_t seed) noexcept {
    SelfTestResult r;
    if (!g_info.enabled) { r.failedStep = "PSRAM non disponibile"; return r; }
    bufferBytes &= ~static_cast<std::size_t>(63);
    if (bufferBytes < 64) bufferBytes = 64;
    void* block = mem::tryAlloc(mem::Pool::PSRAM_BOARD, bufferBytes, 64);
    if (!block) { r.failedStep = "memoria PSRAM insufficiente per il test"; return r; }
    const auto cached = reinterpret_cast<std::uintptr_t>(block);
    const auto uncached = cached - board::config::kPsramCachedBase + board::config::kPsramUncachedBase;
    DeviceWindow window{reinterpret_cast<volatile std::uint32_t*>(cached),
                        reinterpret_cast<volatile std::uint32_t*>(uncached), bufferBytes / sizeof(std::uint32_t)};
    SelfTestOptions options;
    options.addressBus = false; // the rest of the chip holds live allocations
    options.samples = static_cast<std::uint32_t>(window.count < 16384 ? window.count : 16384);
    options.seed = seed | 1u;
    const std::uint32_t start = time_us_32();
    r = runPatternTests(window, options);
    r.elapsedUs = time_us_32() - start;
    mem::free(block);
    return r;
}

// Exposed for flash_fs.cpp: runs inside the flash safe zone.
bool __no_inline_not_in_flash_func(verifyConfigurationFromSafeZone)() {
    if (!g_info.enabled) return true;
    const bool intact = checkAndRepairM1();
    ++g_info.configChecks;
    if (!intact) {
        ++g_info.configRepairs;
        ++g_info.timingReapplies;
    }
    return intact;
}

#else // ---------------------------------------------------------------- no PSRAM

bool init() noexcept {
    g_info = Info{};
#if FXAI_ON_DEVICE
    g_info.error = "PSRAM non presente su questa scheda";
    return false;
#else
    return available();   // simulated pool (tests, simulator)
#endif
}
void reapplyTiming() noexcept {}
bool verifyConfiguration() noexcept { return true; }
void cleanCache() noexcept {}
SelfTestResult runDiagnostics(std::size_t bufferBytes, std::uint32_t seed) noexcept {
    SelfTestResult r;
#if FXAI_ON_DEVICE
    (void)bufferBytes; (void)seed;
    r.failedStep = "PSRAM non disponibile";
#else
    // Host: the same pattern tests on a block of the simulated pool (no cache:
    // the cached and uncached accessors are the same memory).
    if (!available()) { r.failedStep = "PSRAM non disponibile"; return r; }
    bufferBytes &= ~static_cast<std::size_t>(63);
    if (bufferBytes < 64) bufferBytes = 64;
    void* block = mem::tryAlloc(mem::Pool::PSRAM_BOARD, bufferBytes, 64);
    if (!block) { r.failedStep = "memoria PSRAM insufficiente per il test"; return r; }
    struct HostWindow {
        std::uint32_t* p;
        std::size_t count;
        std::size_t words() const noexcept { return count; }
        void writeUncached(std::size_t i, std::uint32_t v) noexcept { p[i] = v; }
        std::uint32_t readUncached(std::size_t i) noexcept { return p[i]; }
        void writeCached(std::size_t i, std::uint32_t v) noexcept { p[i] = v; }
        std::uint32_t readCached(std::size_t i) noexcept { return p[i]; }
        void cleanCache() noexcept {}
        void invalidateWords(std::size_t, std::size_t) noexcept {}
    } window{static_cast<std::uint32_t*>(block), bufferBytes / sizeof(std::uint32_t)};
    SelfTestOptions options;
    options.addressBus = false;
    options.samples = static_cast<std::uint32_t>(window.count < 16384 ? window.count : 16384);
    options.seed = seed | 1u;
    r = runPatternTests(window, options);
    mem::free(block);
#endif
    return r;
}
#if FXAI_ON_DEVICE
// SRAM-resident even without PSRAM: it is called from inside flash_safe_execute().
bool __no_inline_not_in_flash_func(verifyConfigurationFromSafeZone)() { return true; }
#else
bool verifyConfigurationFromSafeZone() { return true; }
#endif

#endif

} // namespace psram
