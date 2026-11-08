#include "ui_kit.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace ui {
using namespace theme;

void clear(gfx::Canvas& c, std::uint16_t color) { c.resetClip(); c.clear(color); }

static void batteryIcon(gfx::Canvas& c, int x, int y, int percent, bool charging) {
    c.rect(x, y, 18, 9, StatusInk, false);
    c.rect(x + 18, y + 3, 2, 3, StatusInk, true);
    if (percent < 0) { font::draw(c, font::Face::Small, x + 4, y - 2, "?", StatusInk); return; }
    const int fill = std::clamp(percent, 0, 100) * 14 / 100;
    const std::uint16_t col = percent <= 15 ? Error : (charging ? Ok : StatusInk);
    if (fill > 0) c.rect(x + 2, y + 2, fill, 5, col, true);
}

void statusBar(gfx::Canvas& c, const StatusInfo& s) {
    c.verticalGradient(0, 0, ScreenW, StatusH - 1, gfx::rgb888To565(248, 249, 251), StatusBg);
    c.hLine(0, StatusH - 1, ScreenW, StatusLine);
    int x = 4;
    if (s.shift) {
        c.rect(x, 3, 12, 12, Shift, true);
        font::draw(c, font::Face::Small, x + 3, 3, "S", 0xFFFF);
        x += 16;
    }
    auto item = [&](const char* t) {
        if (!t || !*t) return;
        font::draw(c, font::Face::Small, x, 3, t, StatusInk);
        x += font::measure(font::Face::Small, t).width + 8;
    };
    item(s.angle); item(s.display); item(s.complex); item(s.fraction);
    int right = ScreenW - 26;
    batteryIcon(c, right, 4, s.battery, s.charging);
    if (s.wifi) { right -= 18; font::draw(c, font::Face::Small, right, 3, "WiFi", Accent); right -= 8; }
    if (s.busy) { right -= 12; c.circle(right + 4, 8, 3, Warn, true); }
    if (s.title && *s.title) {
        const int w = font::measure(font::Face::Small, s.title).width;
        const int tx = std::max(x, right - 8 - w);
        font::drawClipped(c, font::Face::Small, tx, 3, s.title, Muted, right - 8 - tx);
    }
}

void softKeys(gfx::Canvas& c, const char* const labels[6]) {
    const int y = ScreenH - SoftKeyH;
    c.rect(0, y, ScreenW, SoftKeyH, Bg, true);
    for (int i = 0; i < 6; ++i) {
        const int x0 = i * ScreenW / 6, x1 = (i + 1) * ScreenW / 6;
        const char* label = labels ? labels[i] : nullptr;
        if (!label || !*label) continue;
        const int w = x1 - x0 - 2;
        c.roundedRect(x0 + 1, y + 1, w, SoftKeyH - 2, 3, KeyBg, true);
        c.hLine(x0 + 3, y + 2, w - 4, KeyTop);
        const auto m = font::measure(font::Face::Small, label);
        const int tx = x0 + 1 + std::max(1, (w - m.width) / 2);
        font::drawClipped(c, font::Face::Small, tx, y + 4, label, KeyInk, w - 2);
    }
}

int text(gfx::Canvas& c, int x, int y, const char* s, std::uint16_t color, font::Face face, int maxWidth) {
    return font::draw(c, face, x, y, s, color, maxWidth).width;
}
int textRight(gfx::Canvas& c, int right, int y, const char* s, std::uint16_t color, font::Face face) {
    const int w = font::measure(face, s).width;
    font::draw(c, face, right - w, y, s, color);
    return w;
}
int textCenter(gfx::Canvas& c, int cx, int y, const char* s, std::uint16_t color, font::Face face) {
    const int w = font::measure(face, s).width;
    font::draw(c, face, cx - w / 2, y, s, color);
    return w;
}
void textFit(gfx::Canvas& c, int x, int y, const std::string& s, int maxWidth, std::uint16_t color, font::Face face) {
    const int adv = font::advance(face);
    const std::size_t maxGlyphs = static_cast<std::size_t>(std::max(0, maxWidth / adv));
    if (font::glyphCount(s.c_str()) <= maxGlyphs) { font::draw(c, face, x, y, s.c_str(), color); return; }
    if (maxGlyphs == 0) return;
    // Keep maxGlyphs-1 glyphs and append an ellipsis.
    std::string out; std::size_t g = 0;
    const auto* p = reinterpret_cast<const unsigned char*>(s.c_str());
    while (*p && g + 1 < maxGlyphs) { const unsigned char* st = p; (void)font::nextCodepoint(p); out.append(reinterpret_cast<const char*>(st), static_cast<std::size_t>(p - st)); ++g; }
    out += "\xE2\x80\xA6";
    font::draw(c, face, x, y, out.c_str(), color);
}

void listRow(gfx::Canvas& c, int y, int h, const char* label, const char* value, bool selected, int x, int w) {
    if (selected) c.rect(x, y, w, h, Select, true);
    const std::uint16_t ink = selected ? SelectInk : Ink;
    const int ty = y + (h - 16) / 2;
    int valueW = 0;
    if (value && *value) valueW = font::measure(font::Face::Medium, value).width;
    textFit(c, x + 4, ty, label ? label : "", w - 12 - valueW, ink);
    if (valueW) font::draw(c, font::Face::Medium, x + w - 4 - valueW, ty, value, ink);
}

void scrollBar(gfx::Canvas& c, int x, int y, int h, int total, int visible, int first) {
    if (total <= visible || total <= 0) return;
    c.rect(x, y, 3, h, Grid, true);
    const int th = std::max(8, h * visible / total);
    const int ty = y + (h - th) * std::clamp(first, 0, total - visible) / std::max(1, total - visible);
    c.rect(x, ty, 3, th, Muted, true);
}

void panel(gfx::Canvas& c, gfx::Rect r, std::uint16_t fill, std::uint16_t border) {
    c.roundedRect(r.x, r.y, r.w, r.h, 4, fill, true);
    c.roundedRect(r.x, r.y, r.w, r.h, 4, border, false);
}

void menuPopup(gfx::Canvas& c, const char* title, const char* const* items, int count, int selected, int first, int visibleRows) {
    const int rows = std::min(count, visibleRows);
    const int h = 22 + rows * 18 + 4, w = 228, x = (ScreenW - w) / 2, y = std::max(ContentTop + 4, (ScreenH - SoftKeyH - h) / 2);
    c.rect(x + 3, y + 3, w, h, Faint, true);          // drop shadow
    panel(c, {x, y, w, h}, Bg, Ink);
    c.rect(x + 1, y + 1, w - 2, 19, KeyBg, true);
    font::drawClipped(c, font::Face::Small, x + 6, y + 4, title, KeyInk, w - 12);
    for (int i = 0; i < rows; ++i) {
        const int idx = first + i;
        if (idx >= count) break;
        const int ry = y + 22 + i * 18;
        const bool sel = idx == selected;
        if (sel) c.rect(x + 2, ry, w - 4, 18, Select, true);
        char num[4] = {0};
        if (idx < 9) { num[0] = static_cast<char>('1' + idx); num[1] = ':'; }
        font::draw(c, font::Face::Small, x + 6, ry + 3, num, sel ? SelectInk : Muted);
        textFit(c, x + 22, ry + 1, items[idx], w - 32, sel ? SelectInk : Ink);
    }
    scrollBar(c, x + w - 5, y + 22, rows * 18, count, rows, first);
}

void errorBox(gfx::Canvas& c, const char* title, const char* detail) {
    const int w = 260, h = 112, x = (ScreenW - w) / 2, y = 92;
    c.rect(x + 3, y + 3, w, h, Faint, true);
    panel(c, {x, y, w, h}, Bg, Ink);
    c.rect(x + 2, y + 2, w - 4, 2, Error, true);
    textCenter(c, ScreenW / 2, y + 12, title, Error);
    std::string lines[3];
    const std::size_t n = wrapText(detail ? detail : "", 30, lines, 3);
    for (std::size_t i = 0; i < n; ++i) textCenter(c, ScreenW / 2, y + 36 + static_cast<int>(i) * 14, lines[i].c_str(), Ink, font::Face::Small);
    textCenter(c, ScreenW / 2, y + h - 22, "Press:[EXIT]", Ink);
}

void toast(gfx::Canvas& c, const char* message) {
    const int w = std::min(ScreenW - 20, font::measure(font::Face::Small, message).width + 20), h = 20;
    const int x = (ScreenW - w) / 2, y = ScreenH - SoftKeyH - h - 6;
    c.roundedRect(x, y, w, h, 6, KeyBg, true);
    font::drawClipped(c, font::Face::Small, x + 10, y + 4, message, KeyInk, w - 20);
}

std::size_t wrapText(const std::string& s, std::size_t maxGlyphs, std::string* lines, std::size_t maxLines) {
    if (maxGlyphs == 0 || maxLines == 0) return 0;
    std::size_t n = 0;
    std::string current, word;
    std::size_t curG = 0, wordG = 0;
    auto flushLine = [&]() { if (n < maxLines) lines[n++] = current; current.clear(); curG = 0; };
    auto pushWord = [&]() {
        if (word.empty()) return;
        if (curG && curG + 1 + wordG > maxGlyphs) flushLine();
        if (n >= maxLines) return;
        if (curG) { current += ' '; ++curG; }
        // Hard-split words longer than a line.
        const auto* p = reinterpret_cast<const unsigned char*>(word.c_str());
        while (*p) {
            const unsigned char* st = p; (void)font::nextCodepoint(p);
            if (curG >= maxGlyphs) { flushLine(); if (n >= maxLines) return; }
            current.append(reinterpret_cast<const char*>(st), static_cast<std::size_t>(p - st)); ++curG;
        }
        word.clear(); wordG = 0;
    };
    const auto* p = reinterpret_cast<const unsigned char*>(s.c_str());
    while (*p) {
        const unsigned char* st = p; const std::uint32_t cp = font::nextCodepoint(p);
        if (cp == '\n') { pushWord(); flushLine(); if (n >= maxLines) return n; continue; }
        if (cp == ' ') { pushWord(); continue; }
        word.append(reinterpret_cast<const char*>(st), static_cast<std::size_t>(p - st)); ++wordG;
    }
    pushWord();
    if (!current.empty() && n < maxLines) lines[n++] = current;
    return n;
}
} // namespace ui
