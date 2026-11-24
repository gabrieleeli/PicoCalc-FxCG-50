// GEOMETRY (v1.19.0): disegno della costruzione (assi, griglia, sfondo, figure
// con colori e stili delle linee, aree, etichette, misure incollate, tracce),
// della casella di misura, delle anteprime degli strumenti e delle schermate
// Key Help, Edit Animation e tabella. Grafica propria del progetto.
#include "apps.hpp"
#include "font_renderer.hpp"
#include "geometry.hpp"
#include "geometry_ui.hpp"
#include "pict.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace fx::app {
using namespace ui::theme;
namespace geoui {
namespace {
using gfx::rgb888To565;
constexpr std::uint16_t kPalette[geo::ColorCount] = {rgb888To565(20, 20, 20), rgb888To565(0, 70, 230), rgb888To565(220, 30, 30), rgb888To565(200, 0, 200),
                                                     rgb888To565(0, 150, 60), rgb888To565(0, 160, 200), rgb888To565(225, 170, 0)};
const char* const kPaletteNames[geo::ColorCount] = {"Black", "Blue", "Red", "Magenta", "Green", "Cyan", "Yellow"};
const char* const kHelp1[] = {"Tasti della zona di disegno (1/3)", "Frecce: puntatore (SHIFT: passo x4)", "1-9: salto al settore dello schermo",
                              "EXE: seleziona / deseleziona", "AC: Deselect All, ferma l'animazione", "DEL: Delete della selezione",
                              "x: Select Figure    *: Undo/Redo", "v: sposta la selezione (EXE posa)", "   senza selezione: Pan"};
const char* const kHelp2[] = {"Menu e viste (2/3)", "F1 File/View  F2 Edit/Option/Properties", "F3 Draw/Draw Spec  F4 Construct",
                              "F5 Transform  F6 Animate", "nei menu: ◀ ▶ schede, 1-9 voce", "+ / -: Zoom In / Zoom Out   .: Scroll",
                              "SHIFT F3: V-Window", "SHIFT MENU: SET UP (Option)", "SHIFT 5: FORMAT (colori, linee)"};
const char* const kHelp3[] = {"Casella di misura (3/3)", "OPTN o EXIT: apre / chiude la casella", "◀ ▶ o F1 TYPE: tipo di misura",
                              "cifre + EXE: valore (blocca la misura)", "F2 LOCK: blocca / sblocca", "F3 PASTE: misura sullo schermo",
                              "F4 TABLE: colonna della tabella", "F5 LABEL: etichetta", "moduli: EXIT conferma, AC annulla"};
template <std::size_t N> constexpr int countOf(const char* const (&)[N]) { return static_cast<int>(N); }

struct Painter {
    Context& ctx;
    gfx::Canvas& c;
    const geo::State& s;
    int bottom;
    void map(double x, double y, double& sx, double& sy) const {
        geo::toScreen(s.view, ui::ScreenW, AreaH, x, y, sx, sy);
        sy += AreaTop;
    }
    // Liang-Barsky sul riquadro di disegno.
    bool clip(double& x0, double& y0, double& x1, double& y1) const {
        const double xmin = -2, xmax = ui::ScreenW + 2, ymin = AreaTop - 2, ymax = bottom + 2;
        double t0 = 0, t1 = 1;
        const double dx = x1 - x0, dy = y1 - y0;
        const double p[4] = {-dx, dx, -dy, dy}, q[4] = {x0 - xmin, xmax - x0, y0 - ymin, ymax - y0};
        for (int i = 0; i < 4; ++i) {
            if (p[i] == 0) { if (q[i] < 0) return false; continue; }
            const double r = q[i] / p[i];
            if (p[i] < 0) { if (r > t1) return false; t0 = std::max(t0, r); }
            else { if (r < t0) return false; t1 = std::min(t1, r); }
        }
        const double ax = x0 + t0 * dx, ay = y0 + t0 * dy, bx = x0 + t1 * dx, by = y0 + t1 * dy;
        x0 = ax; y0 = ay; x1 = bx; y1 = by;
        return std::isfinite(x0) && std::isfinite(y0) && std::isfinite(x1) && std::isfinite(y1);
    }
    void line(double x0, double y0, double x1, double y1, std::uint16_t col, geo::LineStyle ls, bool selected) {
        if (!clip(x0, y0, x1, y1)) return;
        const int a = static_cast<int>(std::lround(x0)), b = static_cast<int>(std::lround(y0)), e = static_cast<int>(std::lround(x1)), f = static_cast<int>(std::lround(y1));
        if (selected) for (int k = -2; k <= 2; ++k) { c.line(a + k, b, e + k, f, SelectSoft); c.line(a, b + k, e, f + k, SelectSoft); }
        switch (ls) {
        case geo::LineStyle::Thick:
            for (int k = -1; k <= 1; ++k) { c.line(a + k, b, e + k, f, col); c.line(a, b + k, e, f + k, col); }
            break;
        case geo::LineStyle::Thin: c.line(a, b, e, f, gfx::Canvas::blend(col, Bg, 110)); break;
        case geo::LineStyle::Broken: c.dashedLine(a, b, e, f, col, 6, 4); break;
        case geo::LineStyle::Dot: c.dashedLine(a, b, e, f, col, 1, 3); break;
        default: c.line(a, b, e, f, col); break;
        }
        if (selected && ls != geo::LineStyle::Thick) { c.line(a + 1, b, e + 1, f, col); c.line(a, b + 1, e, f + 1, col); }
    }
    void planeLine(double x0, double y0, double x1, double y1, std::uint16_t col, geo::LineStyle ls, bool sel) {
        double a, b, e, f;
        map(x0, y0, a, b);
        map(x1, y1, e, f);
        line(a, b, e, f, col, ls, sel);
    }
    // Retta, semiretta, segmento o vettore dai due punti della forma.
    void lineLike(const geo::Object& o, std::uint16_t col, bool sel) {
        const auto& g = o.g.a;
        double x0 = g[0], y0 = g[1], x1 = g[2], y1 = g[3];
        if (o.kind == geo::Kind::Line || o.kind == geo::Kind::Ray) {
            const double dx = x1 - x0, dy = y1 - y0, len = std::hypot(dx, dy);
            const double far = 4.0 * (ui::ScreenW + AreaH) / s.view.scale + std::hypot(x0 - s.view.cx, y0 - s.view.cy);
            const double ex = x0 + dx / len * far, ey = y0 + dy / len * far;
            if (o.kind == geo::Kind::Line) { x0 -= dx / len * far; y0 -= dy / len * far; }
            x1 = ex; y1 = ey;
        }
        planeLine(x0, y0, x1, y1, col, o.style.lineStyle, sel);
        if (o.kind == geo::Kind::Vector) {
            double bx, by, ax, ay;
            map(g[2], g[3], bx, by);
            map(g[0], g[1], ax, ay);
            const double ang = std::atan2(by - ay, bx - ax);
            for (double d : {2.6, -2.6}) line(bx, by, bx + 9 * std::cos(ang + d), by + 9 * std::sin(ang + d), col, geo::LineStyle::Normal, false);
        }
    }
    void round(const geo::Object& o, std::uint16_t col, bool sel) {
        const auto& g = o.g.a;
        const double sweep = o.kind == geo::Kind::Circle ? 2 * M_PI : g[4];
        const double start = o.kind == geo::Kind::Circle ? 0 : g[3];
        const double rpx = g[2] * s.view.scale;
        const int n = static_cast<int>(std::clamp(rpx * std::abs(sweep) / 3.0, 12.0, 1440.0));
        double px = 0, py = 0;
        for (int i = 0; i <= n; ++i) {
            const double a = start + sweep * i / n;
            double sx, sy;
            map(g[0] + g[2] * std::cos(a), g[1] + g[2] * std::sin(a), sx, sy);
            if (i > 0) line(px, py, sx, sy, col, o.style.lineStyle, sel);
            px = sx; py = sy;
        }
    }
    void function(const geo::Object& o, std::uint16_t col, bool sel) {
        bool have = false;
        double px = 0, py = 0;
        for (int x = 0; x <= ui::ScreenW; ++x) {
            double fx, fy, y;
            geo::toPlane(s.view, ui::ScreenW, AreaH, x, 0, fx, fy);
            if (!geo::evalFunction(s, o, fx, y)) { have = false; continue; }
            double sx, sy;
            map(fx, y, sx, sy);
            if (have && std::abs(sy - py) < 4 * AreaH) line(px, py, sx, sy, col, o.style.lineStyle, sel);
            px = sx; py = sy;
            have = true;
        }
    }
    void fill(const geo::Object& o) {
        if (o.style.area == geo::NoArea) return;
        const std::uint16_t col = gfx::Canvas::blend(paletteColor(o.style.area), Bg, o.style.paint ? 70 : 140);
        if (o.kind == geo::Kind::Circle) {
            double cx, cy;
            map(o.g.a[0], o.g.a[1], cx, cy);
            const double r = o.g.a[2] * s.view.scale;
            if (r < 3000) c.circle(static_cast<int>(std::lround(cx)), static_cast<int>(std::lround(cy)), static_cast<int>(std::lround(r)), col, true);
            return;
        }
        std::vector<gfx::Point> pts;
        for (geo::Id v : o.parents) {
            const geo::Object* p = geo::find(s, v);
            if (!p || !p->g.ok) return;
            double sx, sy;
            map(p->g.a[0], p->g.a[1], sx, sy);
            pts.push_back(gfx::Point{static_cast<int>(std::lround(std::clamp(sx, -4000.0, 4000.0))), static_cast<int>(std::lround(std::clamp(sy, -4000.0, 4000.0)))});
        }
        c.polygon(pts.data(), pts.size(), col, true);
    }
    void chevron(double x, double y, double ang, std::uint16_t col) {
        double sx, sy;
        map(x, y, sx, sy);
        const double a = -ang;
        for (double d : {2.5, -2.5}) line(sx + 4 * std::cos(a), sy + 4 * std::sin(a), sx + 4 * std::cos(a) + 6 * std::cos(a + d), sy + 4 * std::sin(a) + 6 * std::sin(a + d), col, geo::LineStyle::Normal, false);
    }
};
std::string pointName(const geo::State& s, geo::Id id) { const geo::Object* o = geo::find(s, id); return o ? o->label : std::string(); }
std::string objectName(const geo::State& s, const geo::Object& o) {
    std::string n = geo::kindName(o.kind);
    if (!o.label.empty()) return n + " " + o.label;
    if (o.def == geo::Def::TwoPoints || o.def == geo::Def::CircleCP || o.def == geo::Def::Semicircle) return n + " " + pointName(s, o.parents[0]) + pointName(s, o.parents[1]);
    if (o.kind == geo::Kind::Polygon) { std::string v; for (geo::Id p : o.parents) v += pointName(s, p); return n + " " + v; }
    if (o.kind == geo::Kind::Function) return n + " y=" + o.text;
    return n;
}
std::string selectionText(const geo::State& s, const std::vector<geo::Id>& sel) {
    if (sel.empty()) return {};
    int points = 0, sides = 0;
    for (geo::Id id : sel) if (const geo::Object* o = geo::find(s, id)) { points += o->kind == geo::Kind::Point; sides += o->kind == geo::Kind::Segment; }
    const std::size_t n = sel.size();
    if (static_cast<std::size_t>(points) == n) return n == 1 ? "1 vertice selezionato" : std::to_string(n) + " vertici selezionati";
    if (static_cast<std::size_t>(sides) == n) return n == 1 ? "1 lato selezionato" : std::to_string(n) + " lati selezionati";
    if (n == 1) return objectName(s, *geo::find(s, sel[0])) + " selezionato";
    return std::to_string(n) + " oggetti selezionati";
}
std::string toolPrompt(const View& v) {
    const std::size_t k = v.picks.size();
    switch (v.tool) {
    case Tool::Point: return "Point: EXE mette il punto, EXIT finisce";
    case Tool::Line: return std::string(geo::kindName(v.lineKind)) + (k == 0 ? ": primo punto" : ": secondo punto");
    case Tool::Circle: return k == 0 ? "Circle: centro" : "Circle: punto della circonferenza";
    case Tool::Arc: return k == 0 ? "Arc: centro" : k == 1 ? "Arc: inizio" : "Arc: fine (antiorario)";
    case Tool::Semicircle: return k == 0 ? "SemiCirc: primo estremo" : "SemiCirc: secondo estremo";
    case Tool::Arc3: return k == 0 ? "3-Point Arc: primo punto" : k == 1 ? "3-Point Arc: punto intermedio" : "3-Point Arc: ultimo punto";
    case Tool::Box: return v.corner ? "Secondo angolo del riquadro" : "Primo angolo del riquadro";
    case Tool::Polygon: return "Polygon: vertice " + std::to_string(k + 1) + " (EXE sul primo chiude)";
    case Tool::Tangent: return "Select Curve";
    case Tool::AnglePosition: case Tool::PlaceText: case Tool::PlaceExpression: return "Select Display Position";
    case Tool::Axis: return "Select Axis";
    case Tool::Vector: return "Select Vector";
    case Tool::Center: return v.ttype == geo::TType::Rotation ? "Sel Rotation Center" : v.ttype == geo::TType::Dilation ? "Sel Dilation Center" : "Select Center Point";
    case Tool::ZoomBox: return v.corner ? "Zoom Box: secondo angolo" : "Zoom Box: primo angolo";
    case Tool::Pan: return "PAN: frecce, EXIT finisce";
    case Tool::Scroll: return "SCROLL: frecce, EXIT finisce";
    case Tool::Move: return "MOVE: frecce, EXE posa, AC annulla";
    default: return {};
    }
}
std::string textOf(const geo::State& s, const geo::Object& o) { return geo::measureText(s, o); }
} // namespace

std::uint16_t paletteColor(std::uint8_t index) noexcept { return kPalette[index < geo::ColorCount ? index : 0]; }
const char* paletteName(std::uint8_t index) noexcept { return kPaletteNames[index < geo::ColorCount ? index : 0]; }
const char* const* helpLines(int page, int& count) noexcept {
    switch (page) {
    case 1: count = countOf(kHelp2); return kHelp2;
    case 2: count = countOf(kHelp3); return kHelp3;
    default: count = countOf(kHelp1); return kHelp1;
    }
}
geo::Id hitText(const Context& ctx, const View& v, int sx, int sy) {
    (void)v;
    const geo::State& s = ctx.ws.geometry;
    for (auto it = s.objects.rbegin(); it != s.objects.rend(); ++it) {
        const geo::Object& o = *it;
        if (o.hidden || !o.g.ok || (o.kind != geo::Kind::Text && o.kind != geo::Kind::Measure && o.kind != geo::Kind::Expression)) continue;
        int x, y;
        toScreen(ctx, o.g.a[0], o.g.a[1], x, y);
        const int w = font::advance(font::Face::Small) * static_cast<int>(font::glyphCount(textOf(s, o).c_str()));
        if (sx >= x - 2 && sx <= x + w + 2 && sy >= y - 2 && sy <= y + 14) return o.id;
    }
    return geo::NoId;
}

void drawMain(Context& ctx, View& v, gfx::Canvas& c) {
    const geo::State& s = ctx.ws.geometry;
    ui::clear(c);
    ctx.drawStatus(c, "GEOMETRY");
    const int bottom = areaBottom(v);
    Painter p{ctx, c, s, bottom};
    c.setClip(gfx::Rect{0, AreaTop, ui::ScreenW, bottom - AreaTop});
    // sfondo (O11) con Fade I/O
    if (!v.background.empty()) {
        const int fade = s.fade;
        (void)pict::forEachSpan(v.background, [&](int x, int y, int n, std::uint16_t col) {
            const std::uint16_t shown = fade ? gfx::Canvas::blend(0xFFFF, col, static_cast<std::uint8_t>(fade * 255 / 100)) : col;
            c.hLine(x, AreaTop + y, n, shown);
        });
    }
    // griglia (V08)
    if (s.options.grid != geo::Grid::Off) {
        const double gs = s.options.gridSpace;
        if (gs * s.view.scale >= 4) {
            double x0, y0, x1, y1;
            geo::toPlane(s.view, ui::ScreenW, AreaH, 0, bottom - AreaTop, x0, y0);
            geo::toPlane(s.view, ui::ScreenW, AreaH, ui::ScreenW, 0, x1, y1);
            for (double gx = std::ceil(x0 / gs) * gs; gx <= x1; gx += gs) {
                double sx, sy;
                p.map(gx, 0, sx, sy);
                if (s.options.grid == geo::Grid::Line) c.vLine(static_cast<int>(std::lround(sx)), AreaTop, bottom - AreaTop, Grid);
                else for (double gy = std::ceil(y0 / gs) * gs; gy <= y1; gy += gs) { double tx, ty; p.map(gx, gy, tx, ty); c.pixel(static_cast<int>(std::lround(tx)), static_cast<int>(std::lround(ty)), Muted); }
            }
            if (s.options.grid == geo::Grid::Line)
                for (double gy = std::ceil(y0 / gs) * gs; gy <= y1; gy += gs) { double sx, sy; p.map(0, gy, sx, sy); c.hLine(0, static_cast<int>(std::lround(sy)), ui::ScreenW, Grid); }
        }
    }
    // assi (V07)
    if (s.options.axes != geo::Axes::Off) {
        double ox, oy;
        p.map(0, 0, ox, oy);
        const int ix = static_cast<int>(std::lround(ox)), iy = static_cast<int>(std::lround(oy));
        if (iy >= AreaTop && iy < bottom) c.hLine(0, iy, ui::ScreenW, Muted);
        if (ix >= 0 && ix < ui::ScreenW) c.vLine(ix, AreaTop, bottom - AreaTop, Muted);
        if (s.options.axes == geo::Axes::Scale) {
            const double raw = 40 / s.view.scale, pw = std::pow(10.0, std::floor(std::log10(raw)));
            double unit = pw;
            for (double m : {1.0, 2.0, 5.0, 10.0}) if (m * pw >= raw) { unit = m * pw; break; }
            double x0, y0, x1, y1;
            geo::toPlane(s.view, ui::ScreenW, AreaH, 0, bottom - AreaTop, x0, y0);
            geo::toPlane(s.view, ui::ScreenW, AreaH, ui::ScreenW, 0, x1, y1);
            char b[24];
            for (double t = std::ceil(x0 / unit) * unit; t <= x1; t += unit) {
                if (std::abs(t) < unit * 1e-9) continue;
                double sx, sy;
                p.map(t, 0, sx, sy);
                const int tx = static_cast<int>(std::lround(sx)), ty = std::clamp(iy, AreaTop, bottom - 1);
                c.vLine(tx, ty - 2, 5, Muted);
                std::snprintf(b, sizeof b, "%g", t);
                ui::text(c, tx - 6, ty + 3, b, Muted, font::Face::Small);
            }
            for (double t = std::ceil(y0 / unit) * unit; t <= y1; t += unit) {
                if (std::abs(t) < unit * 1e-9) continue;
                double sx, sy;
                p.map(0, t, sx, sy);
                const int ty = static_cast<int>(std::lround(sy)), tx = std::clamp(ix, 0, ui::ScreenW - 1);
                c.hLine(tx - 2, ty, 5, Muted);
                std::snprintf(b, sizeof b, "%g", t);
                ui::text(c, tx + 4, ty - 6, b, Muted, font::Face::Small);
            }
        }
    }
    // oggetti nell'ordine di disegno: z, poi aree, curve, punti, testi
    auto layer = [](geo::Kind k) { return k == geo::Kind::Polygon ? 0 : k == geo::Kind::Point ? 2 : (k == geo::Kind::Text || k == geo::Kind::Measure || k == geo::Kind::Expression) ? 3 : 1; };
    std::vector<const geo::Object*> order;
    order.reserve(s.objects.size());
    for (const auto& o : s.objects) if (!o.hidden && o.g.ok && o.kind != geo::Kind::Transform) order.push_back(&o);
    std::stable_sort(order.begin(), order.end(), [&](const geo::Object* a, const geo::Object* b) {
        if (a->z != b->z) return a->z < b->z;
        return layer(a->kind) < layer(b->kind);
    });
    auto selected = [&](geo::Id id) { return std::find(v.sel.begin(), v.sel.end(), id) != v.sel.end(); };
    for (const geo::Object* op : order) {
        const geo::Object& o = *op;
        const bool sel = selected(o.id);
        const std::uint16_t col = paletteColor(o.style.line);
        switch (o.kind) {
        case geo::Kind::Polygon:
            p.fill(o);
            if (sel)
                for (std::size_t i = 0; i < o.parents.size(); ++i) {
                    const geo::Object* a = geo::find(s, o.parents[i]);
                    const geo::Object* b = geo::find(s, o.parents[(i + 1) % o.parents.size()]);
                    if (a && b && a->g.ok && b->g.ok) p.planeLine(a->g.a[0], a->g.a[1], b->g.a[0], b->g.a[1], Select, geo::LineStyle::Thick, false);
                }
            break;
        case geo::Kind::Segment: case geo::Kind::Line: case geo::Kind::Ray: case geo::Kind::Vector:
            p.lineLike(o, col, sel);
            if (o.def == geo::Def::Parallel) {
                const geo::Object* base = geo::find(s, o.parents[0]);
                const double ang = std::atan2(o.g.a[3] - o.g.a[1], o.g.a[2] - o.g.a[0]);
                p.chevron(o.g.a[0], o.g.a[1], ang, col);
                if (base && base->g.ok) {
                    const double bx = base->g.a[2] - base->g.a[0], by = base->g.a[3] - base->g.a[1];
                    const double t = ((o.g.a[0] - base->g.a[0]) * bx + (o.g.a[1] - base->g.a[1]) * by) / (bx * bx + by * by);
                    p.chevron(base->g.a[0] + t * bx, base->g.a[1] + t * by, ang, paletteColor(base->style.line));
                }
            }
            break;
        case geo::Kind::Circle: p.fill(o); p.round(o, col, sel); break;
        case geo::Kind::Arc: p.round(o, col, sel); break;
        case geo::Kind::Function: p.function(o, col, sel); break;
        case geo::Kind::Point: {
            double sx, sy;
            p.map(o.g.a[0], o.g.a[1], sx, sy);
            if (sx < -20 || sx > ui::ScreenW + 20 || sy < AreaTop - 20 || sy > bottom + 20) break;
            const int x = static_cast<int>(std::lround(sx)), y = static_cast<int>(std::lround(sy));
            if (sel) { c.circle(x, y, 6, Select, false); c.circle(x, y, 5, Select, false); }
            c.circle(x, y, 3, col, true);
            if (o.showLabel && !o.label.empty()) ui::text(c, x + 5, y - 14, o.label.c_str(), paletteColor(o.style.chars), font::Face::Small);
            break;
        }
        default: {
            double sx, sy;
            p.map(o.g.a[0], o.g.a[1], sx, sy);
            const int x = static_cast<int>(std::lround(sx)), y = static_cast<int>(std::lround(sy));
            if (o.kind == geo::Kind::Measure && o.flag2 == 1 && o.parents.size() == 2) {
                // Attached Angle: segno d'angolo al vertice comune dei due lati
                const geo::Object* a = geo::find(s, o.parents[0]);
                const geo::Object* b = geo::find(s, o.parents[1]);
                if (a && b && a->g.ok && b->g.ok) {
                    const double ax = a->g.a[2] - a->g.a[0], ay = a->g.a[3] - a->g.a[1], bx = b->g.a[2] - b->g.a[0], by = b->g.a[3] - b->g.a[1];
                    const double den = ax * by - ay * bx;
                    if (std::abs(den) > 1e-12) {
                        const double t = ((b->g.a[0] - a->g.a[0]) * by - (b->g.a[1] - a->g.a[1]) * bx) / den;
                        const double vx = a->g.a[0] + t * ax, vy = a->g.a[1] + t * ay;
                        auto away = [&](const geo::Object* l, double& dx, double& dy) {
                            const double d0 = std::hypot(l->g.a[0] - vx, l->g.a[1] - vy), d1 = std::hypot(l->g.a[2] - vx, l->g.a[3] - vy);
                            dx = d0 > d1 ? l->g.a[0] - vx : l->g.a[2] - vx;
                            dy = d0 > d1 ? l->g.a[1] - vy : l->g.a[3] - vy;
                        };
                        double ux, uy, wx, wy;
                        away(a, ux, uy);
                        away(b, wx, wy);
                        double a0 = std::atan2(uy, ux), a1 = std::atan2(wy, wx);
                        double d = std::remainder(a1 - a0, 2 * M_PI);
                        double cx, cy;
                        p.map(vx, vy, cx, cy);
                        const double r = 14;
                        double qx = cx + r * std::cos(a0), qy = cy - r * std::sin(a0);
                        for (int i = 1; i <= 16; ++i) {
                            const double ang = a0 + d * i / 16;
                            const double nx = cx + r * std::cos(ang), ny = cy - r * std::sin(ang);
                            p.line(qx, qy, nx, ny, paletteColor(o.style.chars), geo::LineStyle::Normal, false);
                            qx = nx; qy = ny;
                        }
                    }
                }
            }
            const std::string t = textOf(s, o);
            if (sel) c.rect(x - 2, y - 1, font::advance(font::Face::Small) * static_cast<int>(font::glyphCount(t.c_str())) + 4, 14, SelectSoft, true);
            ui::text(c, x, y, t.c_str(), paletteColor(o.style.chars), font::Face::Small);
            break;
        }
        }
    }
    // tracce (A03)
    for (std::size_t k = 0; k < v.traces.size(); ++k) {
        const auto& t = v.traces[k];
        for (std::size_t i = 0; i + 1 < t.size(); i += 2) {
            double sx, sy;
            p.map(t[i], t[i + 1], sx, sy);
            c.rect(static_cast<int>(std::lround(sx)) - 1, static_cast<int>(std::lround(sy)) - 1, 3, 3, Warn, true);
        }
    }
    // anteprime degli strumenti
    double psx, psy;
    p.map(v.px, v.py, psx, psy);
    for (geo::Id id : v.picks) {
        const geo::Object* o = geo::find(s, id);
        if (!o || !o->g.ok) continue;
        double sx, sy;
        p.map(o->g.a[0], o->g.a[1], sx, sy);
        c.rect(static_cast<int>(std::lround(sx)) - 3, static_cast<int>(std::lround(sy)) - 3, 7, 7, Warn, false);
    }
    if (!v.picks.empty() && (v.tool == Tool::Line || v.tool == Tool::Polygon || v.tool == Tool::Arc3 || v.tool == Tool::Semicircle)) {
        const geo::Object* last = geo::find(s, v.picks.back());
        if (last && last->g.ok) { double sx, sy; p.map(last->g.a[0], last->g.a[1], sx, sy); p.line(sx, sy, psx, psy, Warn, geo::LineStyle::Broken, false); }
    }
    if (!v.picks.empty() && (v.tool == Tool::Circle || (v.tool == Tool::Arc && v.picks.size() == 1))) {
        const geo::Object* cen = geo::find(s, v.picks[0]);
        if (cen && cen->g.ok) {
            double cx, cy;
            p.map(cen->g.a[0], cen->g.a[1], cx, cy);
            const double r = std::hypot(psx - cx, psy - cy);
            if (r < 3000) c.circle(static_cast<int>(std::lround(cx)), static_cast<int>(std::lround(cy)), static_cast<int>(std::lround(r)), Warn, false);
        }
    }
    if (v.corner && (v.tool == Tool::Box || v.tool == Tool::ZoomBox)) {
        double ax, ay;
        p.map(v.cx0, v.cy0, ax, ay);
        const int x0 = static_cast<int>(std::lround(std::min(ax, psx))), y0 = static_cast<int>(std::lround(std::min(ay, psy)));
        c.dottedRect(x0, y0, static_cast<int>(std::lround(std::abs(psx - ax))) + 1, static_cast<int>(std::lround(std::abs(psy - ay))) + 1, Warn);
    }
    // puntatore
    if (v.tool != Tool::Scroll) {
        const int x = static_cast<int>(std::lround(psx)), y = static_cast<int>(std::lround(psy));
        c.hLine(x - 6, y, 13, Ink);
        c.vLine(x, y - 6, 13, Ink);
    }
    if (v.tool == Tool::Pan || v.tool == Tool::Scroll) ui::textRight(c, ui::ScreenW - 4, AreaTop + 2, v.tool == Tool::Pan ? "PAN" : "SCROLL", Accent, font::Face::Small);
    c.resetClip();

    // riga in alto: coordinate e richiesta o stato
    std::string text = "(" + formNumber(v.px) + ", " + formNumber(v.py) + ")  ";
    std::string what = toolPrompt(v);
    if (what.empty()) what = selectionText(s, v.sel);
    if (what.empty()) {
        geo::Id hover = hitText(ctx, v, static_cast<int>(std::lround(psx)), static_cast<int>(std::lround(psy)));
        if (hover == geo::NoId) hover = geo::hitTest(s, v.px, v.py, pixelTolerance(ctx));
        if (const geo::Object* o = geo::find(s, hover)) what = objectName(s, *o);
    }
    v.header = text + what;
    drawModeHeader(c, v.header);

    // casella di misura (M01)
    v.boxLine.clear();
    if (v.sel != v.boxSel) { v.boxSel = v.sel; v.boxType = 0; }
    if (v.box != BoxState::Hidden) {
        const auto specs = geo::available(s, v.sel);
        const int y = bottom;
        c.rect(0, y, ui::ScreenW, BoxH, StatusBg, true);
        c.rect(0, y, ui::ScreenW, BoxH, v.box == BoxState::Focused ? Select : StatusLine, false);
        if (specs.empty()) v.boxLine = "Selezionare un oggetto";
        else {
            const geo::MSpec& m = specs[static_cast<std::size_t>(std::clamp(v.boxType, 0, static_cast<int>(specs.size()) - 1))];
            const bool locked = geo::isLocked(s, m);
            v.boxLine = std::string(geo::mtypeName(m.type)) + ": " + geo::measureValueText(s, m, s.options.numFormat);
            if (s.options.lengthUnit != 0 && (m.type == geo::MType::Distance || m.type == geo::MType::Radius || m.type == geo::MType::Circumference || m.type == geo::MType::Perimeter))
                v.boxLine += std::string(" ") + geo::lengthUnitName(s.options.lengthUnit);
            if (locked) v.boxLine += "  [bloccata]";
            // icona del tipo e lucchetto
            ui::text(c, 4, y + 5, "\xE2\x96\xBC", Accent, font::Face::Small);
            if (locked) {
                const int lx = ui::ScreenW - 16, ly = y + 6;
                c.rect(lx, ly + 4, 10, 8, Ink, true);
                c.rect(lx + 2, ly, 6, 6, Ink, false);
            }
        }
        ui::textFit(c, 16, y + 5, v.boxLine, ui::ScreenW - 36, Ink, font::Face::Small);
    }
    v.shell.fkeys.draw(c, false);
    if (v.captureReady == false && v.pendingStore > 0) {
        // Store Picture (O12): l'area del disegno, scritta nel prossimo tick
        const int h = AreaH;
        if (pict::encode(c.pixels() + static_cast<std::size_t>(AreaTop) * static_cast<std::size_t>(c.width()), c.width(), ui::ScreenW, h, v.capture)) v.captureReady = true;
        else { v.pendingStore = 0; ctx.toast("Memory ERROR: immagine troppo grande"); }
    }
}

void drawHelp(Context& ctx, View& v, gfx::Canvas& c) {
    ui::clear(c);
    ctx.drawStatus(c, "GEOMETRY");
    int n = 0;
    const char* const* lines = helpLines(v.helpPage, n);
    c.rect(0, ui::ContentTop + 1, ui::ScreenW, 20, SelectSoft, true);
    char title[48];
    std::snprintf(title, sizeof title, "Key Help %d/%d", v.helpPage + 1, HelpPages);
    ui::text(c, 6, ui::ContentTop + 3, title, Ink);
    for (int i = 0; i < n; ++i) ui::text(c, 8, ui::ContentTop + 28 + i * 22, lines[i], i == 0 ? Accent : Ink, font::Face::Small);
    const char* keys[6] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
    ui::softKeys(c, keys);
    v.header = title;
}
void drawAnimations(Context& ctx, View& v, gfx::Canvas& c) {
    const geo::State& s = ctx.ws.geometry;
    ui::clear(c);
    ctx.drawStatus(c, "GEOMETRY");
    c.rect(0, ui::ContentTop + 1, ui::ScreenW, 20, SelectSoft, true);
    ui::text(c, 6, ui::ContentTop + 3, "Edit Animation", Ink);
    if (s.animations.empty()) ui::textCenter(c, ui::ScreenW / 2, 140, "Nessuna animazione", Muted);
    for (std::size_t i = 0; i < s.animations.size(); ++i) {
        const geo::Animation& a = s.animations[i];
        const geo::Object* p = geo::find(s, a.point);
        const geo::Object* path = geo::find(s, a.path);
        char b[96];
        std::snprintf(b, sizeof b, "%s > %s", p ? p->label.c_str() : "?", path ? objectName(s, *path).c_str() : "?");
        char d[80];
        std::snprintf(d, sizeof d, "Steps %d  t0 %s  t1 %s  Times %d", a.steps, formNumber(a.t0).c_str(), formNumber(a.t1).c_str(), a.times);
        const int y = ui::ContentTop + 26 + static_cast<int>(i) * 30;
        const bool sel = static_cast<int>(i) == v.animRow;
        if (sel) c.rect(0, y - 2, ui::ScreenW, 30, Select, true);
        ui::textFit(c, 6, y, b, ui::ScreenW - 12, sel ? SelectInk : Ink, font::Face::Small);
        ui::text(c, 18, y + 13, d, sel ? SelectInk : Muted, font::Face::Small);
    }
    const char* keys[6] = {"EDIT", nullptr, nullptr, nullptr, "DELETE", "DEL-ALL"};
    ui::softKeys(c, keys);
    v.header = "Edit Animation";
}
void drawTable(Context& ctx, View& v, gfx::Canvas& c) {
    const geo::State& s = ctx.ws.geometry;
    ui::clear(c);
    ctx.drawStatus(c, "GEOMETRY");
    const int cols = static_cast<int>(s.columns.size()), rows = s.tableRows;
    constexpr int ColW = 72, RowH = 16, IndexW = 28, Visible = 4, VisibleRows = 15;
    if (v.tableCol < v.tableFirst) v.tableFirst = v.tableCol;
    if (v.tableCol >= v.tableFirst + Visible) v.tableFirst = v.tableCol - Visible + 1;
    const int firstRow = std::max(0, v.tableRow - VisibleRows + 1);
    const int top = ui::ContentTop + 2;
    c.rect(0, top, ui::ScreenW, RowH, StatusBg, true);
    for (int k = 0; k < Visible && v.tableFirst + k < cols; ++k) {
        const int col = v.tableFirst + k;
        ui::textFit(c, IndexW + k * ColW + 2, top + 2, s.columns[static_cast<std::size_t>(col)].name, ColW - 4, col == v.tableCol ? Accent : Ink, font::Face::Small);
    }
    for (int r = 0; r < VisibleRows && firstRow + r < rows; ++r) {
        const int row = firstRow + r;
        const int y = top + RowH * (r + 1);
        char idx[16];
        std::snprintf(idx, sizeof idx, "%d", row + 1);
        ui::text(c, 4, y + 2, idx, Muted, font::Face::Small);
        for (int k = 0; k < Visible && v.tableFirst + k < cols; ++k) {
            const int col = v.tableFirst + k;
            const double x = s.table[static_cast<std::size_t>(row) * static_cast<std::size_t>(cols) + static_cast<std::size_t>(col)];
            const bool sel = row == v.tableRow && col == v.tableCol;
            if (sel) c.rect(IndexW + k * ColW, y, ColW, RowH, Select, true);
            const std::string t = x == geo::NoValue ? std::string("-") : geo::formatNumber(x, s.options.numFormat);
            ui::textFit(c, IndexW + k * ColW + 2, y + 2, t, ColW - 4, sel ? SelectInk : Ink, font::Face::Small);
        }
    }
    for (int k = 0; k <= Visible; ++k) c.vLine(IndexW + k * ColW - 1, top, RowH * (VisibleRows + 1), Faint);
    const char* keys[6] = {"STORE", "DELETE", nullptr, nullptr, nullptr, nullptr};
    ui::softKeys(c, keys);
    v.header = "Table";
}
} // namespace geoui
} // namespace fx::app
