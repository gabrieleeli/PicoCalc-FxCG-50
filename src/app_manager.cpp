#include "app.hpp"
#include "expression.hpp"
#include <cstdio>
#include <exception>
#include <new>

namespace fx::app {
namespace {
const char* angleName(AngleMode a) { return a == AngleMode::Degrees ? "Deg" : a == AngleMode::Radians ? "Rad" : "Gra"; }
const char* complexName(ComplexMode m) { return m == ComplexMode::Real ? "Real" : m == ComplexMode::Rectangular ? "a+bi" : "r\xE2\x88\xA0\xCE\xB8"; }
} // namespace

void Context::open(AppId id) { manager_.open(id); }
void Context::home() { manager_.home(); }
void Context::toast(const std::string& message, std::uint32_t durationMs) {
    toast_ = message;
    toastUntil_ = platform.millis() + durationMs;
}
ui::StatusInfo Context::status(const char* title) const {
    ui::StatusInfo s;
    s.title = title;
    s.angle = angleName(ws.settings.angle);
    s.display = displayModeName(ws.settings.display);
    s.complex = complexName(ws.settings.complex);
    s.fraction = ws.settings.mixedFractions ? "ab/c" : "d/c";
    s.battery = platform.batteryPercent();
    s.charging = platform.charging();
    s.shift = shift_;
    return s;
}
void Context::drawStatus(gfx::Canvas& c, const char* title) const { ui::statusBar(c, status(title)); }

AppManager::AppManager(Workspace& ws, Platform& platform) : ctx_(ws, platform, *this) {}

void AppManager::add(App& a) {
    const auto i = static_cast<std::size_t>(a.id());
    if (i < AppCount) apps_[i] = &a;
}
App* AppManager::find(AppId id) const noexcept {
    const auto i = static_cast<std::size_t>(id);
    return i < AppCount ? apps_[i] : nullptr;
}
void AppManager::switchTo(App* next, AppId id) {
    if (active_) {
        try { active_->leave(ctx_); } catch (...) {}
    }
    active_ = next; activeId_ = id; fullRedraw_ = true;
    ctx_.shift_ = false;
    if (active_) {
        try { active_->enter(ctx_); } catch (const std::exception& e) { showError(e.what()); }
    }
}
void AppManager::start() { ctx_.ws.apply(); switchTo(launcher_, AppId::MainMenu); }
void AppManager::home() { switchTo(launcher_, AppId::MainMenu); }
void AppManager::open(AppId id) {
    if (id == AppId::MainMenu) { home(); return; }
    if (App* a = find(id)) { switchTo(a, id); return; }
    requested_ = id;
    switchTo(unavailable_, id);
}
void AppManager::showError(const std::string& what) {
    // Split "Math ERROR: detail" into the fx-CG50 title and a detail line.
    const auto colon = what.find(':');
    if (colon != std::string::npos && what.find("ERROR") < colon) {
        errorTitle_ = what.substr(0, colon);
        errorDetail_ = what.substr(colon + 1);
        while (!errorDetail_.empty() && errorDetail_.front() == ' ') errorDetail_.erase(errorDetail_.begin());
    } else {
        errorTitle_ = "ERROR";
        errorDetail_ = what;
    }
}
void AppManager::key(const KeyEvent& ev) {
    if (ev.none()) return;
    if (!errorTitle_.empty()) {   // any key closes the error box (EXIT on the CG50)
        errorTitle_.clear(); errorDetail_.clear();
        return;
    }
    if (ev.is(Key::Menu) || (ev.is(Key::Exit) && ev.longPress)) { home(); return; }
    if (ev.is(Key::Shift)) { ctx_.shift_ = !ctx_.shift_; return; }
    if (ev.is(Key::Setup) && activeId_ != AppId::System && !(active_ && active_->ownsSetup())) { open(AppId::System); return; }
    if (!active_) return;
    const bool shiftBefore = ctx_.shift_;
    try {
        active_->key(ctx_, ev);
    } catch (const std::bad_alloc&) {
        showError("Memory ERROR: memoria insufficiente");
    } catch (const std::exception& e) {
        showError(e.what());
    }
    if (shiftBefore) ctx_.shift_ = false;   // SHIFT is one-shot, like the CG50
}
bool AppManager::tick() {
    bool redraw = false;
    if (active_) {
        try { redraw = active_->tick(ctx_); } catch (const std::exception& e) { showError(e.what()); redraw = true; }
    }
    if (!ctx_.toast_.empty() && static_cast<std::int32_t>(ctx_.platform.millis() - ctx_.toastUntil_) >= 0) {
        ctx_.toast_.clear(); redraw = true;
    }
    return redraw;
}
void AppManager::draw(gfx::Canvas& c) {
    c.resetClip();
    if (active_) {
        try { active_->draw(ctx_, c); }
        catch (const std::exception& e) { ui::clear(c); ui::errorBox(c, "ERROR", e.what()); }
    }
    c.resetClip();
    if (!ctx_.toast_.empty()) ui::toast(c, ctx_.toast_.c_str());
    if (!errorTitle_.empty()) ui::errorBox(c, errorTitle_.c_str(), errorDetail_.c_str());
}
} // namespace fx::app
