#pragma once
#include "graphics_engine.hpp"
#include "storage.hpp"
#include <cstddef>
#include <cstdint>

namespace fx::net { class ChatTransport; }

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
storage::FlashDevice& storageFlash();
fx::net::ChatTransport* chatTransport();   // nullptr when Wi-Fi is not available
const char* name();
// Statistics for the System app (bytes sent to the LCD by the last present()).
std::size_t lastFlushBytes();
} // namespace board
