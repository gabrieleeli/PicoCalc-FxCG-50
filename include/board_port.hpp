#pragma once
#include "graphics_engine.hpp"
#include "storage.hpp"
#include <cstddef>
#include <cstdint>

namespace fx::net { class ChatTransport; }
namespace fx::files { class FileSystem; }

// Board support interface. Implemented by src/platform/pico/board_pico.cpp for
// the PicoCalc (RP2350) and by src/platform/host/board_host.cpp for host tests
// and the screenshot simulator.
namespace board {
void init();
gfx::Canvas& graphics() noexcept;
// Sends the 16x16 tiles that changed since the previous call to the LCD.
void present();
// Forces a full-frame transfer on the next present().
void invalidate();
// Non-blocking keyboard poll: returns true and the FIFO word (state<<8 | key).
bool pollKey(std::uint16_t& raw);
// Runs periodic hardware work (keyboard I2C state machine, battery, Wi-Fi).
void service();
// Sleeps until the next scheduler tick (a few milliseconds).
void idle();
std::uint32_t millis();
int batteryPercent();          // -1 when unknown
bool charging();
void setBacklight(std::uint8_t lcd, std::uint8_t keyboard);
std::size_t freeHeap();
// Internal flash regions (board_config.hpp): the workspace record store, the
// LittleFS partition and the 4 KB diagnostic sector. On the RP2350 every write
// goes through flash_safe_execute() with an SRAM bounce buffer and a check of
// the PSRAM (QMI M1) configuration.
storage::FlashDevice& storageFlash();
// Pico Plus 2 W: record store of a v1.8.0 (pico2_w) firmware that ran on the
// same module, read once to migrate the workspace; nullptr elsewhere.
storage::FlashDevice* legacyStorageFlash();
storage::FlashDevice* internalFlash();
storage::FlashDevice* diagnosticFlash();
struct FramebufferInfo { std::uint16_t* pixels; std::size_t bytes; bool inPsram; bool requestedPsram; };
FramebufferInfo framebufferInfo() noexcept;
std::uint32_t lastPresentUs() noexcept;      // duration of the last present() (0 on the host)
std::uint32_t benchmarkFullPresent();        // sends a full frame, returns its duration in us
struct FlashStats { std::uint32_t jobs; std::uint32_t failures; };
FlashStats flashStats() noexcept;            // flash_safe_execute() calls and failures
std::size_t firmwareBytes() noexcept;        // flash occupied by the firmware image (0 on the host)
fx::net::ChatTransport* chatTransport();   // nullptr when Wi-Fi is not available
// User files: microSD (FatFs) on the PicoCalc, RAM card on the host.
fx::files::FileSystem* fileSystem();
// LittleFS on the internal flash partition (both ports; nullptr if unusable).
fx::files::FileSystem* internalFileSystem();
const char* name();
// Statistics for the System app (bytes sent to the LCD by the last present()).
std::size_t lastFlushBytes();
} // namespace board
