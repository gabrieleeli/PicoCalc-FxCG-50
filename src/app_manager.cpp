#include "app.hpp"
#include <cstring>
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
const gfx::Canvas* Context::screen() const noexcept { return manager_.canvas_; }
void Context::home() {
    // v1.16.0: dalla modalita' di una striscia si torna all'eActivity.
    if (strip_.active && manager_.activeId_ != strip_.owner) { manager_.open(strip_.owner); return; }
    manager_.home();
}
void Context::beginStrip(AppId owner, AppId target, std::uint8_t entry) {
    strip_ = StripSession{true, owner, target, entry, false};
    try {
        manager_.open(target);
    } catch (...) {
        strip_ = StripSession{};
        throw;
    }
}
void Context::leaveSetup() {
    const AppId back = manager_.setupReturn_;
    manager_.setupReturn_ = AppId::MainMenu;
    if (back == AppId::MainMenu || back == AppId::System) { manager_.home(); return; }
    resuming_ = true;
    try { manager_.open(back); } catch (...) { resuming_ = false; throw; }
    resuming_ = false;
}
namespace {
// Copies into the capacity already reserved: never allocates (the text is cut).
void assignReserved(std::string& dst, const char* s, std::size_t n) noexcept {
    if (n > dst.capacity()) n = dst.capacity();
    dst.assign(s, n);
}
constexpr std::size_t ToastCapacity = 160, TitleCapacity = 48, DetailCapacity = 200;
} // namespace

void Context::toast(const std::string& message, std::uint32_t durationMs) { toast(message.c_str(), durationMs); }
void Context::toast(const char* message, std::uint32_t durationMs) {
    if (toast_.capacity() < ToastCapacity) toast_.reserve(ToastCapacity);   // first use (normal conditions)
    assignReserved(toast_, message ? message : "", message ? std::strlen(message) : 0);
    toastUntil_ = platform.millis() + durationMs;
}
ui::StatusInfo Context::status(const char* title) const {
    ui::StatusInfo s;
    s.title = title;
    s.angle = angleName(ws.settings.angle);
    s.display = displayModeName(ws.settings.display);
    s.complex = complexName(ws.settings.complex);
    s.fraction = ws.settings.mixedFractions ? "ab/c" : "d/c";
    s.input = ws.settings.inputMode == Settings::InputMode::Line ? "Line" : "Math";
    s.battery = platform.batteryPercent();
    s.charging = platform.charging();
    s.shift = shift_;
    // barra azzurra (manuale 10-20) anche nelle schermate aperte dalla modalita' della striscia
    s.strip = strip_.active && manager_.activeId_ != strip_.owner;
    return s;
}
void Context::drawStatus(gfx::Canvas& c, const char* title) const { ui::statusBar(c, status(title)); }

AppManager::AppManager(Workspace& ws, Platform& platform) : ctx_(ws, platform, *this) {
    // Reserved up front so that an out-of-memory error can always be shown.
    errorTitle_.reserve(TitleCapacity);
    errorDetail_.reserve(DetailCapacity);
    ctx_.toast_.reserve(ToastCapacity);
    // PxlTest( (v1.12.0): GRAPH disegna la sua schermata nel framebuffer, come
    // la VRAM della fx-CG50, e il pixel si legge da li'; la modalita' attiva
    // ridisegna subito dopo.
    ws.pixelProbe = [this](int row, int column) {
        App* graph = find(AppId::Graph);
        if (!graph || !canvas_) return 0;
        const int v = graph->probePixel(ctx_, *canvas_, row, column);
        fullRedraw_ = true;
        return v > 0 ? 1 : 0;
    };
}

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
    if (id != AppId::System) setupReturn_ = AppId::MainMenu;   // il ritorno vale solo per il SET UP appena aperto
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
void AppManager::showError(const std::string& what) { showError(what.c_str()); }
void AppManager::showError(const char* what) {
    // Split "Math ERROR: detail" into the fx-CG50 title and a detail line.
    if (!what) what = "";
    const char* colon = std::strchr(what, ':');
    const char* error = std::strstr(what, "ERROR");
    // Titoli della fx-CG50 senza la parola ERROR (Appendice α, tabella degli errori α-2, α-3).
    static const char* const kTitles[] = {"Can't Solve!", "Time Out", "No Variable", "Underflow", "No Solution", "Infinitely Many Solutions",
                                          "Complex Number In List", "Complex Number In Matrix", "Complex Number In Data",
                                          // v1.14.0 (3D Graph, Appendice α-12)
                                          "Invalid Setting", "Out of Domain", "INTSECT requires", "RELATION requires",
                                          // v1.16.0 (eActivity, Appendice α-6; Break del ricalcolo)
                                          "Not Found", "No MEMO", "Invalid Name", "Break"};
    bool known = false;
    if (colon) for (const char* t : kTitles) known = known || (std::strncmp(what, t, std::strlen(t)) == 0 && what + std::strlen(t) == colon);
    if (colon && ((error && error < colon) || known)) {
        assignReserved(errorTitle_, what, static_cast<std::size_t>(colon - what));
        const char* d = colon + 1;
        while (*d == ' ') ++d;
        assignReserved(errorDetail_, d, std::strlen(d));
    } else {
        assignReserved(errorTitle_, "ERROR", 5);
        assignReserved(errorDetail_, what, std::strlen(what));
    }
}
void AppManager::key(const KeyEvent& ev) {
    if (ev.none()) return;
    if (!errorTitle_.empty()) {   // any key closes the error box (EXIT on the CG50)
        errorTitle_.clear(); errorDetail_.clear();
        return;
    }
    // v1.16.0: dalla modalita' di una striscia MENU, SHIFT ' e SHIFT , tornano all'eActivity.
    // (anche da una modalita' aperta da quella della striscia, es. Graph da Table)
    const bool inStripApp = ctx_.strip_.active && activeId_ != ctx_.strip_.owner;
    if (inStripApp && ctx_.shift_ && ev.key == Key::Char && (ev.ch == '\'' || ev.ch == ',')) {
        ctx_.shift_ = false;
        ctx_.strip_.switchRequested = ev.ch == ',';
        open(ctx_.strip_.owner);
        return;
    }
    if (ev.is(Key::Menu) || (ev.is(Key::Exit) && ev.longPress)) {
        if (inStripApp) { open(ctx_.strip_.owner); return; }
        home();
        return;
    }
    if (ev.is(Key::Shift)) { ctx_.shift_ = !ctx_.shift_; return; }
    if (ev.is(Key::Setup) && activeId_ != AppId::System && !(active_ && active_->ownsSetup())) {
        const AppId from = activeId_;
        open(AppId::System);
        setupReturn_ = from;
        return;
    }
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
        try {
            redraw = active_->tick(ctx_);
        } catch (const std::bad_alloc&) {
            showError("Memory ERROR: memoria insufficiente"); redraw = true;
        } catch (const std::exception& e) {
            showError(e.what()); redraw = true;
        }
    }
    if (!ctx_.toast_.empty() && static_cast<std::int32_t>(ctx_.platform.millis() - ctx_.toastUntil_) >= 0) {
        ctx_.toast_.clear(); redraw = true;
    }
    return redraw;
}
void AppManager::draw(gfx::Canvas& c) {
    canvas_ = &c;
    c.resetClip();
    if (active_) {
        try { active_->draw(ctx_, c); }
        catch (const std::bad_alloc&) { ui::clear(c); ui::errorBox(c, "Memory ERROR", "memoria insufficiente"); }
        catch (const std::exception& e) { ui::clear(c); ui::errorBox(c, "ERROR", e.what()); }
    }
    c.resetClip();
    if (!ctx_.toast_.empty()) ui::toast(c, ctx_.toast_.c_str());
    if (!errorTitle_.empty()) ui::errorBox(c, errorTitle_.c_str(), errorDetail_.c_str());   // allocation-free
}
} // namespace fx::app
