// SYMBOLS: guide of the symbols and functions accepted by the editor and the
// parser (content in cheatsheet.hpp, also published as docs/SYMBOL_CHEATSHEET.md).
// v1.14.0: the constant pages are generated from the single constants table
// (physical_constants.hpp), one group per page; pages longer than the screen scroll.
#include "apps.hpp"
#include "cheatsheet.hpp"
#include <cstdio>
#include <cstring>
#include <string>

namespace fx::app {
using namespace ui::theme;

namespace {
// Numero con 15 cifre significative, esponente compatto: 6.02214076e23, 1.25663706127e-6.
std::string number(double v) {
    char buf[40];
    std::snprintf(buf, sizeof(buf), "%.15g", v);
    std::string s = buf;
    const std::size_t e = s.find('e');
    if (e == std::string::npos) return s;
    std::string exp = s.substr(e + 1);
    const bool negative = !exp.empty() && exp[0] == '-';
    if (!exp.empty() && (exp[0] == '-' || exp[0] == '+')) exp.erase(0, 1);
    while (exp.size() > 1 && exp[0] == '0') exp.erase(0, 1);
    return s.substr(0, e + 1) + (negative ? "-" : "") + exp;
}
// Riquadro di una costante: nome del parser e alias, valore con unita', incertezza, esempio del gruppo.
std::size_t constantLines(const help::Page& page, std::size_t i, std::string& head, std::string* lines) {
    const std::size_t group = static_cast<std::size_t>(page.constGroup);
    const physconst::ConstDef& k = physconst::constantsOf(group)[i];
    head = std::string("Nome: ") + k.parserName;
    physconst::forEachAlias(k, [&](std::string_view alias) { head += "   alias: "; head += alias; });
    lines[0] = "= " + number(k.value) + (k.unit[0] ? std::string(" ") + k.unit : std::string());
    if (group == 0) lines[1] = "Costante matematica";
    else if (physconst::isExact(k)) lines[1] = "Esatta (per definizione)";
    else lines[1] = "Incertezza relativa " + number(k.relUncertainty);
    lines[2] = std::string("Es.: ") + physconst::constGroups()[group].example;
    return 3;
}
} // namespace

void SymbolsApp::key(Context& ctx, const KeyEvent& ev) {
    const unsigned pages = static_cast<unsigned>(help::PageCount);
    const unsigned entries = static_cast<unsigned>(help::entryCount(help::pages[page_ % help::PageCount]));
    switch (ev.key) {
    case Key::Left: case Key::F1: page_ = (page_ + pages - 1) % pages; selected_ = 0; first_ = 0; break;
    case Key::Right: case Key::F2: page_ = (page_ + 1) % pages; selected_ = 0; first_ = 0; break;
    case Key::Up: if (entries) selected_ = (selected_ + entries - 1) % entries; break;
    case Key::Down: if (entries) selected_ = (selected_ + 1) % entries; break;
    case Key::Exit: case Key::F6: ctx.home(); break;
    default: break;
    }
    // La finestra di VisibleRows voci segue la selezione.
    if (selected_ < first_) first_ = selected_;
    if (selected_ >= first_ + help::VisibleRows) first_ = selected_ + 1 - help::VisibleRows;
}
void SymbolsApp::draw(Context& ctx, gfx::Canvas& c) {
    ui::clear(c);
    ctx.drawStatus(c, "SYMBOLS");
    const auto& data = help::pages[page_ % help::PageCount];
    const unsigned count = static_cast<unsigned>(help::entryCount(data));
    c.rect(0, ui::ContentTop, ui::ScreenW, 22, SelectSoft, true);
    char pg[16]; std::snprintf(pg, sizeof(pg), "%u/%u", page_ + 1, static_cast<unsigned>(help::PageCount));
    const int pgW = font::measure(font::Face::Small, pg).width;
    // Titoli lunghi ("COSTANTI FISICHE: ELETTROMAGNETICHE") nella faccia piccola.
    const char* title = help::pageTitle(data);
    const bool fits = font::measure(font::Face::Medium, title).width <= ui::ScreenW - 18 - pgW;
    if (fits) ui::text(c, 6, ui::ContentTop + 3, title, Ink);
    else ui::textFit(c, 6, ui::ContentTop + 5, title, ui::ScreenW - 18 - pgW, Ink, font::Face::Small);
    ui::textRight(c, ui::ScreenW - 6, ui::ContentTop + 5, pg, Muted, font::Face::Small);
    for (unsigned row = 0; row < help::VisibleRows && first_ + row < count; ++row) {
        const unsigned i = first_ + row;
        const int y = ui::ContentTop + 26 + static_cast<int>(row) * 26;
        const bool sel = i == selected_;
        if (sel) c.rect(2, y - 2, ui::ScreenW - 4, 24, Select, true);
        ui::textFit(c, 8, y + 2, help::entrySymbol(data, i), 100, sel ? SelectInk : Accent);
        ui::textFit(c, 112, y + 4, help::entryMeaning(data, i), ui::ScreenW - 124, sel ? SelectInk : Ink, font::Face::Small);
    }
    if (count > help::VisibleRows)
        ui::scrollBar(c, ui::ScreenW - 6, ui::ContentTop + 24, static_cast<int>(help::VisibleRows) * 26 - 2,
                      static_cast<int>(count), static_cast<int>(help::VisibleRows), static_cast<int>(first_));
    const int by = 202;
    ui::panel(c, {4, by, ui::ScreenW - 8, 74}, StatusBg, Faint);
    std::string head = "Esempio", lines[3];
    std::size_t n = 0;
    if (count) {
        const unsigned sel = selected_ % count;
        if (help::isConstantPage(data)) n = constantLines(data, sel, head, lines);
        else n = ui::wrapText(data.entries[sel].example, 37, lines, 3);
    }
    ui::textFit(c, 10, by + 4, head, ui::ScreenW - 20, Muted, font::Face::Small);
    for (std::size_t i = 0; i < n; ++i) ui::textFit(c, 10, by + 20 + static_cast<int>(i) * 16, lines[i], ui::ScreenW - 20, Ink);
    const char* keys[6] = {"\xE2\x97\x80PAG", "PAG\xE2\x96\xB6", nullptr, nullptr, nullptr, "EXIT"};
    ui::softKeys(c, keys);
}
} // namespace fx::app
