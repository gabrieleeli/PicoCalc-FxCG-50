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
    // legacy: record store of an older firmware, read once to migrate the
    // workspace when `flash` holds no record yet (Pico Plus 2 W, v1.8.0 data).
    Firmware(app::Platform& platform, storage::FlashDevice& flash, storage::FlashDevice* legacy = nullptr);
    ~Firmware();
    void boot();
    void rawKey(std::uint16_t raw);
    void key(const KeyEvent& ev);
    // Periodic work; returns true when the screen must be redrawn.
    bool step();
    void draw(gfx::Canvas& canvas);
    bool saveNow();
    bool loadedFromFlash() const noexcept { return loaded_; }
    bool migrated() const noexcept { return migrated_; }      // workspace imported from the legacy store
    // Message shown in an error box once the main menu is up (boot reports:
    // previous crash, PSRAM disabled...).
    void notice(const std::string& message);
    void notice(const char* message);   // allocation-free
    Workspace& workspace() noexcept { return ws_; }
    app::AppManager& apps() noexcept { return manager_; }
    std::uint32_t saves() const noexcept { return saves_; }
private:
    struct Apps;
    app::Platform& platform_;
    Workspace ws_;
    storage::RecordStore store_;
    storage::FlashDevice* legacy_{};
    app::AppManager manager_;
    KeyDecoder decoder_;
    std::unique_ptr<Apps> apps_;
    bool redraw_{true};
    bool loaded_{};
    bool migrated_{};
    std::uint32_t lastInput_{};
    std::uint32_t saves_{};
    app::AppId lastApp_{app::AppId::MainMenu};
};
} // namespace fx
