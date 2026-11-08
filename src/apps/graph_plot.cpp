// GRAPH plot screen: incremental rendering of the curves sampled by the graph
// engine, the layers of the graph screen (background picture, grid, axes,
// recalled picture, inequality regions, curves, sketches) and the interactive
// tools: TRACE, ZOOM, SKETCH, G-SOLVE, PAN, paste and Modify display.
// Every value shown comes from the shared evaluator through the engine.
#include "apps.hpp"
#include "graph_solve.hpp"
#include "math_renderer.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

namespace fx::app {
using namespace ui::theme;

namespace {
constexpr int L = graphgeo::Left, T = graphgeo::Top, R = graphgeo::Right, B = graphgeo::Bottom;
constexpr int Cols = graphgeo::Columns, Rows = graphgeo::Rows;
constexpr std::size_t MaxMarks = 20;
const double NaN = std::numeric_limits<double>::quiet_NaN();

std::uint16_t graphColor(std::uint8_t c) {
    static const std::uint16_t pal[GraphColorCount] = {Graph[0], Graph[1], Graph[2], Graph[3], Graph[4], Graph[5], rgb888To565(225, 180, 0)};
    return pal[c % GraphColorCount];
}
struct Pen { int width; int on; int off; };
Pen penFor(LineStyle s, bool strict) {
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
// Segment in graph coordinates, clipped before rasterising.
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
std::string value(double v, const DisplayFormat& f) { return std::isfinite(v) ? formatReal(v, f) : std::string("--"); }
const char* actionName(GraphApp::Action a) {
    using A = GraphApp::Action;
    switch (a) {
    case A::ZoomBox: return "BOX";
    case A::ZoomIn: return "ZOOM IN";
    case A::ZoomOut: return "ZOOM OUT";
    case A::Pan: return "PAN";
    case A::Plot: return "Plot";
    case A::PlotOn: return "PlotOn";
    case A::PlotOff: return "PlotOff";
    case A::PlotChg: return "PlotChg";
    case A::FLine: return "F-Line";
    case A::Circle: return "Circle";
    case A::Vertical: return "Vertical";
    case A::Horizontal: return "Horizontal";
    case A::Text: return "Text";
    case A::Pen: return "PEN";
    case A::Tangent: return "Tangent";
    case A::Normal: return "Normal";
    case A::Inverse: return "Inverse";
    case A::Root: return "ROOT";
    case A::Max: return "MAX";
    case A::Min: return "MIN";
    case A::YIcept: return "Y-ICEPT";
    case A::Isect: return "INTSECT";
    case A::YCal: return "Y-CAL";
    case A::XCal: return "X-CAL";
    case A::Integral: return "\xE2\x88\xAB" "dx";
    case A::IntRoot: return "\xE2\x88\xAB" "dx ROOT";
    case A::IntIsect: return "\xE2\x88\xAB" "dx INTSECT";
    case A::IntMixed: return "\xE2\x88\xAB" "dx MIXED";
    case A::Format: return "FORMAT";
    default: return "";
    }
}
bool isTType(GraphType t) { return t == GraphType::Polar || t == GraphType::Param; }
// "=" or the relation of an inequality ("Y>" -> ">").
std::string relationText(const graph::Plot& p) {
    if (p.type == GraphType::Param || p.type == GraphType::Polar) return "=";
    return std::string(graphTypeLabel(p.type)).substr(1);
}
} // namespace

// ---------------------------------------------------------------- rendering state
bool GraphApp::tableDraw(Context& ctx) const { return ctx.ws.table.draw != TableRange::Draw::None; }

// FNV-1a of everything the plots depend on: other modes (Statistics copying a
// regression, Run-Matrix storing A..Z, the lists) may change it at any time.
std::uint64_t GraphApp::sourceFingerprint(Context& ctx) const {
    std::uint64_t h = 1469598103934665603ull;
    auto mix = [&](const void* p, std::size_t n) { const auto* b = static_cast<const std::uint8_t*>(p); for (std::size_t i = 0; i < n; ++i) { h ^= b[i]; h *= 1099511628211ull; } };
    auto mixd = [&](double d) { mix(&d, sizeof(d)); };
    for (const auto& f : ctx.ws.functions) {
        mix(f.expr.data(), f.expr.size()); mix(f.expr2.data(), f.expr2.size());
        const std::uint8_t meta[4] = {static_cast<std::uint8_t>(f.type), static_cast<std::uint8_t>(f.selected), f.color, static_cast<std::uint8_t>(f.style)};
        mix(meta, 4);
    }
    for (char v = 'A'; v <= 'Z'; ++v) mixd(ctx.ws.memory.recall(v));
    for (const auto& l : ctx.ws.lists) { const std::size_t n = l.size(); mix(&n, sizeof(n)); for (double d : l) mixd(d); }
    const std::uint8_t settings[3] = {static_cast<std::uint8_t>(ctx.ws.settings.angle), static_cast<std::uint8_t>(ctx.ws.graph.settings.plotDraw), static_cast<std::uint8_t>(ctx.ws.table.draw)};
    mix(settings, 3);
    for (const auto& s : ctx.ws.graph.sketch) if (s.kind == SketchItem::Kind::Inverse) { mix(s.text.data(), s.text.size()); mix(&s.color, 1); }
    for (const auto& p : pasted_) mix(p.data(), p.size());
    return h;
}

void GraphApp::ensureBuilt(Context& ctx) {
    const std::uint64_t fp = sourceFingerprint(ctx);
    if (fp != fingerprint_) built_ = false;
    if (built_ && builtView_ == ctx.ws.view) return;
    if (!built_) {
        ctx.ws.apply();
        engine_.build({&ctx.ws.functions, &ctx.ws.memory, &ctx.ws, &ctx.ws.lists}, pasted_);
        sketchPlots_.clear();
        for (const auto& s : ctx.ws.graph.sketch) {
            if (s.kind != SketchItem::Kind::Inverse) continue;
            try {
                graph::Plot p = engine_.makePlot(s.text, GraphType::X, s.color, s.style);
                p.variable = "x";                       // x = f(y): the expression keeps its x
                p.label = "Inv";
                sketchPlots_.push_back(std::move(p));
            } catch (const std::bad_alloc&) { throw; } catch (const std::exception&) {}
        }
        built_ = true;
        fingerprint_ = fp;
    }
    if (!(builtView_ == ctx.ws.view)) { shade_.clear(); shadeIntegral_ = false; }
    builtView_ = ctx.ws.view;
    curves_.assign(engine_.plots().size(), graph::Curve{});
    sketchCurves_.assign(sketchPlots_.size(), graph::Curve{});
    renderPlot_ = 0;
    renderDone_ = false;
    if (tableDraw(ctx)) { buildTableCurves(ctx); renderDone_ = true; }
}

bool GraphApp::renderStep(Context& ctx, int budget) {
    ensureBuilt(ctx);
    if (renderDone_) return false;
    const graph::Viewport vp(ctx.ws.view);
    const auto& plots = engine_.plots();
    const bool plotOnly = ctx.ws.graph.settings.plotDraw;
    const std::size_t total = plots.size() + sketchPlots_.size();
    auto plotAt = [&](std::size_t i) -> const graph::Plot& { return i < plots.size() ? plots[i] : sketchPlots_[i - plots.size()]; };
    auto curveAt = [&](std::size_t i) -> graph::Curve& { return i < plots.size() ? curves_[i] : sketchCurves_[i - plots.size()]; };
    if (ctx.ws.graph.settings.simulGraph) {
        // Simul Graph: every graph advances in the same tick.
        bool all = true;
        const int share = std::max(24, budget / static_cast<int>(std::max<std::size_t>(1, total)));
        for (std::size_t i = 0; i < total; ++i) {
            graph::Curve& c = curveAt(i);
            if (c.done) continue;
            int b = share;
            graph::sampleStep(engine_, plotAt(i), vp, plotOnly && i < plots.size(), c, b);
            all = all && c.done;
        }
        renderDone_ = all;
    } else {
        // One graph after the other, in the order of the relation list.
        while (budget > 0 && renderPlot_ < total) {
            graph::Curve& c = curveAt(renderPlot_);
            graph::sampleStep(engine_, plotAt(renderPlot_), vp, plotOnly && renderPlot_ < plots.size(), c, budget);
            if (c.done) ++renderPlot_;
        }
        renderDone_ = renderPlot_ >= total;
    }
    return true;
}

// TABLE GPH-CON / GPH-PLT: the table points of the selected Y functions.
void GraphApp::buildTableCurves(Context& ctx) {
    const graph::Viewport vp(ctx.ws.view);
    const TableRange& t = ctx.ws.table;
    const bool connect = t.draw == TableRange::Draw::Connect;
    const auto& plots = engine_.plots();
    const double count = t.step > 0 ? std::floor((t.end - t.start) / t.step + 1e-9) + 1 : 0;
    for (std::size_t i = 0; i < plots.size(); ++i) {
        graph::Curve& c = curves_[i];
        c = graph::Curve{};
        c.done = true;
        if (!isYType(plots[i].type) || count < 1 || count > 500) continue;
        bool have = false;
        double px = 0, py = 0;
        for (int k = 0; k < static_cast<int>(count); ++k) {
            const double x = t.start + k * t.step;
            double y;
            if (!engine_.eval(plots[i], x, y)) { have = false; continue; }
            const double sx = vp.screenX(x), sy = std::clamp(vp.screenY(y), -1e6, 1e6);
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
}

std::vector<int> GraphApp::drawablePlots(bool analysable) const {
    std::vector<int> out;
    const auto& plots = engine_.plots();
    for (int i = 0; i < static_cast<int>(plots.size()); ++i)
        if (!analysable || isYType(plots[static_cast<std::size_t>(i)].type)) out.push_back(i);
    return out;
}

// ---------------------------------------------------------------- drawing
void GraphApp::drawLayers(Context& ctx, gfx::Canvas& c) {
    const graph::Viewport vp(ctx.ws.view);
    const ViewWindow& v = ctx.ws.view;
    const GraphSettings& gs = ctx.ws.graph.settings;
    c.setClip({L, T, Cols, Rows});
    // 1) background picture with its lightness (Fade I/O)
    if (!background_.empty()) {
        const int fade = gs.fade;
        pict::forEachSpan(background_, [&](int x, int y, int n, std::uint16_t col) {
            if (y >= Rows) return;
            const std::uint16_t shown = fade ? gfx::Canvas::blend(0xFFFF, col, static_cast<std::uint8_t>(fade * 255 / 100)) : col;
            c.hLine(L + x, T + y, n, shown);
        });
    }
    // 2) grid
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
    // 3) axes, ticks, scale values and labels. An axis outside the window is
    // indicated on the edge closest to the origin (manual 5-6).
    if (gs.axes != AxesMode::Off) {
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
    // 4) recalled picture: behind the graphs, white is transparent
    if (!overlay_.empty()) {
        pict::forEachSpan(overlay_, [&](int x, int y, int n, std::uint16_t col) {
            if (col != pict::Transparent && y < Rows) c.hLine(L + x, T + y, n, col);
        });
    }
    // 5) inequality regions: Union fills each region, Intsect only the common one (manual 5-3)
    const auto& plots = engine_.plots();
    std::vector<int> ineq;
    for (int i = 0; i < static_cast<int>(plots.size()) && i < static_cast<int>(curves_.size()); ++i)
        if (isInequality(plots[static_cast<std::size_t>(i)].type) && !curves_[static_cast<std::size_t>(i)].boundary.empty()) ineq.push_back(i);
    auto inside = [&](int i, int x, int y) {
        const graph::Plot& p = plots[static_cast<std::size_t>(i)];
        const auto& bd = curves_[static_cast<std::size_t>(i)].boundary;
        const bool yType = isYType(p.type);
        const std::size_t k = static_cast<std::size_t>(yType ? x - L : y - T);
        if (k >= bd.size() || std::isnan(bd[k])) return false;
        const float b = bd[k];
        const bool strict = isStrictInequality(p.type), greater = isGreaterInequality(p.type);
        if (yType) return greater ? (strict ? y < b - 0.5f : y <= b) : (strict ? y > b + 0.5f : y >= b);
        return greater ? (strict ? x > b + 0.5f : x >= b) : (strict ? x < b - 0.5f : x <= b);
    };
    if (!ineq.empty()) {
        if (gs.ineqUnion || ineq.size() == 1) {
            for (std::size_t n = 0; n < ineq.size(); ++n) {
                const int i = ineq[n];
                const std::uint16_t col = gfx::Canvas::blend(graphColor(plots[static_cast<std::size_t>(i)].color), 0xFFFF, 110);
                for (int x = L; x <= R; ++x)
                    for (int y = T; y <= B; ++y)
                        if (((x - y + static_cast<int>(n)) & 3) == 0 && inside(i, x, y)) c.pixel(x, y, col);
            }
        } else {
            const std::uint16_t col = gfx::Canvas::blend(graphColor(plots[static_cast<std::size_t>(ineq[0])].color), 0xFFFF, 110);
            for (int x = L; x <= R; ++x)
                for (int y = T; y <= B; ++y) {
                    if (((x - y) & 3) != 0) continue;
                    bool all = true;
                    for (int i : ineq) if (!inside(i, x, y)) { all = false; break; }
                    if (all) c.pixel(x, y, col);
                }
        }
    }
    // 6) integral shading
    if (shadeIntegral_ && !shade_.empty() && integral_.f >= 0 && integral_.f < static_cast<int>(plots.size())) {
        const std::uint16_t col = gfx::Canvas::blend(graphColor(plots[static_cast<std::size_t>(integral_.f)].color), 0xFFFF, 90);
        for (std::size_t i = 0; i + 2 < shade_.size(); i += 3) {
            const int y0 = std::clamp<int>(shade_[i + 1], T, B), y1 = std::clamp<int>(shade_[i + 2], T, B);
            c.vLine(shade_[i], std::min(y0, y1), std::abs(y1 - y0) + 1, col);
        }
    }
    // 7) curves
    for (std::size_t i = 0; i < plots.size() && i < curves_.size(); ++i)
        drawCurve(c, curves_[i].xy, penFor(plots[i].style, isStrictInequality(plots[i].type)), graphColor(plots[i].color));
    for (std::size_t i = 0; i < sketchPlots_.size() && i < sketchCurves_.size(); ++i)
        drawCurve(c, sketchCurves_[i].xy, penFor(sketchPlots_[i].style, false), graphColor(sketchPlots_[i].color));
    // 8) sketches
    for (const auto& s : ctx.ws.graph.sketch) {
        const std::uint16_t col = graphColor(s.color);
        const Pen pen = penFor(s.style, false);
        switch (s.kind) {
        case SketchItem::Kind::Point: stamp(c, static_cast<int>(std::lround(vp.screenX(s.x0))), static_cast<int>(std::lround(vp.screenY(s.y0))), 3, col); break;
        case SketchItem::Kind::PointOff: stamp(c, static_cast<int>(std::lround(vp.screenX(s.x0))), static_cast<int>(std::lround(vp.screenY(s.y0))), 3, 0xFFFF); break;
        case SketchItem::Kind::Segment: mathSegment(c, vp, s.x0, s.y0, s.x1, s.y1, pen, col); break;
        case SketchItem::Kind::Tangent: case SketchItem::Kind::Normal: {
            const double len = std::hypot(s.x1 / vp.xdot(), s.y1 / vp.ydot());
            if (len <= 0) break;
            const double k = 4000.0 / len;               // far beyond the screen in both directions
            mathSegment(c, vp, s.x0 - k * s.x1, s.y0 - k * s.y1, s.x0 + k * s.x1, s.y0 + k * s.y1, pen, col);
            break;
        }
        case SketchItem::Kind::Circle: {
            const double rx = s.x1 / vp.xdot(), ry = s.x1 / vp.ydot();
            if (rx < 4000 && ry < 4000) c.ellipse(static_cast<int>(std::lround(vp.screenX(s.x0))), static_cast<int>(std::lround(vp.screenY(s.y0))),
                                                  std::max(1, static_cast<int>(std::lround(rx))), std::max(1, static_cast<int>(std::lround(ry))), col);
            break;
        }
        case SketchItem::Kind::Vertical: mathSegment(c, vp, s.x0, v.ymin - (v.ymax - v.ymin), s.x0, v.ymax + (v.ymax - v.ymin), pen, col); break;
        case SketchItem::Kind::Horizontal: mathSegment(c, vp, v.xmin - (v.xmax - v.xmin), s.y0, v.xmax + (v.xmax - v.xmin), s.y0, pen, col); break;
        case SketchItem::Kind::Text:
            ui::text(c, static_cast<int>(std::lround(vp.screenX(s.x0))), static_cast<int>(std::lround(vp.screenY(s.y0))) - 6, s.text.c_str(), col, font::Face::Small);
            break;
        case SketchItem::Kind::Pen:
            for (std::size_t i = 2; i + 1 < s.path.size(); i += 2) mathSegment(c, vp, s.path[i - 2], s.path[i - 1], s.path[i], s.path[i + 1], pen, col);
            break;
        case SketchItem::Kind::Inverse: break;                  // drawn with the curves
        }
    }
    // 9) marks placed with EXE (filled: on the graph, hollow: not included)
    for (const auto& m : marks_) {
        const int x = static_cast<int>(std::lround(vp.screenX(m.x))), y = static_cast<int>(std::lround(vp.screenY(m.y)));
        if (m.on) c.circle(x, y, 2, Ink, true); else c.circle(x, y, 2, Ink, false);
        const std::string label = "(" + shortNumber(m.x) + "," + shortNumber(m.y) + ")";
        ui::text(c, x + 4 > R - 70 ? x - 74 : x + 4, y - 13, label.c_str(), Ink, font::Face::Small);
    }
    c.resetClip();
}

bool GraphApp::tracePoint(double& x, double& y) const {
    const auto& plots = engine_.plots();
    if (cursorPlot_ < 0 || cursorPlot_ >= static_cast<int>(plots.size())) return false;
    const graph::Plot& p = plots[static_cast<std::size_t>(cursorPlot_)];
    if (isYType(p.type)) { x = cursorV_; return engine_.eval(p, cursorV_, y); }
    if (isXType(p.type)) { y = cursorV_; return engine_.eval(p, cursorV_, x); }
    return engine_.point(p, cursorV_, x, y);
}
bool GraphApp::cursor(double& x, double& y) const noexcept {
    if (mode_ == Mode::Pointer) {
        const graph::Viewport vp(builtView_);
        x = vp.mathX(px_); y = vp.mathY(py_);
        return true;
    }
    if (mode_ == Mode::Results && !solutions_.empty()) { x = solutions_[static_cast<std::size_t>(solutionIndex_)].x; y = solutions_[static_cast<std::size_t>(solutionIndex_)].y; return true; }
    return tracePoint(x, y);
}

void GraphApp::drawOverlays(Context& ctx, gfx::Canvas& c) {
    const graph::Viewport vp(ctx.ws.view);
    const GraphSettings& gs = ctx.ws.graph.settings;
    const DisplayFormat df{DisplayMode::Norm1, 0};
    const auto& plots = engine_.plots();
    topText_.clear(); bottomText_.clear();
    auto plotTitle = [&](int i) {
        if (i < 0 || i >= static_cast<int>(plots.size())) return std::string();
        const graph::Plot& p = plots[static_cast<std::size_t>(i)];
        std::string s = p.label + relationText(p) + formatNatural(p.text);
        if (p.type == GraphType::Param && p.body2) s = p.label + "," + "Yt" + p.label.substr(2) + " (parametrica)";
        if (!p.detail.empty()) s += "  [" + p.detail + "]";
        return s;
    };
    auto cross = [&](int x, int y, std::uint16_t col) {
        c.setClip({L, T, Cols, Rows});
        c.hLine(x - 6, y, 13, col); c.vLine(x, y - 6, 13, col);
        c.hLine(x - 6, y + 1, 4, 0xFFFF); c.hLine(x + 3, y + 1, 4, 0xFFFF);
        c.resetClip();
    };
    switch (mode_) {
    case Mode::Trace: {
        double x = NaN, y = NaN;
        const bool ok = tracePoint(x, y);
        const graph::Plot* p = cursorPlot_ >= 0 && cursorPlot_ < static_cast<int>(plots.size()) ? &plots[static_cast<std::size_t>(cursorPlot_)] : nullptr;
        if (gs.graphFunc || action_ == Action::Tangent || action_ == Action::Normal)
            topText_ = (action_ == Action::Tangent || action_ == Action::Normal ? std::string(actionName(action_)) + "  " : std::string()) + plotTitle(cursorPlot_);
        if (ok && p) {
            const int sx = static_cast<int>(std::lround(vp.screenX(x))), sy = static_cast<int>(std::lround(vp.screenY(y)));
            if (traceLines_ && vp.inside(sx, sy)) {
                c.setClip({L, T, Cols, Rows});
                const int ay = static_cast<int>(std::lround(std::clamp(vp.screenY(0), double(T), double(B))));
                const int ax = static_cast<int>(std::lround(std::clamp(vp.screenX(0), double(L), double(R))));
                for (int yy = std::min(sy, ay); yy <= std::max(sy, ay); yy += 3) c.pixel(sx, yy, Muted);
                for (int xx = std::min(sx, ax); xx <= std::max(sx, ax); xx += 3) c.pixel(xx, sy, Muted);
                c.resetClip();
            }
            if (vp.inside(sx, sy)) cross(sx, sy, Error);
            if (action_ == Action::Tangent || action_ == Action::Normal) {
                double d;
                if (engine_.derivative(*p, cursorV_, d)) bottomText_ = (isXType(p->type) ? "dX/dY=" : "dY/dX=") + value(d, df);
            }
            if (gs.coord && bottomText_.empty()) {
                if (p->type == GraphType::Polar) {
                    double r = NaN; (void)engine_.polarRadius(*p, cursorV_, r);
                    bottomText_ = "r=" + value(r, df) + "  \xCE\xB8=" + value(cursorV_, df);
                } else if (p->type == GraphType::Param) {
                    bottomText_ = "T=" + value(cursorV_, df) + "  X=" + value(x, df) + "  Y=" + value(y, df);
                } else {
                    bottomText_ = "X=" + value(x, df) + "  Y=" + value(y, df);
                }
                if (gs.derivative) {
                    double d;
                    const bool has = engine_.derivative(*p, cursorV_, d);
                    const char* name = p->type == GraphType::Polar ? "  dr/d\xCE\xB8=" : isXType(p->type) ? "  dX/dY=" : "  dY/dX=";
                    bottomText_ += name + (has ? value(d, df) : std::string("--"));
                }
            }
        } else if (gs.coord && p) {
            bottomText_ = (isTType(p->type) ? (p->type == GraphType::Polar ? "\xCE\xB8=" : "T=") : isXType(p->type) ? "Y=" : "X=") + value(cursorV_, df);
        }
        break;
    }
    case Mode::Pointer: {
        const double x = vp.mathX(px_), y = vp.mathY(py_);
        topText_ = actionName(action_);
        if (action_ == Action::ZoomBox || action_ == Action::FLine || action_ == Action::Circle)
            topText_ += havePoint1_ ? "  (secondo punto)" : "  (primo punto)";
        if (action_ == Action::Pen) topText_ += penDown_ ? "  (EXE: fine tratto)" : "  (EXE: inizio tratto)";
        if (action_ == Action::Pan) topText_ += panGrab_ ? "  (afferrato)" : "  (EXE: afferra)";
        c.setClip({L, T, Cols, Rows});
        if (havePoint1_) {
            const int x1 = static_cast<int>(std::lround(vp.screenX(p1x_))), y1 = static_cast<int>(std::lround(vp.screenY(p1y_)));
            if (action_ == Action::ZoomBox) c.dottedRect(std::min(x1, px_), std::min(y1, py_), std::abs(px_ - x1) + 1, std::abs(py_ - y1) + 1, Ink);
            else if (action_ == Action::FLine) c.dashedLine(x1, y1, px_, py_, graphColor(gs.plotColor));
            else if (action_ == Action::Circle) {
                const double r = std::hypot(x - p1x_, y - p1y_);
                c.ellipse(x1, y1, std::max(1, static_cast<int>(std::lround(r / vp.xdot()))), std::max(1, static_cast<int>(std::lround(r / vp.ydot()))), graphColor(gs.plotColor));
            }
        }
        if (action_ == Action::Pen && pen_.size() >= 4)
            for (std::size_t i = 2; i + 1 < pen_.size(); i += 2) mathSegment(c, vp, pen_[i - 2], pen_[i - 1], pen_[i], pen_[i + 1], penFor(gs.sketchLine, false), graphColor(gs.plotColor));
        c.resetClip();
        cross(px_, py_, action_ == Action::Pan && panGrab_ ? Accent : Ink);
        if (gs.coord) bottomText_ = "X=" + value(x, df) + "  Y=" + value(y, df);
        break;
    }
    case Mode::Select: {
        if (selectIndex_ >= 0 && selectIndex_ < static_cast<int>(selectable_.size())) {
            const int i = selectable_[static_cast<std::size_t>(selectIndex_)];
            const bool on = ((ctx.now() - blinkStart_) / 300) % 2 == 0;
            if (i >= 0 && i < static_cast<int>(curves_.size())) {
                c.setClip({L, T, Cols, Rows});
                drawCurve(c, curves_[static_cast<std::size_t>(i)].xy, {4, 0, 0}, on ? Ink : 0xFFFF);
                drawCurve(c, curves_[static_cast<std::size_t>(i)].xy, penFor(plots[static_cast<std::size_t>(i)].style, false), graphColor(plots[static_cast<std::size_t>(i)].color));
                c.resetClip();
            }
            topText_ = std::string(actionName(action_)) + (firstChoice_ >= 0 ? " (2\xC2\xB0 grafico)" : "") + ": \xE2\x96\xB2\xE2\x96\xBC EXE  " + plotTitle(i);
        }
        break;
    }
    case Mode::Results: {
        if (!solutions_.empty()) {
            const Solution& s = solutions_[static_cast<std::size_t>(solutionIndex_)];
            const int sx = static_cast<int>(std::lround(vp.screenX(s.x))), sy = static_cast<int>(std::lround(vp.screenY(s.y)));
            if (vp.inside(sx, sy, 6)) { c.circle(sx, sy, 5, Error, false); c.circle(sx, sy, 4, Error, false); cross(sx, sy, Error); }
            char idx[32];
            std::snprintf(idx, sizeof(idx), "  [%d/%d]", solutionIndex_ + 1, static_cast<int>(solutions_.size()));
            topText_ = solveLabel_ + "  " + (solvePlot_ >= 0 && solvePlot_ < static_cast<int>(plots.size()) ? plots[static_cast<std::size_t>(solvePlot_)].label : std::string()) + (solutions_.size() > 1 ? idx : "");
            bottomText_ = "X=" + value(s.x, df) + "  Y=" + value(s.y, df) + (s.extra.empty() ? "" : "  " + s.extra);
        }
        break;
    }
    case Mode::Integral: {
        const int f = integral_.f;
        topText_ = std::string(actionName(action_)) + "  " + (f >= 0 && f < static_cast<int>(plots.size()) ? plots[static_cast<std::size_t>(f)].label : std::string()) +
                   (integral_.g >= 0 && integral_.g < static_cast<int>(plots.size()) ? "," + plots[static_cast<std::size_t>(integral_.g)].label : std::string());
        if (integral_.stage < 2) {
            topText_ += integral_.stage == 0 ? "  Lower?" : "  Upper?";
            double y = 0;
            const bool ok = f >= 0 && engine_.eval(plots[static_cast<std::size_t>(f)], cursorV_, y);
            if (ok) cross(static_cast<int>(std::lround(vp.screenX(cursorV_))), static_cast<int>(std::lround(vp.screenY(y))), Error);
            if (integral_.stage == 1) {
                c.setClip({L, T, Cols, Rows});
                const int lx = static_cast<int>(std::lround(vp.screenX(integral_.lo)));
                for (int yy = T; yy <= B; yy += 3) c.pixel(lx, yy, Error);
                c.resetClip();
            }
            bottomText_ = "X=" + value(cursorV_, df);
            if (integral_.stage == 1 && action_ == Action::Integral && f >= 0) {
                try {
                    const graph::Plot& p = plots[static_cast<std::size_t>(f)];
                    const double val = graph::integrate([&](double x, double& yy) { return engine_.eval(p, x, yy); }, integral_.lo, cursorV_);
                    bottomText_ += "  \xE2\x88\xAB" "dx=" + value(val, df);
                } catch (const std::exception&) {}
            }
        } else {
            bottomText_ = "\xE2\x88\xAB" "dx=" + value(integral_.value, ctx.ws.settings.display);
            if (integral_.hasArea) bottomText_ += "  Area=" + value(integral_.area, ctx.ws.settings.display);
        }
        break;
    }
    case Mode::Modify: {
        topText_ = plotTitle(modify_.plot);
        const int n = static_cast<int>(modify_.vars.size());
        const int h = (n + 1) * 13 + 6, y0 = B - 16 - h;
        c.rect(2, y0, 116, h, 0xFFFF, true);
        c.rect(2, y0, 116, h, Faint, false);
        for (int i = 0; i <= n; ++i) {
            const bool sel = i == modify_.selected;
            const std::string line = i < n ? std::string(1, modify_.vars[static_cast<std::size_t>(i)]) + "=" + value(ctx.ws.memory.recall(modify_.vars[static_cast<std::size_t>(i)]), df)
                                           : "Step=" + value(modify_.step, df);
            ui::text(c, 6, y0 + 3 + i * 13, line.c_str(), sel ? rgb888To565(200, 0, 200) : Ink, font::Face::Small);
        }
        break;
    }
    case Mode::View:
        if (!renderDone_ && gs.graphFunc && renderPlot_ < plots.size()) topText_ = plotTitle(static_cast<int>(renderPlot_));
        if (shadeIntegral_) {
            topText_ = std::string(actionName(action_));
            bottomText_ = "\xE2\x88\xAB" "dx=" + value(integral_.value, ctx.ws.settings.display);
            if (integral_.hasArea) bottomText_ += "  Area=" + value(integral_.area, ctx.ws.settings.display);
        }
        if (tableDraw(ctx)) topText_ = ctx.ws.table.draw == TableRange::Draw::Connect ? "GPH-CON (TABLE)" : "GPH-PLT (TABLE)";
        break;
    }
    if (!topText_.empty()) { c.rect(0, T, ui::ScreenW, 14, 0xFFFF, true); ui::textFit(c, 3, T + 1, topText_, ui::ScreenW - 6, Ink, font::Face::Small); }
    if (!bottomText_.empty()) { c.rect(0, B - 13, ui::ScreenW, 14, 0xFFFF, true); ui::textFit(c, 3, B - 12, bottomText_, ui::ScreenW - 6, Ink, font::Face::Small); }
    if (!renderDone_) {
        // Progress of the incremental drawing (the keyboard stays active).
        const std::size_t total = plots.size() + sketchPlots_.size();
        const int w = total ? static_cast<int>((Cols - 1) * std::min(renderPlot_, total) / total) : 0;
        c.hLine(L, T, w, Accent);
    }
}

void GraphApp::drawPlot(Context& ctx, gfx::Canvas& c) {
    ensureBuilt(ctx);
    drawLayers(ctx, c);
    if (capturePending_) {
        // PICT: the graph layers only (no menus, cursors or texts).
        capturePending_ = false;
        pict::Image img;
        if (!pict::encode(c.pixels() + static_cast<std::size_t>(T) * static_cast<std::size_t>(c.width()), c.width(), Cols, Rows, img)) {
            ctx.toast("Memory ERROR: immagine troppo complessa", 2500);
        } else {
            img.view = ctx.ws.view;
            capture_ = std::move(img);
            captureReady_ = true;
        }
    }
    drawOverlays(ctx, c);
    if (ctx.shift() || mode_ == Mode::View || mode_ == Mode::Trace) {
        const char* keys[6] = {mode_ == Mode::Trace && action_ == Action::None ? "TRACE*" : "TRACE", "ZOOM", "V-WIN", "SKETCH", "G-SOLV", "G\xE2\x87\x94T"};
        ui::softKeys(c, keys);
    } else if (mode_ == Mode::Modify) {
        const char* keys[6] = {"COPY", nullptr, nullptr, nullptr, nullptr, nullptr};
        ui::softKeys(c, keys);
    } else {
        const char* keys[6] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
        ui::softKeys(c, keys);
    }
}

// ---------------------------------------------------------------- plot screen keys
void GraphApp::plotKey(Context& ctx, const KeyEvent& ev) {
    ensureBuilt(ctx);
    if (tableDraw(ctx) && mode_ == Mode::View && (ev.is(Key::Exit) || ev.is(Key::F6))) {
        ctx.ws.table.draw = TableRange::Draw::None;
        ctx.open(AppId::Table);
        return;
    }
    switch (mode_) {
    case Mode::Trace: traceKey(ctx, ev); return;
    case Mode::Pointer: pointerKey(ctx, ev); return;
    case Mode::Select: selectKey(ctx, ev); return;
    case Mode::Results: resultsKey(ctx, ev); return;
    case Mode::Integral: integralKey(ctx, ev); return;
    case Mode::Modify: modifyKey(ctx, ev); return;
    case Mode::View: break;
    }
    ViewWindow& v = ctx.ws.view;
    switch (ev.key) {
    case Key::F1: startTrace(ctx, Action::None); return;
    case Key::F2:
        showMenu(Menu::Zoom, "ZOOM", {"BOX", "FACTOR", "IN", "OUT", "AUTO", "ORIGINAL", "SQUARE", "ROUND", "INTEGER", "PREVIOUS", "TRIG", "STANDRD", "INITIAL"});
        return;
    case Key::F3: returnScreen_ = Screen::Plot; openWindow(ctx); return;
    case Key::F4:
        showMenu(Menu::Sketch, "SKETCH", {"Cls", "Tangent", "Norm", "Inverse", "PLOT \xE2\x96\xB6", "LINE \xE2\x96\xB6", "Circle", "Vertical", "Horz", "PEN", "Text", "Elementi \xE2\x96\xB6"});
        return;
    case Key::F5:
        showMenu(Menu::Solve, "G-SOLVE", {"ROOT", "MAX", "MIN", "Y-ICEPT", "INTSECT", "Y-CAL", "X-CAL", "\xE2\x88\xAB" "dx \xE2\x96\xB6"});
        return;
    case Key::F6: screen_ = Screen::List; return;               // G<->T: graph <-> relation list (manual 5-2)
    case Key::Optn:
        showMenu(Menu::PlotOptn, "OPTN", {"PICTURE \xE2\x96\xB6", "PAN", "FadeI/O", "BGV-WIN \xE2\x96\xB6", "FORMAT", "\xE2\x86\x92 TABLE", "FILE \xE2\x96\xB6"});
        return;
    case Key::Exit:
        if (shadeIntegral_) { shadeIntegral_ = false; shade_.clear(); return; }
        screen_ = Screen::List;
        return;
    case Key::Left: case Key::Right: {
        const double d = (v.xmax - v.xmin) / 8 * (ev.is(Key::Left) ? -1 : 1);
        v.xmin += d; v.xmax += d; ctx.markDirty();
        return;
    }
    case Key::Up: case Key::Down: {
        const double d = (v.ymax - v.ymin) / 8 * (ev.is(Key::Down) ? -1 : 1);
        v.ymin += d; v.ymax += d; ctx.markDirty();
        return;
    }
    case Key::Del: if (!marks_.empty()) marks_.pop_back(); return;
    case Key::Char:
        if (ev.ctrl) { if (ev.ch == 'v' || ev.ch == 'V') pasteGraph(ctx); return; }
        if (ev.ch == '+' || ev.ch == '-') {
            // Zoom on the centre with the FACTOR values (manual 5-9).
            const GraphSettings& gs = ctx.ws.graph.settings;
            const double cx = 0.5 * (v.xmin + v.xmax), cy = 0.5 * (v.ymin + v.ymax);
            zoomTo(ctx, ev.ch == '+' ? graph::zoomAt(v, cx, cy, gs.xFactor, gs.yFactor) : graph::zoomAt(v, cx, cy, 1 / gs.xFactor, 1 / gs.yFactor));
        }
        return;
    default: return;
    }
}

void GraphApp::startTrace(Context& ctx, Action purpose) {
    ensureBuilt(ctx);
    const auto& plots = engine_.plots();
    if (plots.empty()) throw MathError("Math ERROR: nessun grafico da tracciare");
    action_ = purpose;
    const ViewWindow& v = ctx.ws.view;
    if (cursorPlot_ < 0 || cursorPlot_ >= static_cast<int>(plots.size())) cursorPlot_ = 0;
    if (purpose == Action::Tangent || purpose == Action::Normal) {
        // tangent/normal: graphs with a derivative
        if (isTType(plots[static_cast<std::size_t>(cursorPlot_)].type) && plots[static_cast<std::size_t>(cursorPlot_)].type == GraphType::Polar) cursorPlot_ = 0;
    }
    const graph::Plot& p = plots[static_cast<std::size_t>(cursorPlot_)];
    const graph::Viewport vp(v);
    if (isYType(p.type)) cursorV_ = vp.xAt(std::round((Cols - 1) / 2.0));
    else if (isXType(p.type)) cursorV_ = vp.yAt(std::round((Rows - 1) / 2.0));
    else cursorV_ = v.tmin;
    if (tableDraw(ctx)) cursorV_ = ctx.ws.table.start;
    mode_ = Mode::Trace;
}

void GraphApp::traceMove(Context& ctx, int steps) {
    const auto& plots = engine_.plots();
    if (cursorPlot_ < 0 || cursorPlot_ >= static_cast<int>(plots.size())) return;
    const graph::Plot& p = plots[static_cast<std::size_t>(cursorPlot_)];
    ViewWindow& v = ctx.ws.view;
    const graph::Viewport vp(v);
    if (tableDraw(ctx)) {
        const TableRange& t = ctx.ws.table;
        cursorV_ = std::clamp(cursorV_ + steps * t.step, t.start, t.end);
        return;
    }
    if (isYType(p.type)) {
        cursorV_ += steps * vp.xdot();
        // The window scrolls when the pointer leaves it.
        if (cursorV_ > v.xmax || cursorV_ < v.xmin) {
            const double d = (cursorV_ > v.xmax ? 1 : -1) * (v.xmax - v.xmin) / 2;
            v.xmin += d; v.xmax += d; ctx.markDirty();
        }
    } else if (isXType(p.type)) {
        cursorV_ -= steps * vp.ydot();
        if (cursorV_ > v.ymax || cursorV_ < v.ymin) {
            const double d = (cursorV_ > v.ymax ? 1 : -1) * (v.ymax - v.ymin) / 2;
            v.ymin += d; v.ymax += d; ctx.markDirty();
        }
    } else {
        const double lo = std::min(v.tmin, v.tmax), hi = std::max(v.tmin, v.tmax);
        cursorV_ = std::clamp(cursorV_ + steps * v.tstep, lo, hi);
    }
}
void GraphApp::traceSet(Context& ctx, double v) {
    (void)ctx;
    cursorV_ = v;
}

void GraphApp::traceKey(Context& ctx, const KeyEvent& ev) {
    const auto& plots = engine_.plots();
    switch (ev.key) {
    case Key::Left: traceMove(ctx, ev.ctrl ? -10 : -1); return;
    case Key::Right: traceMove(ctx, ev.ctrl ? 10 : 1); return;
    case Key::Up: case Key::Down: {
        if (ctx.shift()) { traceLines_ = ev.is(Key::Up); return; }   // SHIFT+cursor: supplementary lines (manual 5-54)
        if (plots.size() < 2) return;
        const graph::Plot& from = plots[static_cast<std::size_t>(cursorPlot_)];
        const int n = static_cast<int>(plots.size());
        for (int k = 1; k < n; ++k) {
            const int i = (cursorPlot_ + (ev.is(Key::Up) ? n - k : k)) % n;
            const graph::Plot& to = plots[static_cast<std::size_t>(i)];
            if ((action_ == Action::Tangent || action_ == Action::Normal) && to.type == GraphType::Polar) continue;
            cursorPlot_ = i;
            // Same x for functions of x; otherwise the centre of the new graph.
            if (isYType(to.type) != isYType(from.type) || isXType(to.type) != isXType(from.type) || isTType(to.type) != isTType(from.type)) {
                const graph::Viewport vp(ctx.ws.view);
                cursorV_ = isYType(to.type) ? vp.xAt(std::round((Cols - 1) / 2.0)) : isXType(to.type) ? vp.yAt(std::round((Rows - 1) / 2.0)) : ctx.ws.view.tmin;
            }
            return;
        }
        return;
    }
    case Key::Exe: {
        double x, y;
        if (!tracePoint(x, y)) return;
        const graph::Plot& p = plots[static_cast<std::size_t>(cursorPlot_)];
        if (action_ == Action::Tangent || action_ == Action::Normal) {
            // Direction of the curve at the pointer, from the engine's derivative.
            double dx = 1, dy = 0;
            if (isYType(p.type)) { double d; if (!engine_.derivative(p, cursorV_, d)) throw MathError("Math ERROR: derivata non definita"); dy = d; }
            else if (isXType(p.type)) { double d; if (!engine_.derivative(p, cursorV_, d)) throw MathError("Math ERROR: derivata non definita"); dx = d; dy = 1; }
            else {
                dx = differentiateFunction([&](double t) { double a, b; if (!engine_.point(p, t, a, b)) throw MathError("Math ERROR"); return a; }, cursorV_);
                dy = differentiateFunction([&](double t) { double a, b; if (!engine_.point(p, t, a, b)) throw MathError("Math ERROR"); return b; }, cursorV_);
            }
            if (!std::isfinite(dx) || !std::isfinite(dy) || (dx == 0 && dy == 0)) throw MathError("Math ERROR: direzione non definita");
            SketchItem s;
            s.kind = action_ == Action::Tangent ? SketchItem::Kind::Tangent : SketchItem::Kind::Normal;
            s.x0 = x; s.y0 = y;
            if (action_ == Action::Tangent) { s.x1 = dx; s.y1 = dy; } else { s.x1 = -dy; s.y1 = dx; }
            addSketch(ctx, s);
            return;
        }
        if (marks_.size() >= MaxMarks) marks_.erase(marks_.begin());
        const bool strict = isStrictInequality(p.type);
        marks_.push_back({x, y, !strict});
        return;
    }
    case Key::Del: if (!marks_.empty()) marks_.pop_back(); return;
    case Key::F1: case Key::Exit: mode_ = Mode::View; action_ = Action::None; return;
    case Key::Char:
        if (!ev.ctrl && (std::isdigit(static_cast<unsigned char>(ev.ch)) || ev.ch == '-' || ev.ch == '.' || ev.ch == '(')) {
            const graph::Plot& p = plots[static_cast<std::size_t>(cursorPlot_)];
            const char* var = p.type == GraphType::Polar ? "\xCE\xB8:" : p.type == GraphType::Param ? "T:" : isXType(p.type) ? "Y:" : "X:";
            ask(Ask::TraceValue, var, std::string(1, ev.ch));
        }
        return;
    case Key::F2: case Key::F3: case Key::F4: case Key::F5: case Key::F6: case Key::Optn:
        // The other tools start from the plain graph (the trace point is kept for ZOOM IN/OUT).
        {
            double x, y;
            if (tracePoint(x, y)) { const graph::Viewport vp(ctx.ws.view); px_ = std::clamp(static_cast<int>(std::lround(vp.screenX(x))), L, R); py_ = std::clamp(static_cast<int>(std::lround(vp.screenY(y))), T, B); }
        }
        mode_ = Mode::View; action_ = Action::None;
        plotKey(ctx, ev);
        return;
    default: return;
    }
}

void GraphApp::startPointer(Action purpose) {
    action_ = purpose;
    mode_ = Mode::Pointer;
    havePoint1_ = false; penDown_ = false; panGrab_ = false;
    pen_.clear();
    px_ = std::clamp(px_, L, R); py_ = std::clamp(py_, T, B);
    speed_ = 1;
}

void GraphApp::pointerKey(Context& ctx, const KeyEvent& ev) {
    switch (ev.key) {
    case Key::Left: case Key::Right: case Key::Up: case Key::Down: {
        // Held arrows accelerate (the STM32 repeats them every 100 ms).
        const std::uint32_t now = ctx.now();
        speed_ = now - lastMove_ < 160 ? std::min(speed_ + 1, 8) : 1;
        lastMove_ = now;
        const int step = ev.ctrl ? 16 : speed_;
        const int dx = ev.is(Key::Left) ? -step : ev.is(Key::Right) ? step : 0;
        const int dy = ev.is(Key::Up) ? -step : ev.is(Key::Down) ? step : 0;
        if (action_ == Action::Pan && panGrab_) {
            // The grabbed point follows the pointer: the window moves the other way.
            ViewWindow& v = ctx.ws.view;
            const graph::Viewport vp(v);
            const double mx = -dx * vp.xdot(), my = dy * vp.ydot();
            v.xmin += mx; v.xmax += mx; v.ymin += my; v.ymax += my;
            ctx.markDirty();
            return;
        }
        px_ = std::clamp(px_ + dx, L, R); py_ = std::clamp(py_ + dy, T, B);
        if (action_ == Action::Pen && penDown_ && pen_.size() < 2 * MaxPenPoints) {
            const graph::Viewport vp(ctx.ws.view);
            pen_.push_back(static_cast<float>(vp.mathX(px_))); pen_.push_back(static_cast<float>(vp.mathY(py_)));
        }
        return;
    }
    case Key::Exe: pointerDone(ctx); return;
    case Key::Exit:
        if (action_ == Action::Pen && penDown_) { pointerDone(ctx); }
        mode_ = Mode::View; action_ = Action::None; havePoint1_ = false; panGrab_ = false;
        return;
    default: return;
    }
}

void GraphApp::addSketch(Context& ctx, SketchItem item) {
    auto& sk = ctx.ws.graph.sketch;
    if (sk.size() >= MaxSketchItems) throw MathError("Memory ERROR: massimo 64 elementi SKETCH (Cls per cancellare)");
    const std::string e = validateSketch(item);
    if (!e.empty()) throw MathError("Argument ERROR: " + e);
    sk.push_back(std::move(item));
    ctx.markDirty(); ctx.ws.graph.projectModified = true;
    if (sk.back().kind == SketchItem::Kind::Inverse) built_ = false;
}

void GraphApp::pointerDone(Context& ctx) {
    const graph::Viewport vp(ctx.ws.view);
    const double x = vp.mathX(px_), y = vp.mathY(py_);
    const GraphSettings& gs = ctx.ws.graph.settings;
    SketchItem s;
    s.color = gs.plotColor; s.style = gs.sketchLine;
    s.x0 = x; s.y0 = y;
    switch (action_) {
    case Action::ZoomBox:
        if (!havePoint1_) { havePoint1_ = true; p1x_ = x; p1y_ = y; return; }
        if (std::lround(vp.screenX(p1x_)) == px_ || std::lround(vp.screenY(p1y_)) == py_)
            throw MathError("Argument ERROR: i due punti non devono stare sulla stessa riga o colonna");
        mode_ = Mode::View;
        zoomTo(ctx, graph::zoomBox(ctx.ws.view, p1x_, p1y_, x, y));
        return;
    case Action::ZoomIn: case Action::ZoomOut: {
        const double f = action_ == Action::ZoomIn ? 1.0 : -1.0;
        mode_ = Mode::View;
        zoomTo(ctx, graph::zoomAt(ctx.ws.view, x, y, f > 0 ? gs.xFactor : 1 / gs.xFactor, f > 0 ? gs.yFactor : 1 / gs.yFactor));
        return;
    }
    case Action::Pan: panGrab_ = !panGrab_; return;
    case Action::Plot: case Action::PlotOn: s.kind = SketchItem::Kind::Point; addSketch(ctx, s); return;
    case Action::PlotOff: s.kind = SketchItem::Kind::PointOff; addSketch(ctx, s); return;
    case Action::PlotChg: {
        auto& sk = ctx.ws.graph.sketch;
        for (std::size_t i = sk.size(); i-- > 0;) {
            const auto& it = sk[i];
            if (it.kind == SketchItem::Kind::Point && std::abs(vp.screenX(it.x0) - px_) <= 1.5 && std::abs(vp.screenY(it.y0) - py_) <= 1.5) {
                sk.erase(sk.begin() + static_cast<std::ptrdiff_t>(i));
                ctx.markDirty(); ctx.ws.graph.projectModified = true;
                return;
            }
        }
        s.kind = SketchItem::Kind::Point; addSketch(ctx, s);
        return;
    }
    case Action::FLine:
        if (!havePoint1_) { havePoint1_ = true; p1x_ = x; p1y_ = y; return; }
        s.kind = SketchItem::Kind::Segment; s.x0 = p1x_; s.y0 = p1y_; s.x1 = x; s.y1 = y;
        havePoint1_ = false;
        addSketch(ctx, s);
        return;
    case Action::Circle: {
        if (!havePoint1_) { havePoint1_ = true; p1x_ = x; p1y_ = y; return; }
        const double r = std::hypot(x - p1x_, y - p1y_);
        if (!(r > 0)) throw MathError("Argument ERROR: raggio nullo");
        s.kind = SketchItem::Kind::Circle; s.x0 = p1x_; s.y0 = p1y_; s.x1 = r;
        havePoint1_ = false;
        addSketch(ctx, s);
        return;
    }
    case Action::Vertical: s.kind = SketchItem::Kind::Vertical; addSketch(ctx, s); return;
    case Action::Horizontal: s.kind = SketchItem::Kind::Horizontal; addSketch(ctx, s); return;
    case Action::Text: ask(Ask::SketchText, "Testo (max 32):", {}, true); return;
    case Action::Pen:
        if (!penDown_) { penDown_ = true; pen_ = {static_cast<float>(x), static_cast<float>(y)}; return; }
        penDown_ = false;
        if (pen_.size() >= 4) { s.kind = SketchItem::Kind::Pen; s.path = pen_; addSketch(ctx, s); }
        pen_.clear();
        return;
    default: mode_ = Mode::View; return;
    }
}

void GraphApp::startSelect(Context& ctx, Action purpose) {
    ensureBuilt(ctx);
    finishRendering(ctx);
    action_ = purpose;
    firstChoice_ = -1;
    const bool analysable = purpose != Action::Format && purpose != Action::YCal && purpose != Action::XCal && purpose != Action::Inverse;
    selectable_ = drawablePlots(analysable);
    const auto& plots = engine_.plots();
    if (purpose == Action::Inverse)
        selectable_.erase(std::remove_if(selectable_.begin(), selectable_.end(), [&](int i) { return !isYType(plots[static_cast<std::size_t>(i)].type); }), selectable_.end());
    if (purpose == Action::XCal)
        selectable_.erase(std::remove_if(selectable_.begin(), selectable_.end(), [&](int i) { return isTType(plots[static_cast<std::size_t>(i)].type); }), selectable_.end());
    if (purpose == Action::Format)
        selectable_.erase(std::remove_if(selectable_.begin(), selectable_.end(), [&](int i) { return plots[static_cast<std::size_t>(i)].function < 0; }), selectable_.end());
    const bool two = purpose == Action::Isect || purpose == Action::IntIsect || purpose == Action::IntMixed;
    if (selectable_.size() < (two ? 2u : 1u)) throw MathError(two ? "Math ERROR: servono due grafici Y=" : "Math ERROR: nessun grafico adatto");
    selectIndex_ = 0;
    blinkStart_ = ctx.now();
    if (selectable_.size() == (two ? 2u : 1u)) {
        // Nothing to choose (manual 5-57): use the graphs on the screen.
        if (two) { firstChoice_ = selectable_[0]; selected(ctx, selectable_[1]); }
        else selected(ctx, selectable_[0]);
        return;
    }
    mode_ = Mode::Select;
}

void GraphApp::selectKey(Context& ctx, const KeyEvent& ev) {
    const int n = static_cast<int>(selectable_.size());
    if (ev.is(Key::Up)) { selectIndex_ = (selectIndex_ + n - 1) % n; blinkStart_ = ctx.now(); return; }
    if (ev.is(Key::Down)) { selectIndex_ = (selectIndex_ + 1) % n; blinkStart_ = ctx.now(); return; }
    if (ev.is(Key::Exit)) { mode_ = Mode::View; return; }
    if (!ev.is(Key::Exe)) return;
    const int plot = selectable_[static_cast<std::size_t>(selectIndex_)];
    const bool two = action_ == Action::Isect || action_ == Action::IntIsect || action_ == Action::IntMixed;
    if (two && firstChoice_ < 0) {
        firstChoice_ = plot;
        selectable_.erase(selectable_.begin() + selectIndex_);
        selectIndex_ = 0;
        if (selectable_.size() == 1) selected(ctx, selectable_[0]);
        return;
    }
    selected(ctx, plot);
}

void GraphApp::selected(Context& ctx, int plot) {
    mode_ = Mode::View;
    const auto& plots = engine_.plots();
    switch (action_) {
    case Action::Root: case Action::Max: case Action::Min: case Action::YIcept: solve(ctx, action_, plot, -1, 0); return;
    case Action::Isect: solve(ctx, action_, firstChoice_, plot, 0); return;
    case Action::YCal: case Action::XCal: {
        solvePlot_ = plot;
        const graph::Plot& p = plots[static_cast<std::size_t>(plot)];
        const bool givenX = action_ == Action::YCal;
        const char* prompt = p.type == GraphType::Polar ? "Y-CAL \xCE\xB8:" : p.type == GraphType::Param ? "Y-CAL T:" : givenX ? "Y-CAL X:" : "X-CAL Y:";
        ask(givenX ? Ask::XValue : Ask::YValue, prompt);
        return;
    }
    case Action::Integral: case Action::IntRoot: beginIntegral(ctx, action_, plot, -1); return;
    case Action::IntIsect: case Action::IntMixed: beginIntegral(ctx, action_, firstChoice_, plot); return;
    case Action::Format: openFormat(ctx, plots[static_cast<std::size_t>(plot)].function, -1); return;
    case Action::Inverse: {
        SketchItem s;
        s.kind = SketchItem::Kind::Inverse;
        s.text = plots[static_cast<std::size_t>(plot)].text;
        s.color = ctx.ws.graph.settings.plotColor; s.style = ctx.ws.graph.settings.sketchLine;
        addSketch(ctx, s);
        return;
    }
    default: return;
    }
}

// ---------------------------------------------------------------- G-SOLVE
void GraphApp::solve(Context& ctx, Action what, int f, int g, double given) {
    const auto& plots = engine_.plots();
    const graph::Plot& p = plots[static_cast<std::size_t>(f)];
    const ViewWindow& v = ctx.ws.view;
    const GraphSettings& gs = ctx.ws.graph.settings;
    auto F = [&](double x, double& y) { return engine_.eval(p, x, y); };
    const DisplayFormat df{DisplayMode::Norm1, 0};
    std::vector<Solution> out;
    auto derivative = [&](double x) {
        double d;
        return gs.derivative && engine_.derivative(p, x, d) ? "dY/dX=" + value(d, df) : std::string();
    };
    switch (what) {
    case Action::Root:
        for (double x : graph::findRoots(F, v.xmin, v.xmax)) out.push_back({x, 0.0, derivative(x)});
        solveLabel_ = "ROOT";
        break;
    case Action::Max: case Action::Min:
        for (double x : graph::findExtrema(F, v.xmin, v.xmax, what == Action::Max)) { double y; if (F(x, y)) out.push_back({x, y, {}}); }
        solveLabel_ = what == Action::Max ? "MAX" : "MIN";
        break;
    case Action::YIcept: {
        double y;
        if (F(0, y)) out.push_back({0.0, y, derivative(0)});
        solveLabel_ = "Y-ICEPT";
        break;
    }
    case Action::Isect: {
        const graph::Plot& q = plots[static_cast<std::size_t>(g)];
        auto G = [&](double x, double& y) { return engine_.eval(q, x, y); };
        for (double x : graph::findIntersections(F, G, v.xmin, v.xmax)) { double y; if (F(x, y)) out.push_back({x, y, {}}); }
        solveLabel_ = "INTSECT";
        break;
    }
    case Action::YCal:
        solveLabel_ = "Y-CAL";
        if (isYType(p.type)) { double y; if (F(given, y)) out.push_back({given, y, derivative(given)}); }
        else if (isXType(p.type)) {
            // y values where X = f(y) equals the given x
            for (double y : graph::findValue(F, given, v.ymin, v.ymax)) out.push_back({given, y, {}});
        } else {
            double x, y;
            if (engine_.point(p, given, x, y)) out.push_back({x, y, (p.type == GraphType::Polar ? "\xCE\xB8=" : "T=") + value(given, df)});
        }
        break;
    case Action::XCal:
        solveLabel_ = "X-CAL";
        if (isYType(p.type)) for (double x : graph::findValue(F, given, v.xmin, v.xmax)) out.push_back({x, given, {}});
        else if (isXType(p.type)) { double x; if (F(given, x)) out.push_back({x, given, {}}); }
        else throw MathError("Math ERROR: X-CAL non disponibile per grafici parametrici e polari");
        break;
    default: return;
    }
    if (out.empty()) throw MathError("Math ERROR: Not Found (nessuna soluzione nella finestra)");
    solutions_ = std::move(out);
    solutionXY_.clear();
    for (const auto& s : solutions_) solutionXY_.emplace_back(s.x, s.y);
    solutionIndex_ = 0;
    solvePlot_ = f;
    action_ = what;
    mode_ = Mode::Results;
}

void GraphApp::resultsKey(Context& ctx, const KeyEvent& ev) {
    const int n = static_cast<int>(solutions_.size());
    switch (ev.key) {
    case Key::Right: solutionIndex_ = (solutionIndex_ + 1) % n; return;
    case Key::Left: solutionIndex_ = (solutionIndex_ + n - 1) % n; return;
    case Key::Exe: {
        const Solution& s = solutions_[static_cast<std::size_t>(solutionIndex_)];
        if (marks_.size() >= MaxMarks) marks_.erase(marks_.begin());
        marks_.push_back({s.x, s.y, true});
        return;
    }
    case Key::Exit: mode_ = Mode::View; return;
    case Key::F1: case Key::F2: case Key::F3: case Key::F4: case Key::F5: case Key::F6: case Key::Optn:
        mode_ = Mode::View; plotKey(ctx, ev); return;
    default: return;
    }
}

void GraphApp::beginIntegral(Context& ctx, Action what, int f, int g) {
    const auto& plots = engine_.plots();
    const ViewWindow& v = ctx.ws.view;
    integral_ = IntegralState{};
    integral_.f = f; integral_.g = g;
    action_ = what;
    const graph::Plot& p = plots[static_cast<std::size_t>(f)];
    auto F = [&](double x, double& y) { return engine_.eval(p, x, y); };
    if (what == Action::IntRoot) integral_.stops = graph::findRoots(F, v.xmin, v.xmax);
    if (what == Action::IntIsect || what == Action::IntMixed) {
        const graph::Plot& q = plots[static_cast<std::size_t>(g)];
        auto G = [&](double x, double& y) { return engine_.eval(q, x, y); };
        integral_.stops = graph::findIntersections(F, G, v.xmin, v.xmax);
        if (what == Action::IntMixed) {
            for (double x : graph::findRoots(F, v.xmin, v.xmax)) integral_.stops.push_back(x);
            for (double x : graph::findRoots(G, v.xmin, v.xmax)) integral_.stops.push_back(x);
            std::sort(integral_.stops.begin(), integral_.stops.end());
            integral_.stops.erase(std::unique(integral_.stops.begin(), integral_.stops.end(), [](double a, double b) { return std::abs(a - b) < 1e-9 * std::max(1.0, std::abs(a)); }), integral_.stops.end());
        }
    }
    if (what != Action::Integral) {
        if (integral_.stops.size() < (what == Action::IntMixed ? 1u : 2u)) throw MathError("Math ERROR: Not Found");
        cursorV_ = integral_.stops[0];
    } else {
        const graph::Viewport vp(v);
        cursorV_ = vp.xAt(std::round((Cols - 1) / 2.0));
    }
    cursorPlot_ = f;
    shadeIntegral_ = false; shade_.clear();
    mode_ = Mode::Integral;
}

void GraphApp::integralKey(Context& ctx, const KeyEvent& ev) {
    const graph::Viewport vp(ctx.ws.view);
    auto& st = integral_;
    if (st.stage >= 2) { if (ev.is(Key::Exit) || ev.is(Key::Exe)) { mode_ = Mode::View; } return; }
    const bool free = action_ == Action::Integral;
    switch (ev.key) {
    case Key::Left: case Key::Right: {
        const int dir = ev.is(Key::Left) ? -1 : 1;
        if (free || (action_ == Action::IntMixed && ev.ctrl)) { cursorV_ += dir * vp.xdot() * (ev.ctrl && free ? 10 : 1); return; }
        // jump to the previous/next root or intersection (after the lower limit in stage 1)
        int i = static_cast<int>(std::lower_bound(st.stops.begin(), st.stops.end(), cursorV_ - 1e-12 * std::max(1.0, std::abs(cursorV_))) - st.stops.begin());
        if (i < static_cast<int>(st.stops.size()) && std::abs(st.stops[static_cast<std::size_t>(i)] - cursorV_) <= 1e-9 * std::max(1.0, std::abs(cursorV_))) i += dir;
        else if (dir < 0) i -= 1;
        if (i < 0 || i >= static_cast<int>(st.stops.size())) return;
        if (st.stage == 1 && st.stops[static_cast<std::size_t>(i)] <= st.lo) return;
        cursorV_ = st.stops[static_cast<std::size_t>(i)];
        return;
    }
    case Key::Exe:
        if (st.stage == 0) {
            st.lo = cursorV_; st.stage = 1;
            if (!free) {
                // the pointer moves to the next stop on the right
                for (double x : st.stops) if (x > st.lo + 1e-12 * std::max(1.0, std::abs(st.lo))) { cursorV_ = x; break; }
                if (cursorV_ <= st.lo && action_ != Action::IntMixed) throw MathError("Math ERROR: Not Found");
            }
            return;
        }
        st.hi = cursorV_;
        finishIntegral(ctx);
        return;
    case Key::Exit: mode_ = Mode::View; return;
    case Key::Char:
        if (!ev.ctrl && (free || action_ == Action::IntMixed) && (std::isdigit(static_cast<unsigned char>(ev.ch)) || ev.ch == '-' || ev.ch == '.' || ev.ch == '('))
            ask(st.stage == 0 ? Ask::IntLower : Ask::IntUpper, st.stage == 0 ? "Lower:" : "Upper:", std::string(1, ev.ch));
        return;
    default: return;
    }
}

void GraphApp::finishIntegral(Context& ctx) {
    auto& st = integral_;
    if (!(st.lo < st.hi)) throw MathError("Argument ERROR: Lower deve essere minore di Upper");
    const auto& plots = engine_.plots();
    const graph::Plot& p = plots[static_cast<std::size_t>(st.f)];
    const graph::Plot* q = st.g >= 0 ? &plots[static_cast<std::size_t>(st.g)] : nullptr;
    auto integrand = [&](double x, double& y) {
        double a;
        if (!engine_.eval(p, x, a)) return false;
        if (!q) { y = a; return true; }
        double b;
        if (!engine_.eval(*q, x, b)) return false;
        y = a - b;
        return true;
    };
    if (action_ == Action::IntRoot) {
        int between = 0;
        for (double x : st.stops) if (x > st.lo && x < st.hi) ++between;
        if (between > 20) throw MathError("Math ERROR: 21 o piu' radici nell'intervallo");
    }
    if (action_ == Action::IntIsect) {
        int between = 0;
        for (double x : st.stops) if (x > st.lo && x < st.hi) ++between;
        if (between > 20) throw MathError("Math ERROR: 21 o piu' intersezioni nell'intervallo");
    }
    st.value = graph::integrate(integrand, st.lo, st.hi);
    st.hasArea = action_ != Action::Integral;
    st.area = st.hasArea ? graph::area(integrand, st.lo, st.hi) : 0;
    ctx.ws.memory.setAnswer(st.value);
    ctx.markDirty();
    st.stage = 2;
    // Shading between the curve and the x axis (or the other graph).
    const graph::Viewport vp(ctx.ws.view);
    shade_.clear();
    const int c0 = std::max(L, static_cast<int>(std::ceil(vp.screenX(st.lo)))), c1 = std::min(R, static_cast<int>(std::floor(vp.screenX(st.hi))));
    for (int x = c0; x <= c1; ++x) {
        const double mx = vp.mathX(x);
        double a, b = 0;
        if (!engine_.eval(p, mx, a)) continue;
        if (q && !engine_.eval(*q, mx, b)) continue;
        shade_.push_back(static_cast<std::int16_t>(x));
        shade_.push_back(static_cast<std::int16_t>(std::clamp(vp.screenY(a), -1000.0, 1000.0)));
        shade_.push_back(static_cast<std::int16_t>(std::clamp(vp.screenY(b), -1000.0, 1000.0)));
    }
    shadeIntegral_ = true;
}

// ---------------------------------------------------------------- ZOOM
void GraphApp::zoomTo(Context& ctx, const ViewWindow& v) {
    const std::string e = validateView(v);
    if (!e.empty()) throw MathError(e);
    if (!haveOriginal_) { original_ = ctx.ws.view; haveOriginal_ = true; }
    zoomStack_.push_back(ctx.ws.view);
    if (zoomStack_.size() > 16) zoomStack_.erase(zoomStack_.begin());
    ctx.ws.view = v;
    marks_.clear();
    ctx.markDirty(); ctx.ws.graph.projectModified = true;
}

void GraphApp::zoomAction(Context& ctx, int choice) {
    const ViewWindow& v = ctx.ws.view;
    double cx = 0.5 * (v.xmin + v.xmax), cy = 0.5 * (v.ymin + v.ymax);
    double tx, ty;
    if ((mode_ == Mode::Trace || mode_ == Mode::Results) && cursor(tx, ty)) { cx = tx; cy = ty; }
    switch (choice) {
    case 0: startPointer(Action::ZoomBox); return;
    case 1: ask(Ask::Factor, "Xfact:", formatReal(ctx.ws.graph.settings.xFactor, {DisplayMode::Norm2, 0})); return;
    case 2: startPointer(Action::ZoomIn); return;
    case 3: startPointer(Action::ZoomOut); return;
    case 4: {
        // AUTO: Y range from the values of the Y= graphs on the screen.
        const graph::Viewport vp(v);
        std::vector<double> values;
        for (const auto& p : engine_.plots()) {
            if (!isYType(p.type)) continue;
            for (int c = 0; c < Cols; ++c) { double y; if (engine_.eval(p, vp.xAt(c), y)) values.push_back(y); }
        }
        ViewWindow n = v;
        if (!graph::autoWindow(n, values)) throw MathError("Math ERROR: nessun valore da rappresentare");
        zoomTo(ctx, n);
        return;
    }
    case 5:
        if (haveOriginal_) { ctx.ws.view = original_; zoomStack_.clear(); marks_.clear(); ctx.markDirty(); }
        return;
    case 6: zoomTo(ctx, graph::squareWindow(v)); return;
    case 7: zoomTo(ctx, graph::roundWindow(v, cx, cy)); return;
    case 8: zoomTo(ctx, graph::integerWindow(v, cx, cy)); return;
    case 9:
        if (!zoomStack_.empty()) { ctx.ws.view = zoomStack_.back(); zoomStack_.pop_back(); marks_.clear(); ctx.markDirty(); }
        return;
    case 10: zoomTo(ctx, graph::trigWindow(v, ctx.ws.settings.angle)); return;
    case 11: zoomTo(ctx, graph::standardWindow(v)); return;
    case 12: zoomTo(ctx, graph::initialWindow(ctx.ws.settings.angle)); return;
    default: return;
    }
}

void GraphApp::pasteGraph(Context& ctx) {
    if (!ctx.clipboard) throw MathError("Argument ERROR: appunti vuoti");
    std::string text = ast::serialize(*ctx.clipboard);
    if (pasted_.size() >= graph::MaxPasted) throw MathError("Memory ERROR: massimo 10 grafici incollati");
    (void)engine_.makePlot(text, GraphType::Y, 0, LineStyle::Normal);   // Syntax ERROR before adding
    pasted_.push_back(std::move(text));
    built_ = false;
}

// ---------------------------------------------------------------- plot menus
void GraphApp::sketchAction(Context& ctx, Action a) {
    switch (a) {
    case Action::Tangent: case Action::Normal: startTrace(ctx, a); return;
    case Action::Inverse: startSelect(ctx, a); return;
    case Action::Line: {
        // Line: joins the last two points placed with PLOT (manual 5-52).
        const auto& sk = ctx.ws.graph.sketch;
        const SketchItem* last[2] = {nullptr, nullptr};
        for (std::size_t i = sk.size(); i-- > 0 && !last[1];) {
            if (sk[i].kind != SketchItem::Kind::Point) continue;
            if (!last[0]) last[0] = &sk[i]; else last[1] = &sk[i];
        }
        if (!last[1]) throw MathError("Argument ERROR: servono due punti (PLOT)");
        SketchItem s;
        s.kind = SketchItem::Kind::Segment;
        s.x0 = last[1]->x0; s.y0 = last[1]->y0; s.x1 = last[0]->x0; s.y1 = last[0]->y0;
        s.color = ctx.ws.graph.settings.plotColor; s.style = ctx.ws.graph.settings.sketchLine;
        addSketch(ctx, s);
        return;
    }
    default: startPointer(a); return;
    }
}

void GraphApp::plotMenu(Context& ctx, Menu menu, int c) {
    auto& sk = ctx.ws.graph.sketch;
    switch (menu) {
    case Menu::Zoom: zoomAction(ctx, c); return;
    case Menu::Sketch: {
        static const Action acts[] = {Action::None, Action::Tangent, Action::Normal, Action::Inverse, Action::None, Action::None,
                                      Action::Circle, Action::Vertical, Action::Horizontal, Action::Pen, Action::Text};
        if (c == 0) { confirm(Confirm::ClearSketch, "Cls: cancellare SKETCH, immagine richiamata e grafici incollati?"); return; }
        if (c == 4) { showMenu(Menu::SketchPlot, "PLOT", {"Plot", "PlotOn", "PlotOff", "PlotChg"}); return; }
        if (c == 5) { showMenu(Menu::SketchLine, "LINE", {"Line", "F-Line"}); return; }
        if (c == 11) {
            if (sk.empty()) throw MathError("Argument ERROR: nessun elemento SKETCH");
            std::vector<std::string> items;
            for (std::size_t i = 0; i < sk.size(); ++i) {
                std::string label = std::to_string(i + 1) + " " + sketchKindName(sk[i].kind);
                if (sk[i].kind == SketchItem::Kind::Text) label += " '" + sk[i].text + "'";
                items.push_back(label);
            }
            showMenu(Menu::SketchItems, "ELEMENTI SKETCH", items);
            return;
        }
        sketchAction(ctx, acts[c]);
        return;
    }
    case Menu::SketchPlot: { static const Action a[] = {Action::Plot, Action::PlotOn, Action::PlotOff, Action::PlotChg}; startPointer(a[c]); return; }
    case Menu::SketchLine: sketchAction(ctx, c == 0 ? Action::Line : Action::FLine); return;
    case Menu::SketchItems: sketchSelected_ = c; showMenu(Menu::SketchItem, "ELEMENTO", {"Elimina", "FORMAT (stile, colore)"}); return;
    case Menu::SketchItem:
        if (sketchSelected_ < 0 || sketchSelected_ >= static_cast<int>(sk.size())) return;
        if (c == 0) {
            const bool inverse = sk[static_cast<std::size_t>(sketchSelected_)].kind == SketchItem::Kind::Inverse;
            sk.erase(sk.begin() + sketchSelected_);
            ctx.markDirty(); ctx.ws.graph.projectModified = true;
            if (inverse) built_ = false;
        } else {
            openFormat(ctx, -1, sketchSelected_);
            built_ = false;
        }
        return;
    case Menu::Solve: {
        static const Action acts[] = {Action::Root, Action::Max, Action::Min, Action::YIcept, Action::Isect, Action::YCal, Action::XCal};
        if (c == 7) { showMenu(Menu::SolveIntegral, "\xE2\x88\xAB" "dx", {"\xE2\x88\xAB" "dx", "ROOT", "INTSECT", "MIXED"}); return; }
        startSelect(ctx, acts[c]);
        return;
    }
    case Menu::SolveIntegral: { static const Action a[] = {Action::Integral, Action::IntRoot, Action::IntIsect, Action::IntMixed}; startSelect(ctx, a[c]); return; }
    case Menu::PlotOptn:
        switch (c) {
        case 0: showMenu(Menu::Picture, "PICTURE", {"STORE (Pict 1-20)", "SAVE\xE2\x80\xA2" "AS", "RECALL (Pict 1-20)", "OPEN", "Esporta BMP"}); return;
        case 1: startPointer(Action::Pan); return;
        case 2: ask(Ask::Fade, "Fade I/O (0-100%):", std::to_string(ctx.ws.graph.settings.fade)); return;
        case 3:
            if (background_.empty()) throw MathError("Argument ERROR: nessuna immagine di sfondo");
            showMenu(Menu::BgvWin, "BGV-WIN", {"SAVE", "SAVE\xE2\x80\xA2" "AS"});
            return;
        case 4: startSelect(ctx, Action::Format); return;
        case 5: ctx.open(AppId::Table); return;
        default: showMenu(Menu::File, "FILE (progetto)", {"NEW", "OPEN", "SAVE", "SAVE AS", "RENAME", "DELETE"}); return;
        }
    case Menu::Picture:
        switch (c) {
        case 0: ask(Ask::PictStore, "STORE Pict (1-20):"); return;
        case 1: ask(Ask::PictSaveAs, "SAVE\xE2\x80\xA2" "AS nome (max 8):", {}, true); return;
        case 2: ask(Ask::PictRecall, "RECALL Pict (1-20):"); return;
        case 3: openFiles(ctx, Browse::Pictures); return;
        default: ask(Ask::BmpSaveAs, "BMP nome (max 8):", {}, true); return;
        }
    case Menu::BgvWin:
        if (c == 0) confirm(Confirm::BgvSave, "Aggiornare la V-Window dell'immagine di sfondo?");
        else ask(Ask::BgvSaveAs, "Sfondo SAVE\xE2\x80\xA2" "AS (max 8):", {}, true);
        return;
    default: return;
    }
}

bool GraphApp::plotAnswer(Context& ctx, Ask a, const std::string& text) {
    auto number = [&]() { return evalNumber(ctx, text); };
    auto index = [&](int max) {
        const double v = number();
        if (v != std::floor(v) || v < 1 || v > max) throw MathError("Argument ERROR: numero da 1 a " + std::to_string(max));
        return static_cast<int>(v);
    };
    switch (a) {
    case Ask::TraceValue: traceSet(ctx, number()); return true;
    case Ask::XValue: solve(ctx, Action::YCal, solvePlot_, -1, number()); return true;
    case Ask::YValue: solve(ctx, Action::XCal, solvePlot_, -1, number()); return true;
    case Ask::IntLower: cursorV_ = number(); integralKey(ctx, {Key::Exe}); return true;
    case Ask::IntUpper: cursorV_ = number(); integralKey(ctx, {Key::Exe}); return true;
    case Ask::Factor: {
        const double v = number();
        if (!(v > 0)) throw MathError("Argument ERROR: fattore positivo");
        factorX_ = v;
        ask(Ask::FactorY, "Yfact:", formatReal(ctx.ws.graph.settings.yFactor, {DisplayMode::Norm2, 0}));
        return true;
    }
    case Ask::FactorY: {
        const double v = number();
        if (!(v > 0)) throw MathError("Argument ERROR: fattore positivo");
        ctx.ws.graph.settings.xFactor = factorX_; ctx.ws.graph.settings.yFactor = v;
        ctx.markDirty(); ctx.ws.graph.projectModified = true;
        return true;
    }
    case Ask::Fade: {
        const double v = number();
        if (v < 0 || v > 100 || v != std::floor(v)) throw MathError("Argument ERROR: Fade da 0 a 100");
        ctx.ws.graph.settings.fade = static_cast<std::uint8_t>(v);
        if (!background_.empty()) background_.fade = ctx.ws.graph.settings.fade;
        ctx.markDirty(); ctx.ws.graph.projectModified = true;
        return true;
    }
    case Ask::PictStore: storePicture(ctx, files::join(files::PictFolder, pict::memoryFileName(index(pict::PictMemories)))); return true;
    case Ask::PictRecall: recallPicture(ctx, files::join(files::PictFolder, pict::memoryFileName(index(pict::PictMemories))), false); return true;
    case Ask::PictSaveAs: {
        if (!files::validName(text, 8)) throw MathError("Argument ERROR: nome di 1-8 caratteri A-Z 0-9 _ -");
        storePicture(ctx, files::join(files::PictFolder, text + files::PictExtension));
        return true;
    }
    case Ask::BgvSaveAs: {
        if (!files::validName(text, 8)) throw MathError("Argument ERROR: nome di 1-8 caratteri A-Z 0-9 _ -");
        bgvSave(ctx, files::join(files::PictFolder, text + files::PictExtension));
        return true;
    }
    case Ask::BmpSaveAs: {
        if (!files::validName(text, 8)) throw MathError("Argument ERROR: nome di 1-8 caratteri A-Z 0-9 _ -");
        finishRendering(ctx);
        capturePath_ = files::join(files::PictFolder, text + files::BmpExtension);
        captureBmp_ = true;
        capturePending_ = true;
        return true;
    }
    case Ask::SketchText: {
        if (text.size() > MaxSketchText) throw MathError("Argument ERROR: massimo 32 caratteri");
        const graph::Viewport vp(ctx.ws.view);
        SketchItem s;
        s.kind = SketchItem::Kind::Text;
        s.x0 = vp.mathX(px_); s.y0 = vp.mathY(py_);
        s.text = text;
        s.color = ctx.ws.graph.settings.plotColor;
        addSketch(ctx, s);
        return true;
    }
    default: return false;
    }
}
} // namespace fx::app
