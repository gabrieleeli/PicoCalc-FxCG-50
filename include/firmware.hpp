#pragma once
#include "app.hpp"
#include "keyboard_input.hpp"
#include "storage.hpp"
#include "workspace.hpp"
#include <cstdint>
#include <memory>

namespace fx {
// Portable firmware core: owns the workspace, the applications, the key
// decoder and persistence. The Pico main loop and the host simulator both
// drive this class, so the logic tested on the host is the logic flashed.
class Firmware {
public:
    Firmware(app::Platform& platform, storage::FlashDevice& flash);
    ~Firmware();
    void boot();
    void rawKey(std::uint16_t raw);
    void key(const KeyEvent& ev);
    // Periodic work; returns true when the screen must be redrawn.
    bool step();
    void draw(gfx::Canvas& canvas);
    bool saveNow();
    bool loadedFromFlash() const noexcept { return loaded_; }
    Workspace& workspace() noexcept { return ws_; }
    app::AppManager& apps() noexcept { return manager_; }
    std::uint32_t saves() const noexcept { return saves_; }
private:
    struct Apps;
    app::Platform& platform_;
    Workspace ws_;
    storage::RecordStore store_;
    app::AppManager manager_;
    KeyDecoder decoder_;
    std::unique_ptr<Apps> apps_;
    bool redraw_{true};
    bool loaded_{};
    std::uint32_t lastInput_{};
    std::uint32_t saves_{};
    app::AppId lastApp_{app::AppId::MainMenu};
};
} // namespace fx
