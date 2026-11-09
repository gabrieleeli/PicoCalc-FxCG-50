#pragma once
#include <cstddef>
#include <cstdint>
#include "graphics_engine.hpp"
namespace board {
void init();
gfx::Canvas& graphics() noexcept;
void clear(uint16_t rgb565);
void text(int x,int y,const char* s,uint16_t rgb565);
void rect(int x,int y,int w,int h,uint16_t rgb565,bool filled=false);
void pixel(int x,int y,uint16_t rgb565);
void present();
int readKey();
bool wifiConnect(const char* ssid,const char* password);

// --- Additions (v1.9.0) -------------------------------------------------------
struct FramebufferInfo {
    std::uint16_t* pixels{};
    std::size_t bytes{};
    bool inPsram{};     // placement actually obtained (FXAI_FRAMEBUFFER_IN_PSRAM may fall back)
    bool requestedPsram{};
};
FramebufferInfo framebuffer() noexcept;
// Duration of the last present() (microseconds) and of the last full-frame flush.
std::uint32_t lastPresentUs() noexcept;
std::uint32_t lastFullFrameUs() noexcept;
// Forces a full-frame transfer and returns its duration in microseconds.
std::uint32_t benchmarkFullPresent();
// Full-frame transfer of an arbitrary 320x320 RGB565 buffer (SRAM or PSRAM).
std::uint32_t benchmarkPresentFrom(const std::uint16_t* pixels);
// Wi-Fi state for diagnostics (string owned by the board layer).
const char* wifiStatus() noexcept;
bool wifiReady() noexcept;
// Changes clk_sys and re-derives every clock-dependent setting: PSRAM QMI M1
// timing (psram::reapplyTiming) and the LCD PIO SPI divider. Must be called
// while core 1 is idle. Returns false if the frequency is not achievable.
bool setSystemClockKhz(std::uint32_t khz);
// Powers the radio up in STA mode (firmware download over the CYW43 SPI bus)
// and counts scan results; -1 on failure.
int wifiScan(unsigned timeoutMs);
}
