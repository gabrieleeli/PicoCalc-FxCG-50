#pragma once
#include "app_registry.hpp"
#include "graphics_engine.hpp"
#include "keyboard_input.hpp"
#include "ui_kit.hpp"
#include "workspace.hpp"
#include <cstdint>
#include <memory>
#include <string>

namespace fx {
namespace net { class ChatTransport; }

namespace app {

// Hardware services an application may use (implemented by the Pico and host ports).
class Platform {
public:
    virtual ~Platform() = default;
    virtual std::uint32_t millis() = 0;
    virtual int batteryPercent() = 0;           // -1 when unknown
    virtual bool charging() = 0;
    virtual void setBacklight(std::uint8_t lcd, std::uint8_t keyboard) = 0;
    virtual std::size_t freeHeap() = 0;
    virtual std::size_t storageCapacity() = 0;  // bytes available for the workspace record
    virtual const char* name() = 0;
    virtual net::ChatTransport* chat() = 0;     // nullptr when networking is unavailable
};

class AppManager;

class Context {
public:
    Context(Workspace& ws, Platform& platform, AppManager& manager) : ws(ws), platform(platform), manager_(manager) {}
    Workspace& ws;
    Platform& platform;

    void open(AppId id);
    void home();
    void markDirty() noexcept { dirty_ = true; }
    bool dirty() const noexcept { return dirty_; }
    void clearDirty() noexcept { dirty_ = false; }
    void toast(const std::string& message, std::uint32_t durationMs = 1600);
    bool shift() const noexcept { return shift_; }
    void setShift(bool on) noexcept { shift_ = on; }
    std::uint32_t now() { return platform.millis(); }
    ui::StatusInfo status(const char* title) const;
    void drawStatus(gfx::Canvas& c, const char* title) const;

    // Clipboard shared by every editor (Ctrl+C / Ctrl+V).
    std::shared_ptr<ast::Node> clipboard;

private:
    friend class AppManager;
    AppManager& manager_;
    bool dirty_{};
    bool shift_{};
    std::string toast_;
    std::uint32_t toastUntil_{};
};

class App {
public:
    virtual ~App() = default;
    virtual AppId id() const = 0;
    virtual void enter(Context&) {}
    virtual void leave(Context&) {}
    virtual void key(Context& ctx, const KeyEvent& event) = 0;
    virtual void draw(Context& ctx, gfx::Canvas& canvas) = 0;
    // Periodic work (network polling...). Returns true when a redraw is needed.
    virtual bool tick(Context&) { return false; }
};

// Routes input to the active application, handles the global keys (MENU,
// SHIFT, long EXIT) and converts exceptions into fx-CG50 style error boxes.
class AppManager {
public:
    AppManager(Workspace& ws, Platform& platform);
    void add(App& app);
    void start();
    void key(const KeyEvent& event);
    bool tick();
    void draw(gfx::Canvas& canvas);
    void open(AppId id);
    void home();
    AppId activeId() const noexcept { return activeId_; }
    App* active() const noexcept { return active_; }
    Context& context() noexcept { return ctx_; }
    bool errorShown() const noexcept { return !errorTitle_.empty(); }
    const std::string& errorTitle() const noexcept { return errorTitle_; }
    // Shows an fx-CG50 error box; the next key closes it.
    void showError(const std::string& what);
    bool needsFullRedraw() noexcept { const bool r = fullRedraw_; fullRedraw_ = false; return r; }

private:
    App* find(AppId id) const noexcept;
    void switchTo(App* app, AppId id);
    Context ctx_;
    App* apps_[AppCount]{};
    App* launcher_{};
    App* unavailable_{};
    App* active_{};
    AppId activeId_{AppId::MainMenu};
    std::string errorTitle_, errorDetail_;
    bool fullRedraw_{true};
    friend class Context;
public:
    void setLauncher(App& launcher) noexcept { launcher_ = &launcher; }
    void setUnavailable(App& unavailable) noexcept { unavailable_ = &unavailable; }
    AppId requestedUnavailable() const noexcept { return requested_; }
private:
    AppId requested_{AppId::MainMenu};
};

} // namespace app
} // namespace fx
