#pragma once
#include "font_renderer.hpp"
#include "graphics_engine.hpp"
#include <cstdint>
#include <string>

// fx-CG50 inspired visual language adapted to the 320x320 PicoCalc panel:
// white "LCD" background, black ink, a light status bar at the top and a
// six-tab function-key menu at the bottom (F6 = Shift+F1 on the PicoCalc).
namespace ui {
namespace theme {
using gfx::rgb888To565;
constexpr std::uint16_t Bg = 0xFFFF;
constexpr std::uint16_t Ink = 0x0000;
constexpr std::uint16_t Muted = rgb888To565(110, 110, 110);
constexpr std::uint16_t Faint = rgb888To565(200, 200, 200);
constexpr std::uint16_t Grid = rgb888To565(225, 225, 225);
constexpr std::uint16_t StatusBg = rgb888To565(236, 238, 242);
constexpr std::uint16_t StatusInk = rgb888To565(40, 40, 48);
constexpr std::uint16_t StatusLine = rgb888To565(160, 166, 178);
constexpr std::uint16_t KeyBg = rgb888To565(24, 28, 36);
constexpr std::uint16_t KeyTop = rgb888To565(70, 78, 96);
constexpr std::uint16_t KeyInk = 0xFFFF;
constexpr std::uint16_t Select = rgb888To565(36, 104, 214);
constexpr std::uint16_t SelectInk = 0xFFFF;
constexpr std::uint16_t SelectSoft = rgb888To565(214, 230, 252);
constexpr std::uint16_t Accent = rgb888To565(0, 120, 215);
constexpr std::uint16_t Error = rgb888To565(200, 20, 30);
constexpr std::uint16_t Ok = rgb888To565(20, 140, 60);
constexpr std::uint16_t Warn = rgb888To565(230, 120, 0);
constexpr std::uint16_t Cursor = rgb888To565(0, 0, 0);
constexpr std::uint16_t Placeholder = rgb888To565(120, 120, 120);
constexpr std::uint16_t Shift = rgb888To565(230, 150, 0);
// Default graph colours (Y1..Y6), similar to the fx-CG50 defaults.
constexpr std::uint16_t Graph[6] = {
    rgb888To565(0, 70, 230), rgb888To565(220, 30, 30), rgb888To565(0, 150, 60),
    rgb888To565(200, 0, 200), rgb888To565(20, 20, 20), rgb888To565(0, 160, 200)};
} // namespace theme

constexpr int ScreenW = 320, ScreenH = 320;
constexpr int StatusH = 18;
constexpr int SoftKeyH = 20;
constexpr int ContentTop = StatusH;
constexpr int ContentBottom = ScreenH - SoftKeyH;       // with soft keys
constexpr int LineH = 16;                               // Medium face line height

struct StatusInfo {
    const char* title{};
    const char* angle{};
    const char* display{};
    const char* complex{};
    int battery{-1};
    bool charging{};
    bool shift{};
    bool wifi{};
    bool busy{};
};

void clear(gfx::Canvas& c, std::uint16_t color = theme::Bg);
void statusBar(gfx::Canvas& c, const StatusInfo& info);
// labels[0..5] for F1..F6; nullptr or "" leaves a tab empty.
void softKeys(gfx::Canvas& c, const char* const labels[6]);

int text(gfx::Canvas& c, int x, int y, const char* s, std::uint16_t color,
         font::Face face = font::Face::Medium, int maxWidth = 0);
int textRight(gfx::Canvas& c, int right, int y, const char* s, std::uint16_t color,
              font::Face face = font::Face::Medium);
int textCenter(gfx::Canvas& c, int cx, int y, const char* s, std::uint16_t color,
               font::Face face = font::Face::Medium);
// Text truncated with an ellipsis to fit maxWidth.
void textFit(gfx::Canvas& c, int x, int y, const std::string& s, int maxWidth, std::uint16_t color,
             font::Face face = font::Face::Medium);

// One list row: label at the left, optional value right-aligned.
void listRow(gfx::Canvas& c, int y, int h, const char* label, const char* value, bool selected,
             int x = 0, int w = ScreenW);
void scrollBar(gfx::Canvas& c, int x, int y, int h, int total, int visible, int first);
void panel(gfx::Canvas& c, gfx::Rect r, std::uint16_t fill, std::uint16_t border);

// Modal popup menu (OPTN catalogue, ZOOM, G-SOLVE...).
void menuPopup(gfx::Canvas& c, const char* title, const char* const* items, int count,
               int selected, int first, int visibleRows = 8);
// fx-CG50 style error box: "Math ERROR" + detail + "Press:[EXIT]".
void errorBox(gfx::Canvas& c, const char* title, const char* detail);
// Short notification banner above the soft keys.
void toast(gfx::Canvas& c, const char* message);

// Wraps UTF-8 text into lines of at most maxGlyphs glyphs (word boundaries when possible).
std::size_t wrapText(const std::string& s, std::size_t maxGlyphs, std::string* lines, std::size_t maxLines);
} // namespace ui
