// SYMBOLS: guide of the symbols and functions accepted by the editor and the
// parser (content in cheatsheet.hpp, also published as docs/SYMBOL_CHEATSHEET.md).
#include "apps.hpp"
#include "cheatsheet.hpp"
#include <cstdio>

namespace fx::app {
using namespace ui::theme;

void SymbolsApp::key(Context& ctx, const KeyEvent& ev) {
    const unsigned pages = static_cast<unsigned>(help::PageCount);
    const unsigned entries = help::pages[page_].count;
    switch (ev.key) {
    case Key::Left: page_ = (page_ + pages - 1) % pages; selected_ = 0; break;
    case Key::Right: page_ = (page_ + 1) % pages; selected_ = 0; break;
    case Key::Up: if (entries) selected_ = (selected_ + entries - 1) % entries; break;
    case Key::Down: if (entries) selected_ = (selected_ + 1) % entries; break;
    case Key::F1: page_ = (page_ + pages - 1) % pages; selected_ = 0; break;
    case Key::F2: page_ = (page_ + 1) % pages; selected_ = 0; break;
    case Key::Exit: case Key::F6: ctx.home(); break;
    default: break;
    }
}
void SymbolsApp::draw(Context& ctx, gfx::Canvas& c) {
    ui::clear(c);
    ctx.drawStatus(c, "SYMBOLS");
    const auto& data = help::pages[page_ % help::PageCount];
    c.rect(0, ui::ContentTop, ui::ScreenW, 22, SelectSoft, true);
    ui::text(c, 6, ui::ContentTop + 3, data.title, Ink);
    char pg[16]; std::snprintf(pg, sizeof(pg), "%u/%u", page_ + 1, static_cast<unsigned>(help::PageCount));
    ui::textRight(c, ui::ScreenW - 6, ui::ContentTop + 5, pg, Muted, font::Face::Small);
    for (unsigned i = 0; i < data.count; ++i) {
        const int y = ui::ContentTop + 26 + static_cast<int>(i) * 26;
        const bool sel = i == selected_;
        if (sel) c.rect(2, y - 2, ui::ScreenW - 4, 24, Select, true);
        ui::textFit(c, 8, y + 2, data.entries[i].symbol, 100, sel ? SelectInk : Accent);
        ui::textFit(c, 112, y + 4, data.entries[i].meaning, ui::ScreenW - 118, sel ? SelectInk : Ink, font::Face::Small);
    }
    const int by = 202;
    ui::panel(c, {4, by, ui::ScreenW - 8, 74}, StatusBg, Faint);
    ui::text(c, 10, by + 4, "Esempio", Muted, font::Face::Small);
    std::string lines[3];
    const std::size_t n = ui::wrapText(data.entries[selected_ % data.count].example, 37, lines, 3);
    for (std::size_t i = 0; i < n; ++i) ui::text(c, 10, by + 20 + static_cast<int>(i) * 16, lines[i].c_str(), Ink);
    const char* keys[6] = {"\xE2\x97\x80PAG", "PAG\xE2\x96\xB6", nullptr, nullptr, nullptr, "EXIT"};
    ui::softKeys(c, keys);
}
} // namespace fx::app
