// Grafici statistici di STATISTICS sulla Pimoroni Pico Plus 2 W (v1.18.0):
// modelli dalle liste (statplot::build), finestra automatica (Stat Wind Auto),
// disegno dei 20 tipi della fx-CG50 (Scatter, xyLine, NPPlot, Pie, Hist,
// MedBox con gli outlier, N-Dist, Broken, regressioni, Bar), marcatori e colori
// (Graph Color, Color Link), TRACE e grafico d'interazione dell'ANOVA a due vie.
// Disegni propri (nessuna grafica Casio).
#include "apps.hpp"
#include "stat_engine.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <limits>

namespace fx::app::statplot {
using namespace ui::theme;
using stats::GraphType;

namespace {
constexpr double kPi = 3.14159265358979323846;
const std::vector<double>* freqOf(const Plot& p) { return p.f.empty() ? nullptr : &p.f; }
// Coordinate di schermo limitate (nessun overflow con valori lontani dalla finestra).
struct Map {
    gfx::Rect a;
    Window w;
    int sx(double x) const { const double v = a.x + (x - w.xmin) / (w.xmax - w.xmin) * (a.w - 1); return static_cast<int>(std::lround(std::clamp(v, -20000.0, 20000.0))); }
    int sy(double y) const { const double v = a.y + a.h - 1 - (y - w.ymin) / (w.ymax - w.ymin) * (a.h - 1); return static_cast<int>(std::lround(std::clamp(v, -20000.0, 20000.0))); }
};
double niceStep(double span) {
    if (!(span > 0) || !std::isfinite(span)) return 1;
    const double raw = span / 8, mag = std::pow(10.0, std::floor(std::log10(raw))), f = raw / mag;
    return (f < 1.5 ? 1 : f < 3.5 ? 2 : f < 7.5 ? 5 : 10) * mag;
}
void mark(gfx::Canvas& c, int x, int y, std::uint8_t kind, std::uint16_t col) {
    switch (kind) {
    case 1: c.line(x - 2, y - 2, x + 2, y + 2, col); c.line(x - 2, y + 2, x + 2, y - 2, col); break;    // croce
    case 2: c.rect(x - 1, y - 1, 3, 3, col, true); break;                                                // punto
    case 3: c.circle(x, y, 3, col, false); break;                                                        // cerchio
    default: c.rect(x - 2, y - 2, 5, 5, col, false); break;                                              // quadrato
    }
}
std::uint16_t pointColor(const Plot& p, std::size_t i, const Workspace& ws) {
    switch (p.g.colorLink) {
    case 1: { const double f = p.f.empty() ? 1 : p.f[i]; return paletteColor(static_cast<std::uint8_t>(static_cast<long>(std::max(0.0, f - 1)) % 6)); }   // X&Freq
    case 2: return paletteColor(ws.statistics.listColor[p.g.xList]);                                                                           // OnData
    default: return paletteColor(p.g.color);
    }
}
void axes(gfx::Canvas& c, const Map& m) {
    const Window& w = m.w;
    if (w.xmin <= 0 && w.xmax >= 0) c.vLine(m.sx(0), m.a.y, m.a.h, Muted);
    if (w.ymin <= 0 && w.ymax >= 0) c.hLine(m.a.x, m.sy(0), m.a.w, Muted);
    const int y0 = w.ymin <= 0 && w.ymax >= 0 ? m.sy(0) : m.a.y + m.a.h - 1, x0 = w.xmin <= 0 && w.xmax >= 0 ? m.sx(0) : m.a.x;
    if (w.xscl > 0 && (w.xmax - w.xmin) / w.xscl < 80)
        for (double t = std::ceil(w.xmin / w.xscl) * w.xscl; t <= w.xmax; t += w.xscl) c.vLine(m.sx(t), y0 - 2, 5, Muted);
    if (w.yscl > 0 && (w.ymax - w.ymin) / w.yscl < 80)
        for (double t = std::ceil(w.ymin / w.yscl) * w.yscl; t <= w.ymax; t += w.yscl) c.hLine(x0 - 2, m.sy(t), 5, Muted);
}
void curve(gfx::Canvas& c, const Map& m, const std::function<double(double)>& f, std::uint16_t col) {
    bool prev = false;
    int px = 0, py = 0;
    for (int x = m.a.x; x < m.a.x + m.a.w; ++x) {
        const double xv = m.w.xmin + (m.w.xmax - m.w.xmin) * (x - m.a.x) / (m.a.w - 1);
        const double yv = f(xv);
        if (!std::isfinite(yv)) { prev = false; continue; }
        const int y = m.sy(yv);
        if (prev && std::abs(y - py) < 2000) c.line(px, py, x, y, col);
        px = x; py = y; prev = true;
    }
}
void drawPie(gfx::Canvas& c, const Plot& p, const gfx::Rect& a) {
    const int cx = a.x + a.w / 2 - 50, cy = a.y + a.h / 2, r = std::min(a.h / 2 - 10, 110);
    double start = -90;
    for (std::size_t i = 0; i < p.pie.size(); ++i) {
        const double sweep = 360 * p.pie[i] / 100;
        const std::uint16_t col = paletteColor(static_cast<std::uint8_t>(i % 6));
        gfx::Point pts[32];
        std::size_t n = 0;
        pts[n++] = {cx, cy};
        const int steps = std::max(1, std::min(30, static_cast<int>(std::ceil(sweep / 6))));
        for (int k = 0; k <= steps && n < 32; ++k) {
            const double ang = (start + sweep * k / steps) * kPi / 180;
            pts[n++] = {cx + static_cast<int>(std::lround(r * std::cos(ang))), cy + static_cast<int>(std::lround(r * std::sin(ang)))};
        }
        if (sweep > 0) c.polygon(pts, n, col, true);
        start += sweep;
    }
    c.circle(cx, cy, r, Ink, false);
    // Legenda: dato e percentuale (Display: %, Data, None).
    int y = a.y + 6;
    for (std::size_t i = 0; i < p.pie.size() && y < a.y + a.h - 12; ++i, y += 14) {
        c.rect(a.x + a.w - 100, y + 2, 8, 8, paletteColor(static_cast<std::uint8_t>(i % 6)), true);
        char b[48];
        if (p.g.pieDisplay == 0) std::snprintf(b, sizeof b, "%.4g%%", p.pie[i]);
        else if (p.g.pieDisplay == 1) std::snprintf(b, sizeof b, "%.6g", p.x[i]);
        else b[0] = 0;
        ui::text(c, a.x + a.w - 88, y, b, Ink, font::Face::Small);
    }
}
void drawBars(gfx::Canvas& c, const Plot& p, const gfx::Rect& a) {
    std::size_t n = 0, sets = 0;
    double lo = 0, hi = 0;
    for (const auto& b : p.bars) {
        if (b.empty()) continue;
        ++sets; n = std::max(n, b.size());
        for (double v : b) { lo = std::min(lo, v); hi = std::max(hi, v); }
    }
    if (!n) return;
    if (hi == lo) hi = lo + 1;
    const bool horiz = p.g.stick == 1;
    const double pad = (hi - lo) * 0.1;
    Window w;
    if (horiz) { w.xmin = lo - (lo < 0 ? pad : 0); w.xmax = hi + pad; w.ymin = 0; w.ymax = static_cast<double>(n) + 0.5; }
    else { w.ymin = lo - (lo < 0 ? pad : 0); w.ymax = hi + pad; w.xmin = 0; w.xmax = static_cast<double>(n) + 0.5; }
    const Map m{a, w};
    if (horiz) c.vLine(m.sx(0), a.y, a.h, Muted); else c.hLine(a.x, m.sy(0), a.w, Muted);
    const double slot = 0.8 / static_cast<double>(sets);
    std::size_t set = 0;
    for (std::size_t k = 0; k < 3; ++k) {
        const auto& b = p.bars[k];
        if (b.empty()) continue;
        const std::uint16_t col = paletteColor(k == 0 ? p.g.color : static_cast<std::uint8_t>((p.g.color + k) % 6));
        for (std::size_t i = 0; i < b.size(); ++i) {
            const double c0 = static_cast<double>(i) + 1 - 0.4 + slot * static_cast<double>(set), c1 = c0 + slot;
            if (horiz) {
                const int y0 = m.sy(c1), y1 = m.sy(c0), x0 = m.sx(std::min(0.0, b[i])), x1 = m.sx(std::max(0.0, b[i]));
                c.rect(x0, y0, std::max(1, x1 - x0), std::max(1, y1 - y0), col, true);
            } else {
                const int x0 = m.sx(c0), x1 = m.sx(c1), y0 = m.sy(std::max(0.0, b[i])), y1 = m.sy(std::min(0.0, b[i]));
                c.rect(x0, y0, std::max(1, x1 - x0), std::max(1, y1 - y0), col, true);
            }
        }
        ++set;
    }
}
} // namespace

std::uint16_t paletteColor(std::uint8_t index) noexcept {
    static const std::uint16_t colors[stats::kPaletteColors] = {gfx::rgb888To565(0, 60, 220), gfx::rgb888To565(220, 20, 30), gfx::rgb888To565(200, 0, 200),
                                                                gfx::rgb888To565(0, 150, 40), gfx::rgb888To565(0, 160, 200), gfx::rgb888To565(220, 170, 0), 0x0000};
    return colors[index < stats::kPaletteColors ? index : 6];
}
gfx::Rect plotArea() { return gfx::Rect{0, ui::ContentTop, ui::ScreenW, ui::ContentBottom - ui::ContentTop}; }

Plot build(const Workspace& ws, std::size_t index, const stats::StatGraph& g, stats::QuartileRule rule) {
    Plot p;
    p.index = index;
    p.g = g;
    const auto& L = ws.lists;
    auto listName = [](std::size_t i) { return "List " + std::to_string(i + 1); };
    switch (g.type) {
    case GraphType::Pie:
        p.x = L[g.pieData];
        if (p.x.empty()) throw DimensionError("Dimension ERROR: " + listName(g.pieData) + " vuota");
        p.pie = stats::piePercent(p.x);
        return p;
    case GraphType::Bar:
        for (std::size_t k = 0; k < 3; ++k) if (g.bar[k] > 0) p.bars[k] = L[g.bar[k] - 1u];
        if (p.bars[0].empty()) throw DimensionError("Dimension ERROR: Data1 vuota");
        for (const auto& b : p.bars) for (double v : b) if (!std::isfinite(v)) throw MathError("Math ERROR: dato non finito");
        return p;
    default: break;
    }
    p.x = L[g.xList];
    if (p.x.empty()) throw DimensionError("Dimension ERROR: " + listName(g.xList) + " vuota");
    if (g.freq > 0) { p.f = L[g.freq - 1u]; stats::checkFrequencies(p.x.size(), &p.f); }
    if (stats::graphNeedsY(g.type)) {
        p.y = L[g.yList];
        if (p.y.size() != p.x.size()) throw DimensionError("Dimension ERROR: " + listName(g.xList) + " e " + listName(g.yList) + " di lunghezza diversa");
    }
    switch (g.type) {
    case GraphType::Hist: case GraphType::Broken: p.hist = stats::histogram(p.x, freqOf(p), g.hStart, g.hWidth); break;
    case GraphType::MedBox: p.box = stats::boxModel(p.x, freqOf(p), rule, g.outliers); break;
    case GraphType::NPPlot: stats::normalProbability(p.x, freqOf(p), p.nx, p.nz); break;
    case GraphType::NDist: {
        const stats::OneVar o = stats::oneVarEx(p.x, freqOf(p), rule);
        if (!(o.popSd > 0)) throw MathError("Math ERROR: \xCF\x83x = 0 (N-Dist)");
        p.mean = o.mean; p.sd = o.popSd;
        break;
    }
    default:
        if (stats::graphIsRegression(g.type)) { p.fit = stats::fit(stats::regressionOf(g.type), p.x, p.y, freqOf(p)); p.hasFit = true; }
        else for (double v : p.x) if (!std::isfinite(v)) throw MathError("Math ERROR: dato non finito");
        break;
    }
    return p;
}

Window autoWindow(const std::vector<Plot>& plots) {
    double xlo = std::numeric_limits<double>::infinity(), xhi = -xlo, ylo = xlo, yhi = -xlo;
    bool yFixed = true, zeroY = false;
    auto ext = [](double& lo, double& hi, double v) { if (std::isfinite(v)) { lo = std::min(lo, v); hi = std::max(hi, v); } };
    for (const Plot& p : plots) {
        switch (p.g.type) {
        case GraphType::Pie: case GraphType::Bar: break;
        case GraphType::Hist: case GraphType::Broken: {
            ext(xlo, xhi, p.hist.start); ext(xlo, xhi, p.hist.start + p.hist.width * static_cast<double>(p.hist.counts.size()));
            for (double v : p.hist.counts) ext(ylo, yhi, v);
            ext(ylo, yhi, 0); zeroY = true; yFixed = false;
            break;
        }
        case GraphType::MedBox: ext(xlo, xhi, p.box.min); ext(xlo, xhi, p.box.max); break;
        case GraphType::NPPlot:
            for (double v : p.nx) ext(xlo, xhi, v);
            for (double v : p.nz) ext(ylo, yhi, v);
            yFixed = false;
            break;
        case GraphType::NDist:
            ext(xlo, xhi, p.mean - 3.5 * p.sd); ext(xlo, xhi, p.mean + 3.5 * p.sd);
            ext(ylo, yhi, 0); ext(ylo, yhi, 1 / (p.sd * std::sqrt(2 * kPi)));
            zeroY = true; yFixed = false;
            break;
        default:
            for (double v : p.x) ext(xlo, xhi, v);
            for (double v : p.y) ext(ylo, yhi, v);
            yFixed = false;
            break;
        }
    }
    Window w;
    if (!std::isfinite(xlo)) { xlo = -1; xhi = 1; }
    if (xlo == xhi) { xlo -= 1; xhi += 1; }
    if (yFixed || !std::isfinite(ylo)) { ylo = 0; yhi = 1; }
    if (ylo == yhi) { ylo -= 1; yhi += 1; }
    const double px = (xhi - xlo) * 0.1, py = (yhi - ylo) * 0.1;
    w.xmin = xlo - px; w.xmax = xhi + px;
    w.ymin = zeroY && ylo >= 0 ? -py * 0.5 : ylo - py; w.ymax = yhi + py;
    w.xscl = niceStep(w.xmax - w.xmin); w.yscl = niceStep(w.ymax - w.ymin);
    return w;
}

void draw(gfx::Canvas& c, const Window& w, const std::vector<Plot>& plots, const stats::Fit* overlay, const Workspace& ws) {
    const gfx::Rect a = plotArea();
    c.setClip(a);
    for (const Plot& p : plots) if (p.g.type == GraphType::Pie) { drawPie(c, p, a); c.resetClip(); return; }
    for (const Plot& p : plots) if (p.g.type == GraphType::Bar) { drawBars(c, p, a); c.resetClip(); return; }
    const Map m{a, w};
    axes(c, m);
    std::size_t boxes = 0, box = 0;
    for (const Plot& p : plots) if (p.g.type == GraphType::MedBox) ++boxes;
    for (const Plot& p : plots) {
        const std::uint16_t col = paletteColor(p.g.color);
        switch (p.g.type) {
        case GraphType::Scatter: case GraphType::XYLine: {
            int px = 0, py = 0;
            for (std::size_t i = 0; i < p.x.size(); ++i) {
                if (!p.f.empty() && p.f[i] <= 0) continue;
                const int x = m.sx(p.x[i]), y = m.sy(p.y[i]);
                if (p.g.type == GraphType::XYLine && i) c.line(px, py, x, y, col);
                mark(c, x, y, p.g.mark, pointColor(p, i, ws));
                px = x; py = y;
            }
            break;
        }
        case GraphType::NPPlot:
            for (std::size_t i = 0; i < p.nx.size(); ++i) mark(c, m.sx(p.nx[i]), m.sy(p.nz[i]), p.g.mark, col);
            break;
        case GraphType::Hist: case GraphType::Broken: {
            const auto& h = p.hist;
            int px = 0, py = 0;
            for (std::size_t k = 0; k < h.counts.size(); ++k) {
                const double x0 = h.start + h.width * static_cast<double>(k), x1 = x0 + h.width;
                const int X0 = m.sx(x0), X1 = m.sx(x1), Y = m.sy(h.counts[k]), B = m.sy(0);
                if (p.g.type == GraphType::Hist) {
                    c.rect(X0, Y, std::max(1, X1 - X0), std::max(0, B - Y), SelectSoft, true);
                    c.rect(X0, Y, std::max(1, X1 - X0 + 1), std::max(1, B - Y + 1), col, false);
                } else {
                    const int cx = (X0 + X1) / 2;
                    if (k) c.line(px, py, cx, Y, col);
                    c.rect(cx - 1, Y - 1, 3, 3, col, true);
                    px = cx; py = Y;
                }
            }
            break;
        }
        case GraphType::MedBox: {
            const auto& b = p.box;
            ++box;
            const int yc = a.y + static_cast<int>(a.h * static_cast<double>(box) / static_cast<double>(boxes + 1)), h = std::min(36, a.h / static_cast<int>(boxes + 1) - 6);
            c.hLine(m.sx(b.lowWhisker), yc, m.sx(b.q1) - m.sx(b.lowWhisker), col);
            c.hLine(m.sx(b.q3), yc, m.sx(b.highWhisker) - m.sx(b.q3), col);
            c.vLine(m.sx(b.lowWhisker), yc - h / 4, h / 2 + 1, col); c.vLine(m.sx(b.highWhisker), yc - h / 4, h / 2 + 1, col);
            c.rect(m.sx(b.q1), yc - h / 2, m.sx(b.q3) - m.sx(b.q1) + 1, h, SelectSoft, true);
            c.rect(m.sx(b.q1), yc - h / 2, m.sx(b.q3) - m.sx(b.q1) + 1, h, col, false);
            c.vLine(m.sx(b.med), yc - h / 2, h, col);
            for (double o : b.outliers) mark(c, m.sx(o), yc, 0, col);
            break;
        }
        case GraphType::NDist:
            curve(c, m, [&](double x) { return std::exp(-0.5 * (x - p.mean) * (x - p.mean) / (p.sd * p.sd)) / (p.sd * std::sqrt(2 * kPi)); }, col);
            break;
        default:
            if (p.hasFit) {
                for (std::size_t i = 0; i < p.x.size(); ++i) if (p.f.empty() || p.f[i] > 0) mark(c, m.sx(p.x[i]), m.sy(p.y[i]), p.g.mark, col);
                curve(c, m, [&](double x) { return p.fit.predict(x); }, col);
            }
            break;
        }
    }
    if (overlay) {
        const std::uint16_t col = paletteColor(1);
        curve(c, m, [&](double x) { return overlay->predict(x); }, col);
        ui::textRight(c, a.x + a.w - 4, a.y + 2, stats::regModel(overlay->type), col, font::Face::Small);
    }
    c.resetClip();
}

std::size_t traceCount(const Plot& p) {
    switch (p.g.type) {
    case GraphType::Pie: return p.pie.size();
    case GraphType::Bar: return p.bars[0].size();
    case GraphType::Hist: case GraphType::Broken: return p.hist.counts.size();
    case GraphType::MedBox: return 5;
    case GraphType::NDist: return 101;
    case GraphType::NPPlot: return p.nx.size();
    default: return p.x.size();
    }
}
bool tracePoint(const Plot& p, std::size_t k, double& x, double& y, std::string& label) {
    char b[96];
    const DisplayFormat f{DisplayMode::Norm1, 0};
    const std::string head = "StatGraph" + std::to_string(p.index + 1) + "  ";
    switch (p.g.type) {
    case GraphType::Pie:
        if (k >= p.pie.size()) return false;
        std::snprintf(b, sizeof b, "%s[%zu]=%s  %.4g%%", "Data", k + 1, formatReal(p.x[k], f).c_str(), p.pie[k]);
        label = head + b; return false;
    case GraphType::Bar:
        if (k >= p.bars[0].size()) return false;
        label = head + "[" + std::to_string(k + 1) + "]=" + formatReal(p.bars[0][k], f); return false;
    case GraphType::Hist: case GraphType::Broken:
        if (k >= p.hist.counts.size()) return false;
        x = p.hist.start + p.hist.width * (static_cast<double>(k) + 0.5); y = p.hist.counts[k];
        label = head + "X=" + formatReal(p.hist.start + p.hist.width * static_cast<double>(k), f) + "  f=" + formatReal(y, f);
        return true;
    case GraphType::MedBox: {
        static const char* const n[] = {"minX", "Q1", "Med", "Q3", "maxX"};
        const double v[] = {p.box.min, p.box.q1, p.box.med, p.box.q3, p.box.max};
        if (k >= 5) return false;
        label = head + n[k] + "=" + formatReal(v[k], f);
        return false;
    }
    case GraphType::NDist:
        x = p.mean - 3.5 * p.sd + 7 * p.sd * static_cast<double>(std::min<std::size_t>(k, 100)) / 100;
        y = std::exp(-0.5 * (x - p.mean) * (x - p.mean) / (p.sd * p.sd)) / (p.sd * std::sqrt(2 * kPi));
        break;
    case GraphType::NPPlot:
        if (k >= p.nx.size()) return false;
        x = p.nx[k]; y = p.nz[k];
        break;
    default:
        if (k >= p.x.size()) return false;
        x = p.x[k]; y = p.y[k];
        break;
    }
    label = head + "X=" + formatReal(x, f) + "  Y=" + formatReal(y, f);
    return true;
}
void drawTraceCursor(gfx::Canvas& c, const Window& w, double x, double y) {
    const Map m{plotArea(), w};
    const int X = m.sx(x), Y = m.sy(y);
    c.hLine(X - 6, Y, 13, Error); c.vLine(X, Y - 6, 13, Error);
}
void drawInteraction(gfx::Canvas& c, const stats::AnovaTable& t) {
    const gfx::Rect a = plotArea();
    if (!t.twoWay || t.cellMeans.empty()) return;
    const std::size_t na = t.levelsA.size(), nb = t.levelsB.size();
    double lo = t.cellMeans[0], hi = lo;
    for (double v : t.cellMeans) { lo = std::min(lo, v); hi = std::max(hi, v); }
    if (lo == hi) { lo -= 1; hi += 1; }
    Window w;
    w.xmin = 0.5; w.xmax = static_cast<double>(na) + 0.5; w.ymin = lo - (hi - lo) * 0.15; w.ymax = hi + (hi - lo) * 0.25;
    w.xscl = 1; w.yscl = niceStep(w.ymax - w.ymin);
    const Map m{a, w};
    c.setClip(a);
    c.hLine(a.x, a.y + a.h - 1, a.w, Muted);
    for (std::size_t i = 0; i < na; ++i) c.vLine(m.sx(static_cast<double>(i) + 1), a.y + a.h - 5, 5, Muted);
    for (std::size_t j = 0; j < nb; ++j) {
        const std::uint16_t col = paletteColor(static_cast<std::uint8_t>(j % 6));
        int px = 0, py = 0;
        for (std::size_t i = 0; i < na; ++i) {
            const int x = m.sx(static_cast<double>(i) + 1), y = m.sy(t.cellMeans[i * nb + j]);
            if (i) c.line(px, py, x, y, col);
            mark(c, x, y, 0, col);
            px = x; py = y;
        }
        char b[32]; std::snprintf(b, sizeof b, "B=%.6g", t.levelsB[j]);
        ui::text(c, a.x + 4 + static_cast<int>(j) * 64, a.y + 2, b, col, font::Face::Small);
    }
    ui::textRight(c, a.x + a.w - 4, a.y + 2, "Interaction A\xC3\x97" "B", Ink, font::Face::Small);
    c.resetClip();
}
} // namespace fx::app::statplot
