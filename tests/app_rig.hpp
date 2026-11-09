#pragma once
// Firmware driven through the host board port, as in the simulator: raw
// keyboard FIFO words in, ticks and redraws out (used by the v1.9.0 tests).
#include "apps.hpp"
#include "board_host.hpp"
#include "board_platform.hpp"
#include "board_port.hpp"
#include "firmware.hpp"
#include <string>

namespace testrig {
inline std::uint16_t press(std::uint8_t code, std::uint8_t state = 1) { return static_cast<std::uint16_t>((state << 8) | code); }

struct Rig {
    fx::BoardPlatform platform;
    fx::Firmware fw;
    explicit Rig(storage::FlashDevice* legacy = nullptr) : fw(platform, board::storageFlash(), legacy) { fw.boot(); settle(); }
    // Main loop until nothing needs a redraw (incremental work complete).
    void settle(int max = 4000) {
        for (int i = 0; i < max; ++i) {
            if (!fw.step()) break;
            fw.draw(board::graphics()); board::present();
            board::host::advance(5);
        }
    }
    void raw(std::uint16_t r) { fw.rawKey(r); board::host::advance(20); if (fw.step()) { fw.draw(board::graphics()); board::present(); } }
    void key(std::uint8_t code) { raw(press(code)); raw(press(code, 3)); }
    void type(const std::string& s) { for (char c : s) raw(press(static_cast<std::uint8_t>(c))); }
    void enter(const std::string& s) { type(s); key(fx::pckey::Enter); }
    bool error() { return fw.apps().errorShown(); }
    std::string errorTitle() { return fw.apps().errorTitle(); }
    std::string errorDetail() { return fw.apps().errorDetail(); }
    void closeError() { if (error()) key(fx::pckey::Esc); }
    fx::Workspace& ws() { return fw.workspace(); }
    void home() { key(fx::pckey::Tab); }
    template <class A> A* active() { return dynamic_cast<A*>(fw.apps().active()); }
};
} // namespace testrig
