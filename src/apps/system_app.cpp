// SYSTEM / SET UP: angle unit, display format, complex mode, exact results, Frac Result (d/c, ab/c),
// backlights, Wi-Fi and AI relay configuration, device information and reset.
#include "apps.hpp"
#include "board_config.hpp"
#include "diagnostics.hpp"
#include "mem_pools.hpp"
#include "net.hpp"
#include "psram.hpp"
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
enum Field { FAngle, FDisplay, FComplex, FFrac, FMixed, FLcd, FKbd, FFiles, FSsid, FPass, FHost, FPort, FPath, FToken, FProvider, FModel, FTokens };
} // namespace

void SystemApp::enter(Context& ctx) {
    const Settings& s = ctx.ws.settings;
    form_.fields = {
        choice("Angolo", {"Deg", "Rad", "Gra"}, s.angle == AngleMode::Degrees ? 0 : s.angle == AngleMode::Radians ? 1 : 2),
        choice("Display", displayChoices(), displayIndex(s.display)),
        choice("Complessi", {"Real", "a+bi", "r\xE2\x88\xA0\xCE\xB8"}, static_cast<int>(s.complex)),
        choice("Forme esatte", {"On", "Off"}, s.fractionResults ? 0 : 1),
        choice("Frac Result", {"d/c", "ab/c"}, s.mixedFractions ? 1 : 0),
        number("Luce LCD", s.lcdBacklight),
        number("Luce tastiera", s.kbdBacklight),
        choice("Archivio file", {"microSD", "Flash interna"}, s.fileStore == Settings::FileStore::Internal ? 1 : 0),
        text("Wi-Fi SSID", s.wifiSsid),
        text("Wi-Fi password", s.wifiPassword, true),
        text("Relay host", s.relayHost),
        number("Relay porta", s.relayPort),
        text("Relay path", s.relayPath),
        text("Token relay", s.relayToken, true),
        choice("AI provider", {"Claude", "ChatGPT"}, s.aiProvider ? 1 : 0),
        text("AI modello", s.aiModel),
        number("AI max caratteri", s.aiMaxChars),
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
    s.mixedFractions = f[FMixed].choice == 1;
    s.fileStore = f[FFiles].choice == 1 ? Settings::FileStore::Internal : Settings::FileStore::MicroSd;
    const double lcd = form_.number(ctx, FLcd), kbd = form_.number(ctx, FKbd), port = form_.number(ctx, FPort), tokens = form_.number(ctx, FTokens);
    if (lcd < 0 || lcd > 255 || kbd < 0 || kbd > 255) throw MathError("Argument ERROR: retroilluminazione 0..255");
    if (port < 1 || port > 65535 || port != static_cast<int>(port)) throw MathError("Argument ERROR: porta 1..65535");
    if (tokens < 200 || tokens > 4000) throw MathError("Argument ERROR: AI max caratteri 200..4000");
    s.lcdBacklight = static_cast<std::uint8_t>(lcd); s.kbdBacklight = static_cast<std::uint8_t>(kbd);
    s.wifiSsid = f[FSsid].text; s.wifiPassword = f[FPass].text; s.relayHost = f[FHost].text;
    s.relayPort = static_cast<std::uint16_t>(port); s.relayPath = f[FPath].text.empty() ? "/v1/chat" : f[FPath].text;
    s.relayToken = f[FToken].text; s.aiProvider = static_cast<std::uint8_t>(f[FProvider].choice);
    s.aiModel = f[FModel].text; s.aiMaxChars = static_cast<std::uint16_t>(tokens);
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
    if (diag_) {
        if (ev.is(Key::Exit) || ev.is(Key::F1)) diag_ = false;
        else if (ev.is(Key::F2)) runDiagnostics(ctx);
        else if (ev.is(Key::Up)) diagTop_ = std::max(0, diagTop_ - 1);
        else if (ev.is(Key::Down)) diagTop_ = std::min(std::max(0, static_cast<int>(diagLines_.size()) - 1), diagTop_ + 1);
        return;
    }
    if (form_.editing()) { (void)form_.key(ctx, ev); return; }
    switch (ev.key) {
    case Key::F1: info_ = true; return;
    case Key::F2: apply(ctx); runDiagnostics(ctx); return;
    case Key::F5: confirmReset_ = true; return;
    case Key::F6: apply(ctx); ctx.toast("Impostazioni salvate"); return;
    case Key::Exit: apply(ctx); ctx.home(); return;
    default: break;
    }
    const bool consumed = form_.key(ctx, ev);
    // Choice changes take effect immediately (angle/display shown in the status bar).
    if (consumed && (ev.is(Key::Left) || ev.is(Key::Right))) apply(ctx);
}

// DIAG: memory/PSRAM/flash/display (v1.9.0), microSD and GRAPH (v1.8.0)
// self-tests, also printed on the USB serial port.
void SystemApp::runDiagnostics(Context& ctx) {
    diag_ = true; diagTop_ = 0; diagLines_.clear();
    const diag::Clock clock = [&ctx]() { return ctx.now(); };
    std::printf("[DIAG] PicoCalc FX %s on %s (%s), free SRAM heap %u KB\n", FXAI_VERSION_STRING, ctx.platform.name(),
                board::config::kBoardName, static_cast<unsigned>(ctx.platform.freeHeap() / 1024));
    diagLines_.emplace_back(true, std::string("RAM SRAM libera prima dei test: ") + std::to_string(ctx.platform.freeHeap() / 1024) + " KB");
    auto run = [&](const char* tag, const std::vector<diag::Line>& lines) {
        for (const auto& l : lines) diagLines_.emplace_back(l.ok, std::string(tag) + l.text);
    };
    run("MEM ", diag::testMemory(ctx.platform));
    run("PSR ", diag::testPsram(clock));
    run("ALC ", diag::testAllocator(clock, true));
    run("FLS ", diag::testFlashPsram(ctx.platform.diagnosticFlash(), clock));
    run("LCD ", diag::testDisplay(ctx.platform));
    run("SD  ", diag::testStorage(ctx.platform.files(), clock));
    run("INT ", diag::testStorage(ctx.platform.internalFiles(), clock));
    run("GPH ", diag::testGraph(ctx.ws, clock));
    diagLines_.emplace_back(true, std::string("RAM SRAM libera dopo i test: ") + std::to_string(ctx.platform.freeHeap() / 1024) + " KB");
    std::printf("[DIAG] end\n");
}

void SystemApp::draw(Context& ctx, gfx::Canvas& c) {
    ui::clear(c);
    ctx.drawStatus(c, "SYSTEM / SET UP");
    const int top = ui::ContentTop + 1;
    if (diag_) {
        c.rect(0, top, ui::ScreenW, 18, SelectSoft, true);
        int fails = 0;
        for (const auto& l : diagLines_) fails += l.first ? 0 : 1;
        const std::string head = fails ? "DIAGNOSTICA: " + std::to_string(fails) + " errori" : std::string("DIAGNOSTICA: tutto OK");
        ui::text(c, 6, top + 2, head.c_str(), fails ? Error : Ink);
        int y = top + 22;
        for (int i = diagTop_; i < static_cast<int>(diagLines_.size()) && y < ui::ContentBottom - 26; ++i) {
            const auto& l = diagLines_[static_cast<std::size_t>(i)];
            std::string wrapped[3];
            const std::size_t n = ui::wrapText(l.second, 49, wrapped, 3);
            ui::text(c, 4, y, l.first ? "+" : "!", l.first ? Accent : Error, font::Face::Small);
            for (std::size_t k = 0; k < n; ++k) { ui::text(c, 14, y, wrapped[k].c_str(), l.first ? Ink : Error, font::Face::Small); y += 13; }
            y += 2;
        }
        ui::text(c, 6, ui::ContentBottom - 22, "Risultati anche su USB seriale: righe [DIAG]", Muted, font::Face::Small);
        const char* k[6] = {"BACK", "RIPETI", nullptr, nullptr, nullptr, nullptr};
        ui::softKeys(c, k);
        return;
    }
    if (info_) {
        const auto blob = serializeWorkspace(ctx.ws);
        char buf[96];
        int y = top + 8;
        auto line = [&](const char* l, const std::string& v) { ui::text(c, 8, y, l, Accent, font::Face::Small); ui::text(c, 130, y, v.c_str(), Ink, font::Face::Small); y += 18; };
        line("Firmware", std::string("PicoCalc FX ") + FXAI_VERSION_STRING);
        line("Piattaforma", ctx.platform.name());
        std::snprintf(buf, sizeof(buf), "%u KB", static_cast<unsigned>(ctx.platform.freeHeap() / 1024)); line("SRAM libera", buf);
        if (mem::available(mem::Pool::PSRAM_BOARD)) {
            const auto ps = mem::stats(mem::Pool::PSRAM_BOARD);
            std::snprintf(buf, sizeof(buf), "%u KB liberi di %u KB", static_cast<unsigned>(ps.free / 1024), static_cast<unsigned>(ps.total / 1024));
            line("PSRAM", buf);
        } else {
            line("PSRAM", board::config::kHasPsram ? std::string("DISABILITATA (solo SRAM)") : std::string("non presente"));
        }
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
    const char* k[6] = {"INFO", "DIAG", nullptr, nullptr, "RESET", "SAVE"};
    ui::softKeys(c, k);
    if (confirmReset_) ui::errorBox(c, "RESET", "Cancellare variabili, storia, liste, funzioni e impostazioni? EXE = conferma");
}
} // namespace fx::app
