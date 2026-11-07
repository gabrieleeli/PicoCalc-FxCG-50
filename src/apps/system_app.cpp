// SYSTEM / SET UP: angle unit, display format, complex mode, fraction results,
// backlights, Wi-Fi and AI relay configuration, device information and reset.
#include "apps.hpp"
#include "net.hpp"
#include <algorithm>
#include <cstdio>

namespace fx::app {
using namespace ui::theme;

namespace {
std::vector<std::string> displayChoices() {
    std::vector<std::string> v = {"Norm1", "Norm2"};
    for (int i = 0; i <= 9; ++i) v.push_back("Fix" + std::to_string(i));
    for (int i = 1; i <= 10; ++i) v.push_back("Sci" + std::to_string(i));
    v.push_back("Eng");
    return v;
}
int displayIndex(DisplayFormat f) {
    switch (f.mode) {
    case DisplayMode::Norm1: return 0;
    case DisplayMode::Norm2: return 1;
    case DisplayMode::Fix: return 2 + std::min<int>(f.digits, 9);
    case DisplayMode::Sci: return 12 + std::clamp<int>(f.digits == 0 ? 10 : f.digits, 1, 10) - 1;
    case DisplayMode::Eng: return 22;
    }
    return 0;
}
DisplayFormat displayFrom(int i) {
    if (i <= 0) return {DisplayMode::Norm1, 0};
    if (i == 1) return {DisplayMode::Norm2, 0};
    if (i <= 11) return {DisplayMode::Fix, static_cast<std::uint8_t>(i - 2)};
    if (i <= 21) return {DisplayMode::Sci, static_cast<std::uint8_t>(i - 11)};
    return {DisplayMode::Eng, 0};
}
FormField choice(const char* l, std::vector<std::string> c, int sel) { const std::string t = c[static_cast<std::size_t>(sel)]; return {l, t, FormField::Type::Choice, std::move(c), sel}; }
FormField text(const char* l, const std::string& v, bool secret = false) { return {l, v, secret ? FormField::Type::Secret : FormField::Type::Text, {}, 0}; }
FormField number(const char* l, double v) { return {l, formatReal(v), FormField::Type::Number, {}, 0}; }
enum Field { FAngle, FDisplay, FComplex, FFrac, FLcd, FKbd, FSsid, FPass, FHost, FPort, FPath, FToken, FProvider, FModel, FTokens };
} // namespace

void SystemApp::enter(Context& ctx) {
    const Settings& s = ctx.ws.settings;
    form_.fields = {
        choice("Angolo", {"Deg", "Rad", "Gra"}, s.angle == AngleMode::Degrees ? 0 : s.angle == AngleMode::Radians ? 1 : 2),
        choice("Display", displayChoices(), displayIndex(s.display)),
        choice("Complessi", {"Real", "a+bi", "r\xE2\x88\xA0\xCE\xB8"}, static_cast<int>(s.complex)),
        choice("Frazioni", {"On", "Off"}, s.fractionResults ? 0 : 1),
        number("Luce LCD", s.lcdBacklight),
        number("Luce tastiera", s.kbdBacklight),
        text("Wi-Fi SSID", s.wifiSsid),
        text("Wi-Fi password", s.wifiPassword, true),
        text("Relay host", s.relayHost),
        number("Relay porta", s.relayPort),
        text("Relay path", s.relayPath),
        text("Token relay", s.relayToken, true),
        choice("AI provider", {"Claude", "ChatGPT"}, s.aiProvider ? 1 : 0),
        text("AI modello", s.aiModel),
        number("AI max token", s.aiMaxTokens),
    };
    form_.selected = 0; info_ = false; confirmReset_ = false;
}

void SystemApp::apply(Context& ctx) {
    Settings& s = ctx.ws.settings;
    const auto& f = form_.fields;
    s.angle = f[FAngle].choice == 0 ? AngleMode::Degrees : f[FAngle].choice == 1 ? AngleMode::Radians : AngleMode::Gradians;
    s.display = displayFrom(f[FDisplay].choice);
    s.complex = static_cast<ComplexMode>(f[FComplex].choice);
    s.fractionResults = f[FFrac].choice == 0;
    const double lcd = form_.number(ctx, FLcd), kbd = form_.number(ctx, FKbd), port = form_.number(ctx, FPort), tokens = form_.number(ctx, FTokens);
    if (lcd < 0 || lcd > 255 || kbd < 0 || kbd > 255) throw MathError("Argument ERROR: retroilluminazione 0..255");
    if (port < 1 || port > 65535 || port != static_cast<int>(port)) throw MathError("Argument ERROR: porta 1..65535");
    if (tokens < 16 || tokens > 4000) throw MathError("Argument ERROR: max token 16..4000");
    s.lcdBacklight = static_cast<std::uint8_t>(lcd); s.kbdBacklight = static_cast<std::uint8_t>(kbd);
    s.wifiSsid = f[FSsid].text; s.wifiPassword = f[FPass].text; s.relayHost = f[FHost].text;
    s.relayPort = static_cast<std::uint16_t>(port); s.relayPath = f[FPath].text.empty() ? "/v1/chat" : f[FPath].text;
    s.relayToken = f[FToken].text; s.aiProvider = static_cast<std::uint8_t>(f[FProvider].choice);
    s.aiModel = f[FModel].text; s.aiMaxTokens = static_cast<std::uint16_t>(tokens);
    ctx.ws.apply();
    ctx.platform.setBacklight(s.lcdBacklight, s.kbdBacklight);
    ctx.markDirty();
}
void SystemApp::leave(Context& ctx) { try { apply(ctx); } catch (...) {} }

void SystemApp::key(Context& ctx, const KeyEvent& ev) {
    if (confirmReset_) {
        if (ev.is(Key::Exe) || ev.isChar('y') || ev.isChar('s')) { ctx.ws.resetAll(); ctx.markDirty(); enter(ctx); ctx.toast("Memoria azzerata"); }
        confirmReset_ = false; return;
    }
    if (info_) { if (ev.is(Key::Exit) || ev.is(Key::F1)) info_ = false; return; }
    if (form_.editing()) { (void)form_.key(ctx, ev); return; }
    switch (ev.key) {
    case Key::F1: info_ = true; return;
    case Key::F5: confirmReset_ = true; return;
    case Key::F6: apply(ctx); ctx.toast("Impostazioni salvate"); return;
    case Key::Exit: apply(ctx); ctx.home(); return;
    default: break;
    }
    const bool consumed = form_.key(ctx, ev);
    // Choice changes take effect immediately (angle/display shown in the status bar).
    if (consumed && (ev.is(Key::Left) || ev.is(Key::Right))) apply(ctx);
}

void SystemApp::draw(Context& ctx, gfx::Canvas& c) {
    ui::clear(c);
    ctx.drawStatus(c, "SYSTEM / SET UP");
    const int top = ui::ContentTop + 1;
    if (info_) {
        const auto blob = serializeWorkspace(ctx.ws);
        char buf[96];
        int y = top + 8;
        auto line = [&](const char* l, const std::string& v) { ui::text(c, 8, y, l, Accent, font::Face::Small); ui::text(c, 130, y, v.c_str(), Ink, font::Face::Small); y += 18; };
        line("Firmware", std::string("PicoCalc FX ") + FXAI_VERSION_STRING);
        line("Piattaforma", ctx.platform.name());
        std::snprintf(buf, sizeof(buf), "%u KB", static_cast<unsigned>(ctx.platform.freeHeap() / 1024)); line("RAM libera", buf);
        std::snprintf(buf, sizeof(buf), "%u / %u byte", static_cast<unsigned>(blob.size()), static_cast<unsigned>(ctx.platform.storageCapacity())); line("Dati salvati", buf);
        const int b = ctx.platform.batteryPercent();
        line("Batteria", b < 0 ? std::string("n/d (USB)") : std::to_string(b) + "%" + (ctx.platform.charging() ? " in carica" : ""));
        line("Wi-Fi", ctx.platform.chat() ? ctx.platform.chat()->wifiStatus() : "non disponibile");
        y += 6;
        std::string lines[5];
        const std::size_t n = ui::wrapText("Firmware indipendente ispirato alla fx-CG50: nessun codice o firmware Casio incluso. Le chiavi API restano sul relay, mai sul dispositivo.", 50, lines, 5);
        for (std::size_t i = 0; i < n; ++i) { ui::text(c, 8, y, lines[i].c_str(), Muted, font::Face::Small); y += 14; }
        const char* k[6] = {"BACK", nullptr, nullptr, nullptr, nullptr, nullptr};
        ui::softKeys(c, k);
        return;
    }
    form_.draw(c, top + 2, ui::ContentBottom - 2);
    const char* k[6] = {"INFO", nullptr, nullptr, nullptr, "RESET", "SAVE"};
    ui::softKeys(c, k);
    if (confirmReset_) ui::errorBox(c, "RESET", "Cancellare variabili, storia, liste, funzioni e impostazioni? EXE = conferma");
}
} // namespace fx::app
