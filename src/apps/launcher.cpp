#include "apps.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>

namespace fx::app {
using namespace ui::theme;

namespace {
std::uint16_t lighten(std::uint16_t c, std::uint8_t amount) { return gfx::Canvas::blend(0xFFFF, c, amount); }
std::uint16_t gray(std::uint16_t c) {
    const auto rgb = gfx::rgb565To888(c);
    const auto l = static_cast<std::uint8_t>((rgb.r * 30 + rgb.g * 59 + rgb.b * 11) / 100);
    const auto m = static_cast<std::uint8_t>(120 + l / 4);
    return gfx::rgb888To565(m, m, m);
}
} // namespace

// Pictograms are drawn with vector primitives in white on the mode colour, in
// the spirit of the fx-CG50 main-menu icons (no bitmap assets are copied).
void drawAppIcon(gfx::Canvas& c, AppId id, int x, int y, int w, int h, bool available) {
    const std::uint16_t base = available ? appInfo(id).color : gray(appInfo(id).color);
    c.roundedRect(x, y, w, h, 6, base, true);
    c.verticalGradient(x + 2, y + 2, w - 4, h / 2, lighten(base, 90), base);
    c.roundedRect(x, y, w, h, 6, gfx::Canvas::blend(0x0000, base, 70), false);
    const std::uint16_t W = 0xFFFF;
    const int cx = x + w / 2, cy = y + h / 2;
    auto txt = [&](int px, int py, const char* s, font::Face f = font::Face::Medium) { font::draw(c, f, px, py, s, W); };
    switch (id) {
    case AppId::RunMatrix:
        txt(x + 8, y + 4, "\xC3\x97\xC3\xB7"); txt(x + 8, y + 19, "+\xE2\x88\x92");
        c.rect(x + 32, y + 9, 2, 22, W, true); c.rect(x + 32, y + 9, 5, 2, W, true); c.rect(x + 32, y + 29, 5, 2, W, true);
        txt(x + 37, y + 13, "88", font::Face::Small);
        break;
    case AppId::Statistics: {
        const int hs[] = {10, 22, 16, 28};
        for (int i = 0; i < 4; ++i) c.rect(x + 9 + i * 10, y + h - 6 - hs[i], 7, hs[i], W, true);
        c.hLine(x + 6, y + h - 5, w - 12, W);
        break;
    }
    case AppId::EActivity:
        c.rect(x + 14, y + 5, 26, 30, W, true);
        for (int i = 0; i < 4; ++i) c.hLine(x + 18, y + 11 + i * 6, 18, base);
        break;
    case AppId::Spreadsheet:
        c.rect(x + 10, y + 7, 36, 26, W, false);
        for (int i = 1; i < 3; ++i) c.vLine(x + 10 + i * 12, y + 7, 26, W);
        for (int j = 1; j < 3; ++j) c.hLine(x + 10, y + 7 + j * 9, 36, W);
        c.rect(x + 11, y + 8, 11, 8, lighten(base, 160), true);
        break;
    case AppId::Graph: case AppId::DynaGraph: {
        c.hLine(x + 6, cy + 2, w - 12, W); c.vLine(cx - 6, y + 5, h - 10, W);
        gfx::Point pts[24];
        const int curves = id == AppId::DynaGraph ? 3 : 1;
        for (int k = 0; k < curves; ++k) {
            for (int i = 0; i < 24; ++i) {
                const double t = i / 23.0;
                pts[i] = {x + 7 + static_cast<int>(t * (w - 14)), cy + 2 - static_cast<int>(std::lround((10 - k * 4) * std::sin(t * 6.28)))};
            }
            c.polyline(pts, 24, W);
        }
        break;
    }
    case AppId::Table:
        c.rect(x + 9, y + 6, 38, 28, W, false); c.vLine(x + 23, y + 6, 28, W); c.hLine(x + 9, y + 13, 38, W);
        txt(x + 12, y + 14, "x", font::Face::Small); txt(x + 27, y + 14, "123", font::Face::Small); txt(x + 27, y + 22, "456", font::Face::Small);
        break;
    case AppId::Recursion: txt(x + 8, y + 4, "an+1"); txt(x + 8, y + 19, "=An", font::Face::Small); txt(x + 26, y + 19, "+B", font::Face::Small); break;
    case AppId::ConicGraphs:
        c.hLine(x + 6, cy, w - 12, W); c.vLine(cx, y + 5, h - 10, W); c.ellipse(cx, cy, 18, 11, W, false);
        break;
    case AppId::Equation: txt(x + 5, y + 6, "aX\xC2\xB2+bX", font::Face::Small); txt(x + 10, y + 20, "+c=0"); break;
    case AppId::Program:
        c.rect(x + 18, y + 4, 20, 9, W, true); c.vLine(cx, y + 13, 6, W);
        c.rect(x + 8, y + 19, 16, 9, W, true); c.rect(x + 32, y + 19, 16, 9, W, true);
        c.hLine(x + 16, y + 17, 24, W); c.vLine(x + 16, y + 17, 3, W); c.vLine(x + 40, y + 17, 3, W);
        break;
    case AppId::Financial: c.circle(cx, cy, 13, W, false); txt(cx - 4, cy - 8, "$"); break;
    case AppId::ECon4: c.rect(x + 10, y + 10, 22, 18, W, false); c.line(x + 32, y + 19, x + 46, y + 12, W); c.circle(x + 46, y + 12, 3, W, true); break;
    case AppId::Link:
        c.rect(x + 6, y + 9, 18, 14, W, false); c.rect(x + 32, y + 9, 18, 14, W, false);
        c.hLine(x + 24, y + 16, 8, W); c.rect(x + 10, y + 26, 10, 3, W, true); c.rect(x + 36, y + 26, 10, 3, W, true);
        break;
    case AppId::Memory:
        c.rect(x + 10, y + 12, 36, 22, W, true); c.rect(x + 10, y + 8, 14, 5, W, true);
        c.hLine(x + 12, y + 18, 32, base);
        break;
    case AppId::System:
        c.circle(cx, cy, 11, W, true); c.circle(cx, cy, 5, base, true);
        for (int k = 0; k < 8; ++k) { const double a = k * 0.785398; c.rect(cx + static_cast<int>(13 * std::cos(a)) - 2, cy + static_cast<int>(13 * std::sin(a)) - 2, 4, 4, W, true); }
        break;
    case AppId::Geometry: {
        const gfx::Point tri[] = {{x + 10, y + 32}, {x + 46, y + 32}, {x + 22, y + 8}};
        c.polygon(tri, 3, W, false); c.circle(x + 26, y + 24, 7, W, false);
        break;
    }
    case AppId::PicturePlot:
        c.rect(x + 8, y + 6, 40, 28, W, false);
        for (int i = 0; i < 6; ++i) c.circle(x + 13 + i * 6, y + 28 - (i * i) / 2, 1, W, true);
        break;
    case AppId::Graph3D:
        for (int i = 0; i < 4; ++i) { c.line(x + 8 + i * 6, y + 30 - i * 4, x + 32 + i * 6, y + 26 - i * 4, W); c.line(x + 8 + i * 8, y + 30, x + 20 + i * 8, y + 14, W); }
        break;
    case AppId::Conversion:
        c.rect(x + 8, y + 14, 40, 12, W, false);
        for (int i = 0; i < 8; ++i) c.vLine(x + 11 + i * 5, y + 14, i % 2 ? 4 : 7, W);
        break;
    case AppId::Python: txt(x + 11, y + 5, ">>>"); txt(x + 8, y + 20, "py", font::Face::Medium); txt(x + 26, y + 22, "def", font::Face::Small); break;
    case AppId::AiChat: {
        c.roundedRect(x + 8, y + 6, 40, 22, 6, W, true);
        const gfx::Point tail[] = {{x + 16, y + 27}, {x + 24, y + 27}, {x + 14, y + 34}};
        c.polygon(tail, 3, W, true);
        txt(x + 20, y + 9, "AI");
        break;
    }
    case AppId::Symbols: txt(x + 8, y + 4, "\xCE\xB1\xCE\xA3\xCF\x80"); txt(x + 10, y + 19, "\xE2\x88\x9A\xE2\x88\xAB"); break;
    default: break;
    }
}

bool LauncherApp::available(std::size_t i) const noexcept { return i < AppCount && available_[i]; }

void LauncherApp::enter(Context&) {}

void LauncherApp::key(Context& ctx, const KeyEvent& ev) {
    constexpr int Cols = 4;
    const int count = static_cast<int>(AppCount);
    int sel = static_cast<int>(selected_);
    switch (ev.key) {
    case Key::Left: sel = (sel + count - 1) % count; break;
    case Key::Right: sel = (sel + 1) % count; break;
    case Key::Up: sel = sel - Cols >= 0 ? sel - Cols : sel; break;
    case Key::Down: sel = sel + Cols < count ? sel + Cols : sel; break;
    case Key::Exe: ctx.open(static_cast<AppId>(selected_)); return;
    case Key::Char: {
        // Direct access with the badge key (1-9, A-N) like on the fx-CG50.
        const char k = static_cast<char>(std::toupper(static_cast<unsigned char>(ev.ch)));
        for (std::size_t i = 0; i < AppCount; ++i) {
            if (appInfoAt(i).key[0] == k) { selected_ = i; ctx.open(static_cast<AppId>(i)); return; }
        }
        return;
    }
    default: return;
    }
    selected_ = static_cast<std::size_t>(sel);
}

void LauncherApp::draw(Context& ctx, gfx::Canvas& c) {
    ui::clear(c);
    ctx.drawStatus(c, "MAIN MENU");
    constexpr int Cols = 4, TileW = 80, TileH = 70, Top = ui::ContentTop + 2, VisibleRows = 4;
    const int rows = static_cast<int>((AppCount + Cols - 1) / Cols);
    const int selRow = static_cast<int>(selected_) / Cols;
    if (selRow < firstRow_) firstRow_ = selRow;
    if (selRow >= firstRow_ + VisibleRows) firstRow_ = selRow - VisibleRows + 1;
    for (int r = 0; r < VisibleRows; ++r) {
        for (int col = 0; col < Cols; ++col) {
            const std::size_t i = static_cast<std::size_t>((firstRow_ + r) * Cols + col);
            if (i >= AppCount) break;
            const int x = col * TileW, y = Top + r * TileH;
            const bool sel = i == selected_;
            const bool ok = available(i);
            if (sel) { c.roundedRect(x + 2, y + 1, TileW - 4, TileH - 2, 6, SelectSoft, true); c.roundedRect(x + 2, y + 1, TileW - 4, TileH - 2, 6, Select, false); c.roundedRect(x + 3, y + 2, TileW - 6, TileH - 4, 5, Select, false); }
            const AppInfo& info = appInfoAt(i);
            drawAppIcon(c, info.id, x + 12, y + 6, 56, 40, ok);
            // Key badge in the upper-right corner of the icon.
            c.roundedRect(x + 58, y + 3, 13, 13, 3, 0xFFFF, true);
            c.roundedRect(x + 58, y + 3, 13, 13, 3, Ink, false);
            font::draw(c, font::Face::Small, x + 62, y + 3, info.key, Ink);
            if (!ok) { c.circle(x + 15, y + 9, 5, Error, true); c.hLine(x + 12, y + 9, 7, 0xFFFF); }
            ui::textCenter(c, x + TileW / 2, y + 50, info.name, ok ? Ink : Muted, font::Face::Small);
        }
    }
    ui::scrollBar(c, ui::ScreenW - 4, Top, VisibleRows * TileH, rows, VisibleRows, firstRow_);
    const AppInfo& cur = appInfoAt(selected_);
    char footer[80];
    std::snprintf(footer, sizeof(footer), "%s  [%s]  %s", cur.name, cur.key, available(selected_) ? "EXE: apri" : "non disponibile");
    c.hLine(0, ui::ScreenH - 17, ui::ScreenW, Faint);
    ui::text(c, 4, ui::ScreenH - 14, footer, available(selected_) ? Ink : Error, font::Face::Small);
}

void UnavailableApp::key(Context& ctx, const KeyEvent& ev) {
    if (ev.is(Key::Exit) || ev.is(Key::Exe) || ev.is(Key::AC) || ev.is(Key::F6)) ctx.home();
}
void UnavailableApp::draw(Context& ctx, gfx::Canvas& c) {
    const AppInfo& info = appInfo(manager_.requestedUnavailable());
    ui::clear(c);
    ctx.drawStatus(c, info.name);
    drawAppIcon(c, info.id, 132, 34, 56, 40, false);
    ui::textCenter(c, ui::ScreenW / 2, 84, info.name, Ink);
    ui::textCenter(c, ui::ScreenW / 2, 106, "MODALITA' NON DISPONIBILE", Error);
    std::string lines[6];
    const std::size_t n = ui::wrapText(info.missing ? info.missing : "Modalita' non ancora implementata.", 34, lines, 6);
    for (std::size_t i = 0; i < n; ++i) ui::textCenter(c, ui::ScreenW / 2, 132 + static_cast<int>(i) * 16, lines[i].c_str(), Ink);
    ui::textCenter(c, ui::ScreenW / 2, 240, "Vedi docs/FEATURE_MATRIX.md", Muted, font::Face::Small);
    const char* keys[6] = {nullptr, nullptr, nullptr, nullptr, nullptr, "MENU"};
    ui::softKeys(c, keys);
}

int PopupMenu::key(const KeyEvent& ev) {
    if (!open) return -2;
    const int n = static_cast<int>(items.size());
    switch (ev.key) {
    case Key::Up: selected = (selected + n - 1) % std::max(1, n); break;
    case Key::Down: selected = (selected + 1) % std::max(1, n); break;
    case Key::PageUp: selected = std::max(0, selected - 8); break;
    case Key::PageDown: selected = std::min(n - 1, selected + 8); break;
    case Key::Exe: case Key::Right: open = false; return selected;
    case Key::Exit: case Key::Left: case Key::AC: open = false; return -2;
    case Key::Char:
        if (ev.ch >= '1' && ev.ch <= '9' && ev.ch - '1' < n) { selected = ev.ch - '1'; open = false; return selected; }
        break;
    default: break;
    }
    if (selected < first) first = selected;
    if (selected >= first + 8) first = selected - 7;
    return -1;
}
void PopupMenu::draw(gfx::Canvas& c) const {
    if (!open) return;
    std::vector<const char*> ptrs; ptrs.reserve(items.size());
    for (const auto& s : items) ptrs.push_back(s.c_str());
    ui::menuPopup(c, title ? title : "", ptrs.data(), static_cast<int>(ptrs.size()), selected, first, 8);
}

} // namespace fx::app
