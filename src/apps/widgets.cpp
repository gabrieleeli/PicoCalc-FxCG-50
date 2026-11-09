#include "app_widgets.hpp"
#include "expression.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace fx::app {
using namespace ui::theme;

double evalNumber(Context& ctx, const std::string& text) {
    if (text.empty()) throw SyntaxError("Syntax ERROR: valore mancante");
    ctx.ws.apply();
    EvalContext ec; ec.memory = &ctx.ws.memory;
    return evaluateReal(*parseExpression(text), ec);
}
std::string fmt(Context& ctx, double v) { return formatReal(v, ctx.ws.settings.display); }
files::FileSystem* userFiles(Context& ctx) {
    return ctx.ws.settings.fileStore == Settings::FileStore::Internal ? ctx.platform.internalFiles() : ctx.platform.files();
}

// ------------------------------------------------------------------ Form
bool Form::key(Context& ctx, const KeyEvent& ev) {
    if (fields.empty()) return false;
    FormField& f = fields[static_cast<std::size_t>(selected)];
    if (editing_) {
        if (ev.is(Key::Exe)) {
            const std::string t = editor_.text();
            if (f.type == FormField::Type::Number) (void)evalNumber(ctx, t);   // throws on invalid input
            f.text = t; editing_ = false;
            selected = std::min(selected + 1, static_cast<int>(fields.size()) - 1);
            return true;
        }
        if (ev.is(Key::Exit)) { editing_ = false; return true; }
        if (ev.is(Key::Up) || ev.is(Key::Down)) {
            if (f.type == FormField::Type::Number && !editor_.empty()) (void)evalNumber(ctx, editor_.text());
            f.text = editor_.text(); editing_ = false;
            // fall through to the selection move below
        } else {
            return editor_.key(ev);
        }
    }
    const int n = static_cast<int>(fields.size());
    switch (ev.key) {
    case Key::Up: selected = (selected + n - 1) % n; return true;
    case Key::Down: selected = (selected + 1) % n; return true;
    case Key::Left: case Key::Right:
        if (f.type == FormField::Type::Choice && !f.choices.empty()) {
            const int k = static_cast<int>(f.choices.size());
            f.choice = (f.choice + (ev.is(Key::Left) ? k - 1 : 1)) % k;
            f.text = f.choices[static_cast<std::size_t>(f.choice)];
            return true;
        }
        return false;
    case Key::Exe:
        if (f.type == FormField::Type::Choice) return false;
        editor_.set(f.text); editing_ = true; return true;
    case Key::Char:
        if (ev.ctrl || f.type == FormField::Type::Choice) return false;
        editor_.clear(); editor_.insert(std::string(1, ev.ch)); editing_ = true; return true;
    case Key::Del: case Key::AC:
        if (f.type == FormField::Type::Choice) return false;
        editor_.clear(); editing_ = true; return true;
    default: return false;
    }
}
void Form::draw(gfx::Canvas& c, int top, int bottom) const {
    const int rowH = 20;
    const int visible = std::max(1, (bottom - top) / rowH);
    if (selected < first_) first_ = selected;
    if (selected >= first_ + visible) first_ = selected - visible + 1;
    for (int i = 0; i < visible; ++i) {
        const int idx = first_ + i;
        if (idx >= static_cast<int>(fields.size())) break;
        const FormField& f = fields[static_cast<std::size_t>(idx)];
        const int y = top + i * rowH;
        const bool sel = idx == selected;
        if (sel) c.rect(0, y, ui::ScreenW, rowH, editing_ ? SelectSoft : Select, true);
        const std::uint16_t ink = sel && !editing_ ? SelectInk : Ink;
        ui::textFit(c, 6, y + 2, f.label, 112, ink);
        if (sel && editing_) { editor_.draw(c, 122, y + 2, ui::ScreenW - 130, Ink, true, f.type == FormField::Type::Secret); continue; }
        std::string value = f.text;
        if (f.type == FormField::Type::Secret) value.assign(value.size(), '*');
        if (f.type == FormField::Type::Choice) value = "\xE2\x97\x80 " + value + " \xE2\x96\xB6";
        ui::textFit(c, 122, y + 2, value, ui::ScreenW - 130, ink);
    }
    ui::scrollBar(c, ui::ScreenW - 4, top, visible * rowH, static_cast<int>(fields.size()), visible, first_);
}

// ------------------------------------------------------------------ ResultList
bool ResultList::key(const KeyEvent& ev) {
    const int n = static_cast<int>(rows.size());
    if (ev.is(Key::Up)) { first_ = std::max(0, first_ - 1); return true; }
    if (ev.is(Key::Down)) { first_ = std::min(std::max(0, n - 1), first_ + 1); return true; }
    if (ev.is(Key::PageUp)) { first_ = std::max(0, first_ - 8); return true; }
    if (ev.is(Key::PageDown)) { first_ = std::min(std::max(0, n - 1), first_ + 8); return true; }
    return false;
}
void ResultList::draw(gfx::Canvas& c, int top, int bottom) const {
    int y = top;
    if (!title.empty()) {
        c.rect(0, y, ui::ScreenW, 20, SelectSoft, true);
        ui::textFit(c, 6, y + 2, title, ui::ScreenW - 12, Ink);
        y += 22;
    }
    const int rowH = 18, visible = std::max(1, (bottom - y) / rowH);
    const int n = static_cast<int>(rows.size());
    const int first = std::min(first_, std::max(0, n - visible));
    for (int i = 0; i < visible && first + i < n; ++i) {
        const auto& r = rows[static_cast<std::size_t>(first + i)];
        const int ry = y + i * rowH;
        ui::textFit(c, 6, ry + 1, r.first, 110, Accent);
        ui::textFit(c, 118, ry + 1, r.second, ui::ScreenW - 124, Ink);
        c.hLine(4, ry + rowH - 1, ui::ScreenW - 8, Grid);
    }
    ui::scrollBar(c, ui::ScreenW - 4, y, visible * rowH, n, visible, first);
}

// ------------------------------------------------------------------ CellGrid
bool CellGrid::key(Context& ctx, const KeyEvent& ev) {
    if (editing_) {
        if (ev.is(Key::Exe) || ev.is(Key::Down) || ev.is(Key::Right)) {
            if (editor_.empty()) { editing_ = false; return true; }
            const double v = evalNumber(ctx, editor_.text());
            set(row, col, v);
            editing_ = false;
            if (ev.is(Key::Right) || (rowMajor && ev.is(Key::Exe))) {
                if (col + 1 < cols) ++col;
                else if (rowMajor && row + 1 < rows) { col = 0; ++row; }
            } else if (row + 1 < rows || growRows) ++row;
            if (row >= rows) rows = row + 1;
            return true;
        }
        if (ev.is(Key::Exit)) { editing_ = false; return true; }
        return editor_.key(ev);
    }
    auto maxRow = [&]() { return columnLength ? std::min(columnLength(col), rows - 1 + (growRows ? 1 : 0)) : rows - 1 + (growRows ? 1 : 0); };
    switch (ev.key) {
    case Key::Up: row = std::max(0, row - 1); return true;
    case Key::Down: row = std::min(row + 1, std::max(0, maxRow())); return true;
    case Key::Left: col = std::max(0, col - 1); row = std::min(row, std::max(0, maxRow())); return true;
    case Key::Right: col = std::min(cols - 1, col + 1); row = std::min(row, std::max(0, maxRow())); return true;
    case Key::PageUp: row = std::max(0, row - 10); return true;
    case Key::PageDown: row = std::min(std::max(0, maxRow()), row + 10); return true;
    case Key::Home: row = 0; return true;
    case Key::End: row = std::max(0, maxRow()); return true;
    case Key::Exe: {
        double v = 0;
        editor_.clear();
        if (get(row, col, v)) editor_.set(formatReal(v, {DisplayMode::Norm2, 0}));
        editing_ = true; return true;
    }
    case Key::Char:
        if (ev.ctrl) return false;
        editor_.clear(); editor_.insert(std::string(1, ev.ch)); editing_ = true; return true;
    case Key::Del: case Key::DelForward: if (erase) erase(row, col); return true;
    default: return false;
    }
}
void CellGrid::draw(Context& ctx, gfx::Canvas& c, int top, int bottom) const {
    const int labelW = growRows ? 26 : 0;
    const int colW = (ui::ScreenW - labelW - 4) / std::max(1, visibleCols);
    const int headerH = 18, rowH = 18, editH = 22;
    const int visibleRows = std::max(1, (bottom - top - headerH - editH) / rowH);
    if (col < firstCol_) firstCol_ = col;
    if (col >= firstCol_ + visibleCols) firstCol_ = col - visibleCols + 1;
    if (row < firstRow_) firstRow_ = row;
    if (row >= firstRow_ + visibleRows) firstRow_ = row - visibleRows + 1;
    c.rect(0, top, ui::ScreenW, headerH, StatusBg, true);
    for (int k = 0; k < visibleCols && firstCol_ + k < cols; ++k) {
        const int x = labelW + k * colW;
        ui::textCenter(c, x + colW / 2, top + 3, header ? header(firstCol_ + k).c_str() : "", Ink, font::Face::Small);
        c.vLine(x, top, headerH + (growRows ? visibleRows : std::min(visibleRows, rows)) * rowH, Faint);
    }
    for (int r = 0; r < visibleRows; ++r) {
        const int rr = firstRow_ + r, y = top + headerH + r * rowH;
        if (!growRows && rr >= rows) break;          // fixed-size grids: no empty ruled rows
        if (growRows) { char lbl[16]; std::snprintf(lbl, sizeof(lbl), "%d", rr + 1); ui::textRight(c, labelW - 3, y + 3, lbl, Muted, font::Face::Small); }
        for (int k = 0; k < visibleCols && firstCol_ + k < cols; ++k) {
            const int cc = firstCol_ + k, x = labelW + k * colW;
            const bool sel = rr == row && cc == col;
            if (sel) c.rect(x + 1, y, colW - 1, rowH, editing_ ? SelectSoft : Select, true);
            double v = 0;
            if (rr < rows && get && get(rr, cc, v)) {
                const std::string s = formatReal(v, ctx.ws.settings.display);
                const int maxGlyphs = (colW - 4) / 6;
                std::string shown = s;
                if (static_cast<int>(font::glyphCount(s.c_str())) > maxGlyphs) shown = formatReal(v, {DisplayMode::Sci, static_cast<std::uint8_t>(std::max(1, maxGlyphs - 5))});
                ui::textRight(c, x + colW - 3, y + 3, shown.c_str(), sel && !editing_ ? SelectInk : Ink, font::Face::Small);
            }
        }
        c.hLine(labelW, y + rowH - 1, ui::ScreenW - labelW, Grid);
    }
    // Bottom line: full value of the current cell, or the edit line.
    const int ey = bottom - editH + 3;
    c.hLine(0, bottom - editH, ui::ScreenW, Faint);
    if (editing_) editor_.draw(c, 6, ey, ui::ScreenW - 12, Ink);
    else {
        double v = 0;
        if (row < rows && get && get(row, col, v)) ui::text(c, 6, ey, formatReal(v, ctx.ws.settings.display).c_str(), Muted);
    }
}
} // namespace fx::app
