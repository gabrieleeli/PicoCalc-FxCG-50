#pragma once
// Board configuration: PicoCalc pin map + capabilities of the selected module.
//
// The PicoCalc v2 mainboard wiring is identical for both supported modules
// (Raspberry Pi Pico 2 W and Pimoroni Pico Plus 2 W are pin-compatible on the
// 40-pin header). Only module-internal resources differ (PSRAM, flash size,
// RP2350A vs RP2350B GPIO/ADC count). Every module-specific value is taken from
// the Pico SDK board header when building for the device — never hard-coded.
//
// Host builds (tests, desktop) do not include the SDK; they select a profile
// with -DFXAI_BOARD_PICO2W=1 or -DFXAI_BOARD_PICO_PLUS_2W=1 (default: Pico 2 W).

#include <cstddef>
#include <cstdint>

#if defined(PICO_ON_DEVICE) && PICO_ON_DEVICE
#include "pico.h"
#include "hardware/platform_defs.h"
#define FXAI_ON_DEVICE 1
#else
#define FXAI_ON_DEVICE 0
#endif

// ---------------------------------------------------------------------------
// Board identity and build-time feature flags (defaults for host builds).
#if !defined(FXAI_BOARD_PICO2W) && !defined(FXAI_BOARD_PICO_PLUS_2W)
#define FXAI_BOARD_PICO2W 1
#endif
#ifndef FXAI_BOARD_PICO2W
#define FXAI_BOARD_PICO2W 0
#endif
#ifndef FXAI_BOARD_PICO_PLUS_2W
#define FXAI_BOARD_PICO_PLUS_2W 0
#endif
#if FXAI_BOARD_PICO2W && FXAI_BOARD_PICO_PLUS_2W
#error "Select exactly one board: FXAI_BOARD_PICO2W or FXAI_BOARD_PICO_PLUS_2W"
#endif
#ifndef FXAI_HAS_PSRAM
#define FXAI_HAS_PSRAM (FXAI_BOARD_PICO_PLUS_2W ? 1 : 0)
#endif
#ifndef FXAI_FLASH_SIZE
#define FXAI_FLASH_SIZE (FXAI_BOARD_PICO_PLUS_2W ? (16u * 1024u * 1024u) : (4u * 1024u * 1024u))
#endif
#ifndef FXAI_BOARD_NAME
#define FXAI_BOARD_NAME (FXAI_BOARD_PICO_PLUS_2W ? "Pimoroni Pico Plus 2 W" : "Raspberry Pi Pico 2 W")
#endif
#ifndef FXAI_BOARD_ID
#define FXAI_BOARD_ID (FXAI_BOARD_PICO_PLUS_2W ? "pico_plus_2w" : "pico2w")
#endif
#ifndef FXAI_VERSION
#define FXAI_VERSION "host"
#endif
#ifndef FXAI_ENABLE_3D
#define FXAI_ENABLE_3D 1
#endif
#ifndef FXAI_ENABLE_CAS
#define FXAI_ENABLE_CAS FXAI_BOARD_PICO_PLUS_2W
#endif
#ifndef FXAI_ENABLE_BT
#define FXAI_ENABLE_BT 0
#endif
#ifndef FXAI_ENABLE_SD
#define FXAI_ENABLE_SD 1
#endif
#ifndef FXAI_FRAMEBUFFER_IN_PSRAM
#define FXAI_FRAMEBUFFER_IN_PSRAM 0
#endif
#ifndef FXAI_PSRAM_NEW_THRESHOLD
#define FXAI_PSRAM_NEW_THRESHOLD 512
#endif
#ifndef FXAI_APP_STACK_BYTES
#define FXAI_APP_STACK_BYTES 32768
#endif
#ifndef FXAI_CORE1_STACK_BYTES
#define FXAI_CORE1_STACK_BYTES 8192
#endif
#ifndef FXAI_FIRMWARE_PARTITION_BYTES
#define FXAI_FIRMWARE_PARTITION_BYTES (FXAI_BOARD_PICO_PLUS_2W ? (4u * 1024u * 1024u) : (2u * 1024u * 1024u))
#endif
#ifndef FXAI_FLASH_DIAG_OFFSET
#define FXAI_FLASH_DIAG_OFFSET FXAI_FIRMWARE_PARTITION_BYTES
#endif
#ifndef FXAI_FLASH_FS_OFFSET
#define FXAI_FLASH_FS_OFFSET (FXAI_FIRMWARE_PARTITION_BYTES + 4096u)
#endif
#ifndef FXAI_FLASH_FS_SIZE
#define FXAI_FLASH_FS_SIZE (FXAI_FLASH_SIZE - FXAI_FLASH_FS_OFFSET - 8192u)
#endif

namespace board::config {

// --- Identity ---------------------------------------------------------------
inline constexpr const char* kBoardName = FXAI_BOARD_NAME;
inline constexpr const char* kBoardId = FXAI_BOARD_ID;
inline constexpr const char* kFirmwareVersion = FXAI_VERSION;
inline constexpr bool kIsPicoPlus2W = FXAI_BOARD_PICO_PLUS_2W != 0;
inline constexpr bool kHasPsram = FXAI_HAS_PSRAM != 0;

// --- PicoCalc v2.0 mainboard wiring (unchanged for both modules) -------------
// Sources: ClockworkPi PicoCalc repository (Code/MP3Player/config.h,
// Code/pico_multi_booter/boot.c, keyboard firmware) and the v2.0 schematic.
// LCD: ST7365P 320x320, SPI1 pins driven by a PIO SPI transmitter.
inline constexpr unsigned kLcdSck = 10, kLcdMosi = 11, kLcdMiso = 12;
inline constexpr unsigned kLcdCs = 13, kLcdDc = 14, kLcdReset = 15;
inline constexpr unsigned kLcdWidth = 320, kLcdHeight = 320;
// Keyboard/battery STM32 controller on I2C1. Battery level is read through the
// controller (REG_ID_BAT), so the firmware never needs an RP2350 ADC pin.
inline constexpr unsigned kKeyboardSda = 6, kKeyboardScl = 7;
inline constexpr std::uint8_t kKeyboardAddress = 0x1F;
inline constexpr unsigned kKeyboardI2cHz = 10'000;
// micro-SD socket on SPI0; card-detect is active-low with pull-up.
inline constexpr unsigned kSdSck = 18, kSdMosi = 19, kSdMiso = 16, kSdCs = 17, kSdDetect = 22;
// UART0 is routed to the PicoCalc USB-C port (not used: stdio goes to USB CDC).
inline constexpr unsigned kUartTx = 0, kUartRx = 1;
// Audio PWM pins are NOT used by this firmware: ClockworkPi sources disagree
// (pwm_sound.h: GP26/GP27, MP3Player/config.h: GP28/GP27). Verify on the
// schematic before writing an audio driver.

// --- Module resources --------------------------------------------------------
inline constexpr std::size_t kSramBytes = 520u * 1024u;
inline constexpr std::uintptr_t kSramBase = 0x20000000u;
inline constexpr std::uintptr_t kFlashXipBase = 0x10000000u;     // cached XIP, CS0
inline constexpr std::uintptr_t kPsramCachedBase = 0x11000000u;  // cached XIP, CS1
inline constexpr std::uintptr_t kPsramUncachedBase = 0x15000000u;// XIP_NOCACHE_NOALLOC + 16 MiB
inline constexpr std::size_t kPsramWindowBytes = 16u * 1024u * 1024u; // CS1 address window
inline constexpr std::size_t kPsramExpectedBytes = kHasPsram ? 8u * 1024u * 1024u : 0u;
inline constexpr std::size_t kFlashSize = FXAI_FLASH_SIZE;
inline constexpr std::size_t kFirmwarePartitionBytes = FXAI_FIRMWARE_PARTITION_BYTES;
inline constexpr std::size_t kFlashDiagOffset = FXAI_FLASH_DIAG_OFFSET;
inline constexpr std::size_t kFlashFsOffset = FXAI_FLASH_FS_OFFSET;
inline constexpr std::size_t kFlashFsSize = FXAI_FLASH_FS_SIZE;
inline constexpr std::size_t kFlashSectorBytes = 4096u;
inline constexpr std::size_t kAppStackBytes = FXAI_APP_STACK_BYTES;
inline constexpr std::size_t kCore1StackBytes = FXAI_CORE1_STACK_BYTES;

static_assert(kFlashFsOffset % kFlashSectorBytes == 0, "LittleFS must start on a sector boundary");
static_assert(kFlashFsSize % kFlashSectorBytes == 0, "LittleFS size must be a multiple of 4 KiB");
static_assert(kFlashFsOffset + kFlashFsSize <= kFlashSize, "LittleFS exceeds the flash");
static_assert(kAppStackBytes >= 8192 && kAppStackBytes % 8 == 0, "application stack must be >= 8 KiB, 8-byte multiple");

#if FXAI_ON_DEVICE
static_assert(PICO_FLASH_SIZE_BYTES == FXAI_FLASH_SIZE, "FXAI_FLASH_SIZE must match the SDK board header");
inline constexpr unsigned kGpioCount = NUM_BANK0_GPIOS;    // 30 on RP2350A, 48 on RP2350B
inline constexpr unsigned kAdcBasePin = ADC_BASE_PIN;      // 26 on RP2350A, 40 on RP2350B
inline constexpr unsigned kAdcChannels = NUM_ADC_CHANNELS; // 5 / 9 (incl. temperature sensor)
inline constexpr unsigned kVsysPin = PICO_VSYS_PIN;        // 29 (Pico 2 W) / 43 (Pico Plus 2 W)
inline constexpr unsigned kCyw43WlOn = CYW43_DEFAULT_PIN_WL_REG_ON;
inline constexpr unsigned kCyw43Data = CYW43_DEFAULT_PIN_WL_DATA_OUT;
inline constexpr unsigned kCyw43Cs = CYW43_DEFAULT_PIN_WL_CS;
inline constexpr unsigned kCyw43Clock = CYW43_DEFAULT_PIN_WL_CLOCK;
#if FXAI_BOARD_PICO_PLUS_2W
#if !defined(PIMORONI_PICO_PLUS2_W_PSRAM_CS_PIN)
#error "SDK board header pimoroni_pico_plus2_w_rp2350.h did not provide PIMORONI_PICO_PLUS2_W_PSRAM_CS_PIN"
#endif
inline constexpr int kPsramCsPin = PIMORONI_PICO_PLUS2_W_PSRAM_CS_PIN; // GPIO47 = XIP_CS1
static_assert(PICO_RP2350A == 0, "Pico Plus 2 W uses the RP2350B (QFN-80) package");
#else
inline constexpr int kPsramCsPin = -1;
static_assert(PICO_RP2350A == 1, "Pico 2 W uses the RP2350A (QFN-60) package");
#endif
#else
// Host profile mirrors the SDK headers of the selected board.
inline constexpr unsigned kGpioCount = kIsPicoPlus2W ? 48u : 30u;
inline constexpr unsigned kAdcBasePin = kIsPicoPlus2W ? 40u : 26u;
inline constexpr unsigned kAdcChannels = kIsPicoPlus2W ? 9u : 5u;
inline constexpr unsigned kVsysPin = kIsPicoPlus2W ? 43u : 29u;
inline constexpr unsigned kCyw43WlOn = 23u, kCyw43Data = 24u, kCyw43Cs = 25u, kCyw43Clock = 29u;
inline constexpr int kPsramCsPin = kIsPicoPlus2W ? 47 : -1;
#endif

// The PicoCalc header pins must never collide with module-internal pins.
constexpr bool pinIsModuleInternal(unsigned pin) noexcept {
    return pin == kCyw43WlOn || pin == kCyw43Data || pin == kCyw43Cs || pin == kCyw43Clock ||
           (kPsramCsPin >= 0 && pin == static_cast<unsigned>(kPsramCsPin));
}
static_assert(!pinIsModuleInternal(kLcdSck) && !pinIsModuleInternal(kLcdMosi) && !pinIsModuleInternal(kLcdCs) &&
              !pinIsModuleInternal(kLcdDc) && !pinIsModuleInternal(kLcdReset), "LCD pin collides with a module pin");
static_assert(!pinIsModuleInternal(kKeyboardSda) && !pinIsModuleInternal(kKeyboardScl), "I2C pin collides");
static_assert(!pinIsModuleInternal(kSdSck) && !pinIsModuleInternal(kSdMosi) && !pinIsModuleInternal(kSdMiso) &&
              !pinIsModuleInternal(kSdCs) && !pinIsModuleInternal(kSdDetect), "SD pin collides");

// True when the GPIO can be used as an ADC input on this module. On the
// RP2350B (Pico Plus 2 W) GP26-GP29 are digital-only; the ADC is on GPIO40-47.
constexpr bool gpioHasAdc(unsigned pin) noexcept {
    return pin >= kAdcBasePin && pin < kAdcBasePin + (kAdcChannels - 1u);
}

} // namespace board::config
