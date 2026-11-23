// v1.18.0: disegno condiviso di GRAPH e TABLE (vedi include/graph_view.hpp).
// Le funzioni sono quelle che GraphApp e TableApp usavano fino alla v1.17.0,
// spostate qui senza cambiare un pixel a schermo pieno, piu' la geometria dei
// due riquadri del Dual Screen.
#include "graph_view.hpp"
#include "ui_kit.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace fx::graphview {
using namespace ui::theme;

// ---------------------------------------------------------------- riquadri
graph::Viewport viewport(const ViewWindow& v, Pane p) noexcept {
    switch (p) {
    case Pane::Main: return graph::Viewport(v, graphgeo::Left, graphgeo::Top, graphgeo::Columns, graphgeo::SplitRows);
    case Pane::Sub: return graph::Viewport(v, graphgeo::Left, graphgeo::SubTop, graphgeo::Columns, graphgeo::SplitRows);
    case Pane::Full: break;
    }
    return graph::Viewport(v);
}
gfx::Rect paneRect(Pane p) noexcept {
    switch (p) {
    case Pane::Main: return {graphgeo::Left, graphgeo::Top, graphgeo::Columns, graphgeo::SplitRows};
    case Pane::Sub: return {graphgeo::Left, graphgeo::SubTop, graphgeo::Columns, graphgeo::SplitRows};
    case Pane::Full: break;
    }
    return {graphgeo::Left, graphgeo::Top, graphgeo::Columns, graphgeo::Rows};
}
void drawSeparator(gfx::Canvas& c) {
    c.hLine(graphgeo::Left, graphgeo::Top + graphgeo::SplitRows, graphgeo::Columns, Faint);
    c.hLine(graphgeo::Left, graphgeo::Top + graphgeo::SplitRows + 1, graphgeo::Columns, Muted);
}

// ---------------------------------------------------------------- colori, penne, tratti
std::uint16_t lineColor(std::uint8_t color) noexcept {
    static const std::uint16_t pal[GraphColorCount] = {Graph[0], Graph[1], Graph[2], Graph[3], Graph[4], Graph[5], rgb888To565(225, 180, 0)};
    return pal[color % GraphColorCount];
}
Pen penFor(LineStyle s, bool strict) noexcept {
    Pen p{2, 0, 0};
    switch (s) {
    case LineStyle::Thin: p = {1, 0, 0}; break;
    case LineStyle::Thick: p = {4, 0, 0}; break;
    case LineStyle::Broken: p = {3, 7, 4}; break;
    case LineStyle::Dot: p = {2, 2, 3}; break;
    default: break;
    }
    if (strict && p.off == 0) { p.on = 6; p.off = 4; }   // Y>, Y<: boundary not included
    return p;
}
void stamp(gfx::Canvas& c, int x, int y, int w, std::uint16_t col) {
    if (w <= 1) { c.pixel(x, y, col); return; }
    const int o = (w - 1) / 2;
    c.rect(x - o, y - o, w, w, col, true);
}
void segment(gfx::Canvas& c, int x0, int y0, int x1, int y1, const Pen& pen, std::uint16_t col, int& phase) {
    const int dx = std::abs(x1 - x0), dy = -std::abs(y1 - y0), sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (int guard = 0; guard < 8192; ++guard) {
        if (pen.off == 0 || phase % (pen.on + pen.off) < pen.on) stamp(c, x0, y0, pen.width, col);
        ++phase;
        if (x0 == x1 && y0 == y1) break;
        const int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}
void drawCurve(gfx::Canvas& c, const std::vector<std::int16_t>& xy, const Pen& pen, std::uint16_t col) {
    int phase = 0;
    bool have = false;
    int px = 0, py = 0;
    for (std::size_t i = 0; i + 1 < xy.size(); i += 2) {
        if (xy[i] == graph::PenUp) { have = false; continue; }
        const int x = xy[i], y = xy[i + 1];
        if (!have) {
            have = true; px = x; py = y;
            if (i + 2 >= xy.size() || xy[i + 2] == graph::PenUp) stamp(c, x, y, std::max(pen.width, 2), col);   // isolated point
            continue;
        }
        segment(c, px, py, x, y, pen, col, phase);
        px = x; py = y;
    }
}
void mathSegment(gfx::Canvas& c, const graph::Viewport& vp, double x0, double y0, double x1, double y1, const Pen& pen, std::uint16_t col) {
    double a = vp.screenX(x0), b = vp.screenY(y0), e = vp.screenX(x1), f = vp.screenY(y1);
    if (!vp.clip(a, b, e, f, 4)) return;
    int phase = 0;
    segment(c, static_cast<int>(std::lround(a)), static_cast<int>(std::lround(b)), static_cast<int>(std::lround(e)), static_cast<int>(std::lround(f)), pen, col, phase);
}
std::string shortNumber(double v) {
    char b[24];
    if (std::abs(v) < 1e-12) v = 0;
    std::snprintf(b, sizeof(b), "%.4g", v);
    return b;
}

// ---------------------------------------------------------------- cornice
void drawFrame(gfx::Canvas& c, const graph::Viewport& vp, const GraphSettings& gs) {
    const ViewWindow& v = vp.view();
    const int L = vp.left(), T = vp.top(), Cols = vp.columns(), Rows = vp.rows();
    const int R = L + Cols - 1, B = T + Rows - 1;
    // grid, drawn over the fills so the axes stay readable
    double fx = 0, fy = 0; int nx = 0, ny = 0;
    const bool xt = graph::tickRange(v.xmin, v.xmax, v.xscl, 200, fx, nx) && v.xscl / vp.xdot() >= 4;
    const bool yt = graph::tickRange(v.ymin, v.ymax, v.yscl, 200, fy, ny) && v.yscl / vp.ydot() >= 4;
    if (gs.grid == GridMode::On && xt && yt) {
        for (int i = 0; i < nx; ++i) for (int j = 0; j < ny; ++j)
            c.pixel(static_cast<int>(std::lround(vp.screenX(fx + i * v.xscl))), static_cast<int>(std::lround(vp.screenY(fy + j * v.yscl))), Muted);
    } else if (gs.grid == GridMode::Line) {
        if (xt) for (int i = 0; i < nx; ++i) c.vLine(static_cast<int>(std::lround(vp.screenX(fx + i * v.xscl))), T, Rows, Faint);
        if (yt) for (int j = 0; j < ny; ++j) c.hLine(L, static_cast<int>(std::lround(vp.screenY(fy + j * v.yscl))), Cols, Faint);
    }
    // axes, ticks, scale values and labels. An axis outside the window is
    // indicated on the edge closest to the origin (manual 5-6).
    if (gs.axes == AxesMode::Off) return;
    const bool xIn = v.ymin <= 0 && v.ymax >= 0, yIn = v.xmin <= 0 && v.xmax >= 0;
    const int ay = xIn ? static_cast<int>(std::lround(vp.screenY(0))) : (v.ymin > 0 ? B : T);
    const int ax = yIn ? static_cast<int>(std::lround(vp.screenX(0))) : (v.xmin > 0 ? L : R);
    if (xIn) c.hLine(L, ay, Cols, Ink);
    if (yIn) c.vLine(ax, T, Rows, Ink);
    if (v.xscl > 0 && graph::tickRange(v.xmin, v.xmax, v.xscl, 200, fx, nx) && v.xscl / vp.xdot() >= 3)
        for (int i = 0; i < nx; ++i) c.vLine(static_cast<int>(std::lround(vp.screenX(fx + i * v.xscl))), ay - 2, 5, Ink);
    if (v.yscl > 0 && graph::tickRange(v.ymin, v.ymax, v.yscl, 200, fy, ny) && v.yscl / vp.ydot() >= 3)
        for (int j = 0; j < ny; ++j) c.hLine(ax - 2, static_cast<int>(std::lround(vp.screenY(fy + j * v.yscl))), 5, Ink);
    if (gs.axes == AxesMode::Scale) {
        if (v.xscl > 0 && graph::tickRange(v.xmin, v.xmax, v.xscl, 200, fx, nx)) {
            const int stride = std::max(1, static_cast<int>(std::ceil(36.0 / (v.xscl / vp.xdot()))));
            const int ty = ay + 4 > B - 12 ? ay - 15 : ay + 4;
            for (int i = 0; i < nx; ++i) {
                const double x = fx + i * v.xscl;
                const long k = std::lround(x / v.xscl);
                if (k % stride != 0 || (yIn && k == 0)) continue;
                ui::textCenter(c, static_cast<int>(std::lround(vp.screenX(x))), ty, shortNumber(x).c_str(), Muted, font::Face::Small);
            }
        }
        if (v.yscl > 0 && graph::tickRange(v.ymin, v.ymax, v.yscl, 200, fy, ny)) {
            const int stride = std::max(1, static_cast<int>(std::ceil(14.0 / (v.yscl / vp.ydot()))));
            for (int j = 0; j < ny; ++j) {
                const double y = fy + j * v.yscl;
                const long k = std::lround(y / v.yscl);
                if (k % stride != 0 || (xIn && k == 0)) continue;
                const std::string s = shortNumber(y);
                const int sy = static_cast<int>(std::lround(vp.screenY(y))) - 6;
                if (ax + 4 + 40 < R) ui::text(c, ax + 4, sy, s.c_str(), Muted, font::Face::Small);
                else ui::textRight(c, ax - 4, sy, s.c_str(), Muted, font::Face::Small);
            }
        }
    }
    if (gs.label) {
        if (xIn) ui::text(c, R - 8, ay - 14, "x", Ink, font::Face::Small);
        if (yIn) ui::text(c, ax + 4, T + 1, "y", Ink, font::Face::Small);
        if (xIn && yIn) ui::text(c, ax - 8, ay + 2, "O", Ink, font::Face::Small);
    }
}

// ---------------------------------------------------------------- GPH-CON / GPH-PLT
void tableCurve(const graph::GraphEngine& engine, const graph::Plot& p, const std::vector<double>& rows,
                const graph::Viewport& vp, bool connect, graph::Curve& c) {
    c = graph::Curve{};
    c.done = true;
    bool have = false;
    double px = 0, py = 0;
    for (double v : rows) {
        double x, y;
        if (!table::point(engine, p, v, x, y)) { have = false; continue; }
        const double sx = std::clamp(vp.screenX(x), -1e6, 1e6), sy = std::clamp(vp.screenY(y), -1e6, 1e6);
        if (connect && have) {
            double a = px, b = py, e = sx, f = sy;
            if (vp.clip(a, b, e, f, 3)) {
                c.xy.push_back(graph::PenUp); c.xy.push_back(0);
                c.xy.push_back(static_cast<std::int16_t>(std::lround(a))); c.xy.push_back(static_cast<std::int16_t>(std::lround(b)));
                c.xy.push_back(static_cast<std::int16_t>(std::lround(e))); c.xy.push_back(static_cast<std::int16_t>(std::lround(f)));
            }
        }
        if (vp.inside(sx, sy)) {
            c.xy.push_back(graph::PenUp); c.xy.push_back(0);
            c.xy.push_back(static_cast<std::int16_t>(std::lround(sx))); c.xy.push_back(static_cast<std::int16_t>(std::lround(sy)));
        }
        px = sx; py = sy; have = true;
    }
}

// ---------------------------------------------------------------- tabella numerica
namespace {
constexpr int kVisibleCols = 4, kXW = 58, kRowH = 18;
std::string cellString(const TableData& d, int row, int column) {
    double v;
    return tableCell(d, row, column, v) ? table::cellText(v) : std::string("ERROR");
}
} // namespace

bool tableCell(const TableData& d, int row, int column, double& out) noexcept {
    const auto& rows = *d.rows;
    if (row < 0 || row >= static_cast<int>(rows.size())) return false;
    const double v = rows[static_cast<std::size_t>(row)];
    if (column < 0) { out = v; return true; }
    if (column >= static_cast<int>(d.columns->size())) return false;
    return table::cell(*d.engine, (*d.columns)[static_cast<std::size_t>(column)], v, out);
}
int tableVisibleRows(int top, int bottom) noexcept { return (bottom - top - 18 - 22) / kRowH; }

void drawTable(gfx::Canvas& c, int top, int bottom, const TableData& d, TableCursor& cur, bool active) {
    const auto& colsV = *d.columns;
    const int rows = static_cast<int>(d.rows->size()), cols = static_cast<int>(colsV.size());
    cur.row = std::clamp(cur.row, 0, std::max(0, rows - 1));
    cur.col = std::clamp(cur.col, -1, cols - 1);
    const int colW = (ui::ScreenW - kXW - 4) / kVisibleCols;
    if (cur.col >= 0 && cur.col < cur.firstCol) cur.firstCol = cur.col;
    if (cur.col >= cur.firstCol + kVisibleCols) cur.firstCol = cur.col - kVisibleCols + 1;
    cur.firstCol = std::clamp(cur.firstCol, 0, std::max(0, cols - kVisibleCols));
    const int visibleRows = tableVisibleRows(top, bottom);
    if (cur.row < cur.firstRow) cur.firstRow = cur.row;
    if (cur.row >= cur.firstRow + visibleRows) cur.firstRow = cur.row - visibleRows + 1;
    cur.firstRow = std::max(0, cur.firstRow);
    // header: variable, then the function columns with their colour
    c.rect(0, top, ui::ScreenW, 18, StatusBg, true);
    ui::textCenter(c, kXW / 2, top + 1, table::variableName(d.type), Ink);
    for (int k = 0; k < kVisibleCols && cur.firstCol + k < cols; ++k) {
        const table::Column& col = colsV[static_cast<std::size_t>(cur.firstCol + k)];
        const int x = kXW + 2 + k * colW;
        c.rect(x + 2, top + 5, 3, 9, lineColor(col.color), true);
        ui::textCenter(c, x + colW / 2 + 2, top + 1, col.title.c_str(), Ink);
    }
    for (int r = 0; r < visibleRows && cur.firstRow + r < rows; ++r) {
        const int rr = cur.firstRow + r, y = top + 18 + r * kRowH;
        const bool selRow = rr == cur.row;
        const bool selX = selRow && cur.col < 0;
        if (selX) { if (active) c.rect(1, y, kXW - 2, kRowH, Select, true); else c.rect(1, y, kXW - 2, kRowH, Select, false); }
        ui::textRight(c, kXW - 4, y + 1, cellString(d, rr, -1).c_str(), selX && active ? SelectInk : Ink);
        for (int k = 0; k < kVisibleCols && cur.firstCol + k < cols; ++k) {
            const int x = kXW + 2 + k * colW, ci = cur.firstCol + k;
            const bool sel = selRow && ci == cur.col;
            if (sel) { if (active) c.rect(x + 1, y, colW - 2, kRowH, Select, true); else c.rect(x + 1, y, colW - 2, kRowH, Select, false); }
            const std::string text = cellString(d, rr, ci);
            ui::textRight(c, x + colW - 4, y + 1, text.c_str(), sel && active ? SelectInk : text == "ERROR" ? Error : lineColor(colsV[static_cast<std::size_t>(ci)].color));
        }
        c.hLine(0, y + kRowH - 1, ui::ScreenW, Grid);
    }
    c.vLine(kXW, top, 18 + std::min(visibleRows, rows) * kRowH, Faint);
    ui::scrollBar(c, ui::ScreenW - 3, top + 18, visibleRows * kRowH, rows, visibleRows, cur.firstRow);
    // full value of the selected cell (the cells show at most 6 characters)
    double v = 0;
    std::string full;
    if (cur.col < 0) full = std::string(table::variableName(d.type)) + "=";
    else {
        const table::Column& col = colsV[static_cast<std::size_t>(cur.col)];
        full = (col.kind == table::Column::Kind::Derivative ? std::string(table::derivativeName(d.type)) : col.title) + "=";
    }
    full += rows > 0 && tableCell(d, cur.row, cur.col, v) ? formatReal(v, d.display) : std::string(rows > 0 ? "ERROR" : "");
    c.hLine(0, bottom - 22, ui::ScreenW, Faint);
    ui::textFit(c, 6, bottom - 19, full, ui::ScreenW - 12, Ink);
}

} // namespace fx::graphview
