// RP2350 entry point: board bring-up, then the portable Firmware loop.
#include "board_platform.hpp"
#include "board_port.hpp"
#include "firmware.hpp"
#include "ui_kit.hpp"
#include "pico/stdlib.h"
#include <cstdio>

#ifndef FXAI_VERSION
#define FXAI_VERSION "dev"
#endif

namespace {
#if FXAI_DISPLAY_CALIBRATION_SCREEN
// Hardware bring-up pattern: verifies colour order, orientation and offsets.
void calibrationScreen() {
    auto& c = board::graphics();
    c.clear(0x0000);
    const std::uint16_t bars[8] = {0xF800, 0x07E0, 0x001F, 0xFFFF, 0xFFE0, 0xF81F, 0x07FF, 0x7BEF};
    for (int i = 0; i < 8; ++i) c.rect(i * 40, 30, 40, 60, bars[i], true);
    ui::text(c, 4, 4, "LCD TEST 320x320  R G B W Y M C Grey", 0xFFFF, font::Face::Small);
    c.rect(0, 0, 320, 320, 0xFFFF, false);
    c.rect(0, 100, 20, 20, 0xF800, true);   ui::text(c, 24, 102, "TOP-LEFT = RED", 0xFFFF);
    c.rect(300, 300, 20, 20, 0x001F, true); ui::text(c, 150, 302, "BOTTOM-RIGHT = BLUE", 0xFFFF);
    for (int x = 0; x < 320; x += 16) c.vLine(x, 130, 150, 0x4208);
    ui::text(c, 40, 200, "Premi un tasto per continuare", 0xFFFF);
    board::invalidate();
    board::present();
    std::uint16_t raw = 0;
    while (true) { board::service(); if (board::pollKey(raw) && (raw >> 8) == 1) break; board::idle(); }
}
#endif
} // namespace

int main() {
    board::init();
    std::printf("PicoCalc FX %s booting (%s)\n", FXAI_VERSION, board::name());
#if FXAI_DISPLAY_CALIBRATION_SCREEN
    calibrationScreen();
#endif
    fx::BoardPlatform platform;
    static fx::Firmware firmware(platform, board::storageFlash());
    firmware.boot();
    std::printf("workspace %s, storage %u bytes, free heap %u bytes\n", firmware.loadedFromFlash() ? "restored" : "new",
                static_cast<unsigned>(board::storageFlash().size()), static_cast<unsigned>(board::freeHeap()));
    board::invalidate();
    firmware.draw(board::graphics());
    board::present();
    while (true) {
        board::service();
        std::uint16_t raw = 0;
        while (board::pollKey(raw)) firmware.rawKey(raw);
        if (firmware.step()) {
            firmware.draw(board::graphics());
            board::present();
        }
        board::idle();
    }
}
