// TABLE: number table of the Y= functions of the relation list shared with
// GRAPH (Y1..Y20) over Start/End/Step, with dY/dX columns when the Setup item
// Derivative is On (manual 5-34). Values come from the GRAPH engine (the same
// evaluator as the graphs). GPH-CON / GPH-PLT ask GRAPH to draw the table.
#include "apps.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace fx::app {
using namespace ui::theme;

namespace {
constexpr int MaxRows = 500;
std::uint16_t lineColor(std::uint8_t c) {
    static const std::uint16_t pal[GraphColorCount] = {Graph[0], Graph[1], Graph[2], Graph[3], Graph[4], Graph[5], rgb888To565(225, 180, 0)};
    return pal[c % GraphColorCount];
}
} // namespace

void TableApp::enter(Context& ctx) { setup_ = false; rebuild(ctx); }
void TableApp::leave(Context& ctx) { (void)ctx; xs_.clear(); xs_.shrink_to_fit(); }

void TableApp::rebuild(Context& ctx) {
    const TableRange& t = ctx.ws.table;
    if (!(t.step > 0) || t.end < t.start) throw MathError("Argument ERROR: Start<=End e Step>0");
    const double count = std::floor((t.end - t.start) / t.step + 1e-9) + 1;
    if (count > MaxRows) throw MathError("Argument ERROR: massimo 500 righe (aumenta Step)");
    xs_.clear();
    for (int i = 0; i < static_cast<int>(count); ++i) xs_.push_back(t.start + i * t.step);
    ctx.ws.apply();
    engine_.build({&ctx.ws.functions, &ctx.ws.memory, &ctx.ws, &ctx.ws.lists});
    cols_.clear();
    const auto& plots = engine_.plots();
    for (int i = 0; i < static_cast<int>(plots.size()); ++i) {
        const auto& p = plots[static_cast<std::size_t>(i)];
        if (!isYType(p.type) || p.variant != 0 || p.pasted) continue;
        cols_.push_back({i, false});
        if (ctx.ws.graph.settings.derivative) cols_.push_back({i, true});
    }
    row_ = std::min(row_, std::max(0, static_cast<int>(xs_.size()) - 1));
    col_ = std::min(col_, std::max(0, static_cast<int>(cols_.size()) - 1));
}

bool TableApp::cell(int row, int column, double& out) const {
    if (row < 0 || row >= static_cast<int>(xs_.size()) || column < 0 || column >= static_cast<int>(cols_.size())) return false;
    const Column& c = cols_[static_cast<std::size_t>(column)];
    const auto& p = engine_.plots()[static_cast<std::size_t>(c.plot)];
    const double x = xs_[static_cast<std::size_t>(row)];
    return c.derivative ? engine_.derivative(p, x, out) : engine_.eval(p, x, out);
}

void TableApp::key(Context& ctx, const KeyEvent& ev) {
    if (setup_) {
        if (form_.editing()) { (void)form_.key(ctx, ev); return; }
        if (ev.is(Key::Exit) || ev.is(Key::F6) || ev.is(Key::F1)) {
            const TableRange t{form_.number(ctx, 0), form_.number(ctx, 1), form_.number(ctx, 2)};
            if (!(t.step > 0) || t.end < t.start) throw MathError("Argument ERROR: Start<=End e Step>0");
            ctx.ws.table.start = t.start; ctx.ws.table.end = t.end; ctx.ws.table.step = t.step;
            ctx.markDirty(); ctx.ws.graph.projectModified = true;
            setup_ = false; rebuild(ctx); return;
        }
        (void)form_.key(ctx, ev);
        return;
    }
    const int n = static_cast<int>(xs_.size());
    switch (ev.key) {
    case Key::Up: row_ = std::max(0, row_ - 1); return;
    case Key::Down: row_ = std::min(n - 1, row_ + 1); return;
    case Key::PageUp: row_ = std::max(0, row_ - 12); return;
    case Key::PageDown: row_ = std::min(n - 1, row_ + 12); return;
    case Key::Left: col_ = std::max(0, col_ - 1); return;
    case Key::Right: col_ = std::min(std::max(0, static_cast<int>(cols_.size()) - 1), col_ + 1); return;
    case Key::F1: {
        const TableRange& t = ctx.ws.table;
        auto f = [](const char* l, double v) { return FormField{l, formatReal(v, {DisplayMode::Norm2, 0}), FormField::Type::Number, {}, 0}; };
        form_.fields = {f("Start", t.start), f("End", t.end), f("Step", t.step)};
        form_.selected = 0; setup_ = true; return;
    }
    case Key::F4: ctx.open(AppId::Graph); return;
    case Key::F5: case Key::F6:
        // GPH-CON / GPH-PLT: the table points drawn by GRAPH (manual 5-35).
        if (cols_.empty()) throw MathError("Math ERROR: nessuna funzione Y= selezionata");
        ctx.ws.table.draw = ev.is(Key::F5) ? TableRange::Draw::Connect : TableRange::Draw::Plot;
        ctx.open(AppId::Graph);
        return;
    case Key::Exit: ctx.home(); return;
    default: return;
    }
}

void TableApp::draw(Context& ctx, gfx::Canvas& c) {
    ui::clear(c);
    ctx.drawStatus(c, "TABLE");
    const int top = ui::ContentTop + 1, bottom = ui::ContentBottom;
    if (setup_) {
        c.rect(0, top, ui::ScreenW, 20, SelectSoft, true);
        ui::text(c, 6, top + 2, "Table Setting (X)", Ink);
        form_.draw(c, top + 22, top + 22 + 60);
        const char* keys[6] = {"OK", nullptr, nullptr, nullptr, nullptr, "OK"};
        ui::softKeys(c, keys);
        return;
    }
    if (cols_.empty()) {
        ui::textCenter(c, ui::ScreenW / 2, 120, "Nessuna funzione Y= selezionata", Ink);
        ui::textCenter(c, ui::ScreenW / 2, 144, "Inseriscile nella modalita' GRAPH", Muted, font::Face::Small);
        const char* keys[6] = {"SET", nullptr, nullptr, "GRAPH", nullptr, nullptr};
        ui::softKeys(c, keys);
        return;
    }
    const auto& plots = engine_.plots();
    const int visibleCols = 3, xW = 26, colW = (ui::ScreenW - 2 * xW - 6) / visibleCols, rowH = 18;
    const int firstCol = std::clamp(col_ - visibleCols + 1, 0, std::max(0, static_cast<int>(cols_.size()) - visibleCols));
    const int visibleRows = (bottom - top - 18 - 22) / rowH;
    if (row_ < firstRow_) firstRow_ = row_;
    if (row_ >= firstRow_ + visibleRows) firstRow_ = row_ - visibleRows + 1;
    c.rect(0, top, ui::ScreenW, 18, StatusBg, true);
    ui::textCenter(c, xW + 1, top + 3, "X", Ink, font::Face::Small);
    for (int k = 0; k < visibleCols && firstCol + k < static_cast<int>(cols_.size()); ++k) {
        const Column& col = cols_[static_cast<std::size_t>(firstCol + k)];
        const auto& p = plots[static_cast<std::size_t>(col.plot)];
        const std::string h = col.derivative ? p.label + "'" : p.label;
        const int x = 2 * xW + 4 + k * colW;
        c.rect(x + 2, top + 5, 4, 8, lineColor(p.color), true);
        ui::textCenter(c, x + colW / 2, top + 3, h.c_str(), Ink, font::Face::Small);
    }
    const DisplayFormat cellFmt{DisplayMode::Sci, 6};
    for (int r = 0; r < visibleRows && firstRow_ + r < static_cast<int>(xs_.size()); ++r) {
        const int rr = firstRow_ + r, y = top + 18 + r * rowH;
        const double x = xs_[static_cast<std::size_t>(rr)];
        const std::string xs = formatReal(x, ctx.ws.settings.display);
        ui::textRight(c, 2 * xW, y + 3, (font::glyphCount(xs.c_str()) > 8 ? formatReal(x, {DisplayMode::Sci, 3}) : xs).c_str(), Ink, font::Face::Small);
        for (int k = 0; k < visibleCols && firstCol + k < static_cast<int>(cols_.size()); ++k) {
            const int cx = 2 * xW + 4 + k * colW;
            const bool sel = rr == row_ && firstCol + k == col_;
            if (sel) c.rect(cx + 1, y, colW - 2, rowH, Select, true);
            double v;
            std::string text = "ERROR";
            if (cell(rr, firstCol + k, v)) {
                text = formatReal(v, ctx.ws.settings.display);
                if (static_cast<int>(font::glyphCount(text.c_str())) * 6 > colW - 6) text = formatReal(v, cellFmt);
            }
            const auto& p = plots[static_cast<std::size_t>(cols_[static_cast<std::size_t>(firstCol + k)].plot)];
            ui::textRight(c, cx + colW - 4, y + 3, text.c_str(), sel ? SelectInk : lineColor(p.color), font::Face::Small);
        }
        c.hLine(0, y + rowH - 1, ui::ScreenW, Grid);
    }
    c.vLine(2 * xW + 3, top, 18 + visibleRows * rowH, Faint);
    // Full value of the selected cell ("dY/dX" for derivative columns).
    double v;
    const Column& col = cols_[static_cast<std::size_t>(col_)];
    const auto& p = plots[static_cast<std::size_t>(col.plot)];
    std::string full = "X=" + formatReal(xs_[static_cast<std::size_t>(row_)], ctx.ws.settings.display) + "  " + (col.derivative ? std::string("dY/dX") : p.label) + "=";
    full += cell(row_, col_, v) ? formatReal(v, ctx.ws.settings.display) : std::string("--");
    c.hLine(0, bottom - 22, ui::ScreenW, Faint);
    ui::textFit(c, 6, bottom - 19, full, ui::ScreenW - 12, Ink);
    const char* keys[6] = {"SET", nullptr, nullptr, "GRAPH", "GPH-CON", "GPH-PLT"};
    ui::softKeys(c, keys);
}
} // namespace fx::app
