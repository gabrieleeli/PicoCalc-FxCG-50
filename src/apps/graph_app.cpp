// GRAPH: function list (Y=, r=, Y>, Y<, Y>=, Y<=), plot with grid/axes,
// TRACE, ZOOM, V-Window, SKETCH (tangent/normal) and G-SOLVE (roots, extrema,
// y-intercept, intersections, integral with shaded area).
#include "apps.hpp"
#include "math_renderer.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

namespace fx::app {
using namespace ui::theme;

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr int W = GraphApp::PlotRight - GraphApp::PlotLeft;
constexpr int H = GraphApp::PlotBottom - GraphApp::PlotTop;
const float NaNf = std::numeric_limits<float>::quiet_NaN();
const std::vector<std::string> kZoom = {"IN  (x0.5)", "OUT (x2)", "AUTO (scala Y)", "STD (-10..10)", "TRIG", "SQR (scala uguale)", "INITIAL"};
const std::vector<std::string> kSolve = {"ROOT  (radici)", "MAX", "MIN", "Y-ICEPT", "ISECT (intersezione)", "\xE2\x88\xAB" "dx  (integrale)"};
const std::vector<std::string> kSketch = {"Tangent (tangente)", "Normal (normale)", "Cancella"};
bool sameView(const ViewWindow& a, const ViewWindow& b) {
    return a.xmin == b.xmin && a.xmax == b.xmax && a.xscl == b.xscl && a.ymin == b.ymin && a.ymax == b.ymax &&
           a.yscl == b.yscl && a.tmin == b.tmin && a.tmax == b.tmax && a.tstep == b.tstep && a.grid == b.grid;
}
bool isInequality(GraphType t) { return t == GraphType::YGreater || t == GraphType::YLess || t == GraphType::YGreaterEq || t == GraphType::YLessEq; }
FormField num(const char* l, double v) { return {l, formatReal(v, {DisplayMode::Norm2, 0}), FormField::Type::Number, {}, 0}; }
// Golden-section search for an extremum of f on [a,b].
template <typename F> double golden(F f, double a, double b, bool maximum) {
    const double g = 0.6180339887498949;
    double c = b - g * (b - a), d = a + g * (b - a);
    for (int i = 0; i < 100 && std::abs(b - a) > 1e-12 * std::max(1.0, std::abs(a)); ++i) {
        const double fc = f(c), fd = f(d);
        if (maximum ? fc > fd : fc < fd) { b = d; } else { a = c; }
        c = b - g * (b - a); d = a + g * (b - a);
    }
    return 0.5 * (a + b);
}
} // namespace

double GraphApp::pixelToX(const ViewWindow& v, int px) const { return v.xmin + (v.xmax - v.xmin) * (px - PlotLeft) / W; }

void GraphApp::enter(Context& ctx) { cache_.valid = false; (void)ctx; if (screen_ != Screen::Plot) screen_ = Screen::List; menu_.close(); }

int GraphApp::currentFunction(Context& ctx) const {
    const auto& f = ctx.ws.functions;
    if (traceFn_ >= 0 && traceFn_ < 6 && f[static_cast<std::size_t>(traceFn_)].selected && !f[static_cast<std::size_t>(traceFn_)].expr.empty() && f[static_cast<std::size_t>(traceFn_)].type != GraphType::Polar) return traceFn_;
    for (int i = 0; i < 6; ++i) if (f[static_cast<std::size_t>(i)].selected && !f[static_cast<std::size_t>(i)].expr.empty() && f[static_cast<std::size_t>(i)].type != GraphType::Polar) return i;
    return -1;
}

void GraphApp::refresh(Context& ctx) {
    const auto& v = ctx.ws.view;
    bool same = cache_.valid && sameView(cache_.view, v);
    for (std::size_t i = 0; i < 6 && same; ++i) {
        const auto& f = ctx.ws.functions[i];
        same = cache_.expr[i] == f.expr && cache_.type[i] == f.type && cache_.on[i] == f.selected;
    }
    if (same) return;
    cache_.view = v;
    ctx.ws.apply();
    for (std::size_t i = 0; i < 6; ++i) {
        const auto& f = ctx.ws.functions[i];
        cache_.expr[i] = f.expr; cache_.type[i] = f.type; cache_.on[i] = f.selected;
        cache_.y[i].clear(); cache_.polar[i].clear();
        if (!f.selected || f.expr.empty()) continue;
        try {
            if (f.type == GraphType::Polar) {
                // r=f(theta) is evaluated in radians (theta range 0..2pi by default).
                const AngleMode saved = angleMode();
                setAngleMode(AngleMode::Radians);
                const CompiledExpression r(f.expr, "theta");
                const double step = v.tstep > 0 ? v.tstep : 0.05;
                const int n = std::min(4000, static_cast<int>((v.tmax - v.tmin) / step) + 1);
                for (int k = 0; k <= n; ++k) {
                    const double t = v.tmin + k * step; double rv;
                    if (r.tryEval(t, rv, &ctx.ws.memory)) cache_.polar[i].emplace_back(static_cast<float>(rv * std::cos(t)), static_cast<float>(rv * std::sin(t)));
                    else cache_.polar[i].emplace_back(NaNf, NaNf);
                }
                setAngleMode(saved);
            } else {
                const CompiledExpression g(f.expr);
                cache_.y[i].resize(W + 1);
                for (int px = 0; px <= W; ++px) {
                    double yv;
                    cache_.y[i][static_cast<std::size_t>(px)] = g.tryEval(pixelToX(v, px), yv, &ctx.ws.memory) && std::isfinite(yv) ? static_cast<float>(yv) : NaNf;
                }
            }
        } catch (const std::exception&) {
            cache_.y[i].clear(); cache_.polar[i].clear();   // syntax error: function skipped (shown in the list)
        }
    }
    cache_.valid = true;
}

void GraphApp::zoom(Context& ctx, int kind) {
    ViewWindow& v = ctx.ws.view;
    const double cx = trace_ ? pixelToX(v, traceX_) : 0.5 * (v.xmin + v.xmax), cy = 0.5 * (v.ymin + v.ymax);
    const double hw = 0.5 * (v.xmax - v.xmin), hh = 0.5 * (v.ymax - v.ymin);
    switch (kind) {
    case 0: v.xmin = cx - hw / 2; v.xmax = cx + hw / 2; v.ymin = cy - hh / 2; v.ymax = cy + hh / 2; break;
    case 1: v.xmin = cx - hw * 2; v.xmax = cx + hw * 2; v.ymin = cy - hh * 2; v.ymax = cy + hh * 2; break;
    case 2: {
        refresh(ctx);
        std::vector<float> all;
        for (const auto& ys : cache_.y) for (float y : ys) if (std::isfinite(y)) all.push_back(y);
        if (all.empty()) throw MathError("Math ERROR: nessuna funzione da scalare");
        std::sort(all.begin(), all.end());
        double lo = all[all.size() / 20], hi = all[all.size() - 1 - all.size() / 20];
        if (hi - lo < 1e-9) { lo -= 1; hi += 1; }
        const double pad = (hi - lo) * 0.08;
        v.ymin = lo - pad; v.ymax = hi + pad;
        break;
    }
    case 3: v.xmin = -10; v.xmax = 10; v.xscl = 1; v.ymin = -10; v.ymax = 10; v.yscl = 1; break;
    case 4: v.xmin = -3 * kPi; v.xmax = 3 * kPi; v.xscl = kPi / 2; v.ymin = -1.6; v.ymax = 1.6; v.yscl = 0.5; break;
    case 5: { const double perPx = (v.xmax - v.xmin) / W; const double half = perPx * H / 2; v.ymin = cy - half; v.ymax = cy + half; break; }
    default: v = ViewWindow{}; break;
    }
    ctx.markDirty();
    cache_.valid = false;
}

void GraphApp::gsolve(Context& ctx, int kind) {
    refresh(ctx);
    const ViewWindow& v = ctx.ws.view;
    solutions_.clear(); solutionIndex_ = 0; shadeIntegral_ = false;
    const int fi = currentFunction(ctx);
    if (fi < 0) throw MathError("Math ERROR: nessuna funzione Y= selezionata");
    const CompiledExpression f(ctx.ws.functions[static_cast<std::size_t>(fi)].expr);
    const CalculatorMemory* mem = &ctx.ws.memory;
    auto val = [&](double x) { double y; return f.tryEval(x, y, mem) ? y : std::numeric_limits<double>::quiet_NaN(); };
    auto bisect = [&](auto g, double a, double b) {
        double ga = g(a);
        for (int i = 0; i < 200 && b - a > 1e-13 * std::max(1.0, std::abs(a)); ++i) {
            const double m = 0.5 * (a + b), gm = g(m);
            if (!std::isfinite(gm)) break;
            if ((gm < 0) == (ga < 0)) { a = m; ga = gm; } else b = m;
        }
        return 0.5 * (a + b);
    };
    traceFn_ = fi;
    if (kind == 0 || kind == 4) {
        int gi = -1;
        if (kind == 4) {
            for (int i = 0; i < 6; ++i) if (i != fi && ctx.ws.functions[static_cast<std::size_t>(i)].selected && !ctx.ws.functions[static_cast<std::size_t>(i)].expr.empty() && ctx.ws.functions[static_cast<std::size_t>(i)].type != GraphType::Polar) { gi = i; break; }
            if (gi < 0) throw MathError("Math ERROR: ISECT richiede due funzioni selezionate");
        }
        const CompiledExpression g(gi >= 0 ? ctx.ws.functions[static_cast<std::size_t>(gi)].expr : std::string("0"));
        auto diff = [&](double x) { double a, b; if (!f.tryEval(x, a, mem) || !g.tryEval(x, b, mem)) return std::numeric_limits<double>::quiet_NaN(); return a - b; };
        double px = v.xmin, py = diff(px);
        for (int k = 1; k <= 4 * W; ++k) {
            const double x = v.xmin + (v.xmax - v.xmin) * k / (4.0 * W), y = diff(x);
            if (std::isfinite(py) && std::isfinite(y)) {
                if (py == 0) solutions_.emplace_back(px, val(px));
                else if ((py < 0) != (y < 0) && y != 0) {
                    const double r = bisect(diff, px, x);
                    // Reject poles: the residual must be small compared with the bracketing values.
                    if (std::abs(diff(r)) < 1e-6 * std::max({1.0, std::abs(py), std::abs(y)})) solutions_.emplace_back(r, val(r));
                }
            }
            px = x; py = y;
        }
        solveLabel_ = kind == 0 ? "ROOT" : "ISCT";
    } else if (kind == 1 || kind == 2) {
        const bool mx = kind == 1;
        const int N = 2 * W;
        std::vector<double> xs(static_cast<std::size_t>(N + 1)), ys(static_cast<std::size_t>(N + 1));
        for (int k = 0; k <= N; ++k) { xs[static_cast<std::size_t>(k)] = v.xmin + (v.xmax - v.xmin) * k / N; ys[static_cast<std::size_t>(k)] = val(xs[static_cast<std::size_t>(k)]); }
        for (int k = 1; k < N; ++k) {
            const double a = ys[static_cast<std::size_t>(k - 1)], b = ys[static_cast<std::size_t>(k)], c = ys[static_cast<std::size_t>(k + 1)];
            if (!std::isfinite(a) || !std::isfinite(b) || !std::isfinite(c)) continue;
            if (mx ? (b >= a && b > c) : (b <= a && b < c)) {
                const double x = golden(val, xs[static_cast<std::size_t>(k - 1)], xs[static_cast<std::size_t>(k + 1)], mx);
                solutions_.emplace_back(x, val(x));
            }
        }
        solveLabel_ = mx ? "MAX" : "MIN";
    } else if (kind == 3) {
        const double y = val(0);
        if (!std::isfinite(y)) throw MathError("Math ERROR: funzione non definita in x=0");
        solutions_.emplace_back(0.0, y);
        solveLabel_ = "Y-ICPT";
    }
    if (solutions_.empty()) throw MathError("Math ERROR: nessuna soluzione nella finestra");
    trace_ = false;
}

void GraphApp::key(Context& ctx, const KeyEvent& ev) {
    auto& fns = ctx.ws.functions;
    if (menu_.open) {
        const int choice = menu_.key(ev);
        if (choice < 0) return;
        if (menuKind_ == 1) zoom(ctx, choice);
        else if (menuKind_ == 2) {
            if (choice == 5) {
                const ViewWindow& v = ctx.ws.view;
                integral_.fields = {num("Lower", v.xmin + (v.xmax - v.xmin) * 0.25), num("Upper", v.xmin + (v.xmax - v.xmin) * 0.75)};
                integral_.selected = 0; screen_ = Screen::Integral;
            } else gsolve(ctx, choice);
        } else if (menuKind_ == 3) { sketch_ = choice == 2 ? 0 : choice + 1; if (sketch_) trace_ = true; }
        return;
    }
    switch (screen_) {
    case Screen::List:
        switch (ev.key) {
        case Key::Up: row_ = (row_ + 5) % 6; return;
        case Key::Down: row_ = (row_ + 1) % 6; return;
        case Key::F1: fns[static_cast<std::size_t>(row_)].selected = !fns[static_cast<std::size_t>(row_)].selected; ctx.markDirty(); return;
        case Key::F2: case Key::DelForward: fns[static_cast<std::size_t>(row_)].expr.clear(); ctx.markDirty(); return;
        case Key::F3: { auto& t = fns[static_cast<std::size_t>(row_)].type; t = static_cast<GraphType>((static_cast<int>(t) + 1) % 6); ctx.markDirty(); return; }
        case Key::F4: { auto& c = fns[static_cast<std::size_t>(row_)].color; c = static_cast<std::uint8_t>((c + 1) % 6); ctx.markDirty(); return; }
        case Key::F5: {
            const ViewWindow& v = ctx.ws.view;
            window_.fields = {num("Xmin", v.xmin), num("Xmax", v.xmax), num("Xscale", v.xscl), num("Ymin", v.ymin), num("Ymax", v.ymax), num("Yscale", v.yscl),
                              num("\xCE\xB8min", v.tmin), num("\xCE\xB8max", v.tmax), num("\xCE\xB8ptch", v.tstep),
                              {"Griglia", v.grid ? "On" : "Off", FormField::Type::Choice, {"On", "Off"}, v.grid ? 0 : 1}};
            window_.selected = 0; screen_ = Screen::Window; return;
        }
        case Key::F6: screen_ = Screen::Plot; trace_ = false; solutions_.clear(); cache_.valid = false; return;
        case Key::Exe: case Key::Right:
            editor_.clear();
            if (!fns[static_cast<std::size_t>(row_)].expr.empty()) editor_.setExpression(fns[static_cast<std::size_t>(row_)].expr);
            screen_ = Screen::Edit; return;
        case Key::Char:
            if (ev.ctrl) return;
            editor_.clear(); editor_.insert(ev.ch); screen_ = Screen::Edit; return;
        case Key::Exit: ctx.home(); return;
        default: return;
        }
    case Screen::Edit:
        switch (ev.key) {
        case Key::Exe: {
            const std::string text = editor_.empty() ? std::string() : editor_.toExpression();
            if (!text.empty()) (void)parseExpression(text);       // syntax check before storing
            fns[static_cast<std::size_t>(row_)].expr = text;
            fns[static_cast<std::size_t>(row_)].selected = !text.empty();
            ctx.markDirty(); screen_ = Screen::List; row_ = (row_ + 1) % 6; return;
        }
        case Key::Exit: screen_ = Screen::List; return;
        case Key::Char: if (ev.ctrl) return; editor_.insert(ev.ch); return;
        case Key::Left: editor_.moveLeft(); return;
        case Key::Right: editor_.moveRight(); return;
        case Key::Up: editor_.moveUp(); return;
        case Key::Down: editor_.moveDown(); return;
        case Key::Del: editor_.backspace(); return;
        case Key::DelForward: editor_.eraseForward(); return;
        case Key::AC: editor_.clear(); return;
        case Key::F1: editor_.insertFraction(); return;
        case Key::F2: editor_.insertRadical(); return;
        case Key::F3: editor_.insertPower(); return;
        case Key::F4: editor_.insertText(fns[static_cast<std::size_t>(row_)].type == GraphType::Polar ? "theta" : "x"); return;
        case Key::F5: editor_.insertAbs(); return;
        default: return;
        }
    case Screen::Window:
        if (window_.editing()) { (void)window_.key(ctx, ev); return; }
        if (ev.is(Key::Exit) || ev.is(Key::F6)) {
            ViewWindow nv = ctx.ws.view;
            nv.xmin = window_.number(ctx, 0); nv.xmax = window_.number(ctx, 1); nv.xscl = window_.number(ctx, 2);
            nv.ymin = window_.number(ctx, 3); nv.ymax = window_.number(ctx, 4); nv.yscl = window_.number(ctx, 5);
            nv.tmin = window_.number(ctx, 6); nv.tmax = window_.number(ctx, 7); nv.tstep = window_.number(ctx, 8);
            nv.grid = window_.fields[9].choice == 0;
            if (!(nv.xmin < nv.xmax) || !(nv.ymin < nv.ymax)) throw MathError("Argument ERROR: Xmin<Xmax e Ymin<Ymax");
            if (!(nv.tstep > 0) || !(nv.tmin < nv.tmax)) throw MathError("Argument ERROR: θmin<θmax e θptch>0");
            ctx.ws.view = nv; ctx.markDirty(); cache_.valid = false;
            screen_ = ev.is(Key::F6) ? Screen::Plot : Screen::List; return;
        }
        if (ev.is(Key::F1)) { ctx.ws.view = ViewWindow{}; ctx.markDirty(); screen_ = Screen::List; key(ctx, {Key::F5}); return; }
        (void)window_.key(ctx, ev);
        return;
    case Screen::Integral:
        if (integral_.editing()) { (void)integral_.key(ctx, ev); return; }
        if (ev.is(Key::Exit)) { screen_ = Screen::Plot; return; }
        if (ev.is(Key::F1) || ev.is(Key::F6)) {
            const int fi = currentFunction(ctx);
            if (fi < 0) throw MathError("Math ERROR: nessuna funzione Y= selezionata");
            intA_ = integral_.number(ctx, 0); intB_ = integral_.number(ctx, 1);
            if (!(intA_ < intB_)) throw MathError("Argument ERROR: Lower<Upper");
            ctx.ws.apply();
            intValue_ = integrateAdaptive(CompiledExpression(fns[static_cast<std::size_t>(fi)].expr), intA_, intB_, 1e-10, &ctx.ws.memory);
            traceFn_ = fi; shadeIntegral_ = true; solutions_.clear(); trace_ = false;
            ctx.ws.memory.setAnswer(intValue_);
            screen_ = Screen::Plot; return;
        }
        (void)integral_.key(ctx, ev);
        return;
    case Screen::Plot: {
        ViewWindow& v = ctx.ws.view;
        const double dx = (v.xmax - v.xmin) / 8, dy = (v.ymax - v.ymin) / 8;
        if (!solutions_.empty()) {
            if (ev.is(Key::Right)) { solutionIndex_ = (solutionIndex_ + 1) % static_cast<int>(solutions_.size()); return; }
            if (ev.is(Key::Left)) { solutionIndex_ = (solutionIndex_ + static_cast<int>(solutions_.size()) - 1) % static_cast<int>(solutions_.size()); return; }
            if (ev.is(Key::Exe) || ev.is(Key::Exit)) { solutions_.clear(); return; }
        }
        switch (ev.key) {
        case Key::F1: trace_ = !trace_; sketch_ = 0; solutions_.clear(); if (trace_) { traceX_ = W / 2; if (currentFunction(ctx) >= 0) traceFn_ = currentFunction(ctx); } return;
        case Key::F2: menu_.show("ZOOM", kZoom); menuKind_ = 1; return;
        case Key::F3: screen_ = Screen::List; key(ctx, {Key::F5}); return;
        case Key::F4: menu_.show("SKETCH", kSketch); menuKind_ = 3; return;
        case Key::F5: menu_.show("G-SOLVE", kSolve); menuKind_ = 2; return;
        case Key::F6: ctx.open(AppId::Table); return;
        case Key::Exit: if (trace_) trace_ = false; else if (shadeIntegral_) shadeIntegral_ = false; else screen_ = Screen::List; return;
        case Key::Left: if (trace_) traceX_ = std::max(0, traceX_ - (ev.ctrl ? 10 : 1)); else { v.xmin -= dx; v.xmax -= dx; cache_.valid = false; ctx.markDirty(); } return;
        case Key::Right: if (trace_) traceX_ = std::min(W, traceX_ + (ev.ctrl ? 10 : 1)); else { v.xmin += dx; v.xmax += dx; cache_.valid = false; ctx.markDirty(); } return;
        case Key::Up: case Key::Down:
            if (trace_) {
                // Next/previous drawable Y= function.
                for (int k = 1; k <= 6; ++k) {
                    const int i = (traceFn_ + (ev.is(Key::Up) ? 6 - k : k)) % 6;
                    const auto& f = fns[static_cast<std::size_t>(i)];
                    if (f.selected && !f.expr.empty() && f.type != GraphType::Polar) { traceFn_ = i; break; }
                }
            } else { const double s = ev.is(Key::Up) ? dy : -dy; v.ymin += s; v.ymax += s; cache_.valid = false; ctx.markDirty(); }
            return;
        case Key::Char:
            if (ev.ch == '+') zoom(ctx, 0); else if (ev.ch == '-') zoom(ctx, 1);
            return;
        default: return;
        }
    }
    }
}

void GraphApp::drawPlot(Context& ctx, gfx::Canvas& c) {
    refresh(ctx);
    const ViewWindow& v = ctx.ws.view;
    auto sx = [&](double x) { return PlotLeft + static_cast<int>(std::lround((x - v.xmin) / (v.xmax - v.xmin) * W)); };
    auto sy = [&](double y) { return PlotBottom - static_cast<int>(std::lround((y - v.ymin) / (v.ymax - v.ymin) * H)); };
    c.setClip({PlotLeft, PlotTop, W + 1, H + 1});
    // Grid dots at the scale marks (skipped when too dense).
    if (v.grid && v.xscl > 0 && v.yscl > 0 && (v.xmax - v.xmin) / v.xscl < 80 && (v.ymax - v.ymin) / v.yscl < 80) {
        for (double gx = std::ceil(v.xmin / v.xscl) * v.xscl; gx <= v.xmax; gx += v.xscl)
            for (double gy = std::ceil(v.ymin / v.yscl) * v.yscl; gy <= v.ymax; gy += v.yscl) c.pixel(sx(gx), sy(gy), Faint);
    }
    // Axes with ticks.
    const int ax = sx(0), ay = sy(0);
    if (v.ymin <= 0 && v.ymax >= 0) {
        c.hLine(PlotLeft, ay, W + 1, Ink);
        if (v.xscl > 0 && (v.xmax - v.xmin) / v.xscl < 100) for (double gx = std::ceil(v.xmin / v.xscl) * v.xscl; gx <= v.xmax; gx += v.xscl) c.vLine(sx(gx), ay - 2, 5, Ink);
    }
    if (v.xmin <= 0 && v.xmax >= 0) {
        c.vLine(ax, PlotTop, H + 1, Ink);
        if (v.yscl > 0 && (v.ymax - v.ymin) / v.yscl < 100) for (double gy = std::ceil(v.ymin / v.yscl) * v.yscl; gy <= v.ymax; gy += v.yscl) c.hLine(ax - 2, sy(gy), 5, Ink);
    }
    // Integral shading.
    if (shadeIntegral_ && traceFn_ >= 0 && !cache_.y[static_cast<std::size_t>(traceFn_)].empty()) {
        const auto& ys = cache_.y[static_cast<std::size_t>(traceFn_)];
        const std::uint16_t shade = gfx::Canvas::blend(Graph[ctx.ws.functions[static_cast<std::size_t>(traceFn_)].color], 0xFFFF, 90);
        for (int px = std::max(0, sx(intA_)); px <= std::min(W, sx(intB_)); ++px) {
            const float y = ys[static_cast<std::size_t>(px)];
            if (!std::isfinite(y)) continue;
            const int y0 = std::clamp(sy(0), PlotTop, PlotBottom), y1 = std::clamp(sy(y), PlotTop, PlotBottom);
            c.vLine(PlotLeft + px, std::min(y0, y1), std::abs(y1 - y0) + 1, shade);
        }
    }
    for (std::size_t i = 0; i < 6; ++i) {
        const auto& f = ctx.ws.functions[i];
        if (!f.selected || f.expr.empty()) continue;
        const std::uint16_t col = Graph[f.color % 6];
        if (f.type == GraphType::Polar) {
            const auto& pts = cache_.polar[i];
            for (std::size_t k = 1; k < pts.size(); ++k) {
                if (!std::isfinite(pts[k - 1].first) || !std::isfinite(pts[k].first)) continue;
                c.line(sx(pts[k - 1].first), sy(pts[k - 1].second), sx(pts[k].first), sy(pts[k].second), col);
            }
            continue;
        }
        const auto& ys = cache_.y[i];
        if (ys.empty()) continue;
        if (isInequality(f.type)) {
            // Hatched region above (Y>, Y>=) or below (Y<, Y<=) the boundary.
            const bool above = f.type == GraphType::YGreater || f.type == GraphType::YGreaterEq;
            const std::uint16_t shade = gfx::Canvas::blend(col, 0xFFFF, 70);
            for (int px = 0; px <= W; ++px) {
                const float y = ys[static_cast<std::size_t>(px)];
                if (!std::isfinite(y)) continue;
                const int yb = std::clamp(sy(y), PlotTop, PlotBottom);
                for (int yy = above ? PlotTop : yb; yy <= (above ? yb : PlotBottom); ++yy) if (((px + yy) & 3) == 0) c.pixel(PlotLeft + px, yy, shade);
            }
        }
        const bool dashed = f.type == GraphType::YGreater || f.type == GraphType::YLess;
        for (int px = 1; px <= W; ++px) {
            const float a = ys[static_cast<std::size_t>(px - 1)], b = ys[static_cast<std::size_t>(px)];
            if (!std::isfinite(a) || !std::isfinite(b)) continue;
            const int ya = sy(a), yb = sy(b);
            if (std::abs(yb - ya) > H) continue;               // asymptote: do not connect
            if (dashed && (px / 4) % 2) continue;              // strict inequality: dashed boundary
            c.line(PlotLeft + px - 1, ya, PlotLeft + px, yb, col);
            c.line(PlotLeft + px - 1, ya + 1, PlotLeft + px, yb + 1, col);   // 2 px wide for legibility
        }
    }
    c.resetClip();
    // Trace / sketch / solutions overlays.
    auto infoBox = [&](const std::string& top, const std::string& bottom) {
        if (!top.empty()) { c.rect(0, PlotTop, ui::ScreenW, 15, 0xFFFF, true); ui::textFit(c, 3, PlotTop + 2, top, ui::ScreenW - 6, Ink, font::Face::Small); }
        if (!bottom.empty()) { c.rect(0, PlotBottom - 14, ui::ScreenW, 15, 0xFFFF, true); ui::textFit(c, 3, PlotBottom - 12, bottom, ui::ScreenW - 6, Ink, font::Face::Small); }
    };
    const DisplayFormat df{DisplayMode::Norm1, 0};
    if (trace_ && traceFn_ >= 0 && !cache_.y[static_cast<std::size_t>(traceFn_)].empty()) {
        const double xv = pixelToX(v, traceX_);
        double yv = std::numeric_limits<double>::quiet_NaN();
        try { yv = CompiledExpression(ctx.ws.functions[static_cast<std::size_t>(traceFn_)].expr)(xv, &ctx.ws.memory); } catch (...) {}
        const int px = PlotLeft + traceX_;
        std::string bottom = "X=" + formatReal(xv, df) + "  Y=" + (std::isfinite(yv) ? formatReal(yv, df) : std::string("--"));
        if (std::isfinite(yv)) {
            const int py = sy(yv);
            c.vLine(px, py - 6, 13, Error); c.hLine(px - 6, py, 13, Error);
            if (sketch_) {
                const double d = differentiate(CompiledExpression(ctx.ws.functions[static_cast<std::size_t>(traceFn_)].expr), xv, &ctx.ws.memory);
                const double m = sketch_ == 1 ? d : (d == 0 ? std::numeric_limits<double>::infinity() : -1.0 / d);
                c.setClip({PlotLeft, PlotTop, W + 1, H + 1});
                if (std::isfinite(m)) c.line(sx(v.xmin), sy(yv + m * (v.xmin - xv)), sx(v.xmax), sy(yv + m * (v.xmax - xv)), Warn);
                else c.vLine(px, PlotTop, H, Warn);
                c.resetClip();
                bottom = (sketch_ == 1 ? "Tangente y=" : "Normale y=") + formatReal(m, df) + "x+" + formatReal(yv - m * xv, df);
            }
        }
        infoBox(std::string("Y") + std::to_string(traceFn_ + 1) + "=" + ctx.ws.functions[static_cast<std::size_t>(traceFn_)].expr, bottom);
    } else if (!solutions_.empty()) {
        const auto& s = solutions_[static_cast<std::size_t>(solutionIndex_)];
        const int px = sx(s.first), py = sy(s.second);
        c.circle(px, py, 4, Error, false); c.circle(px, py, 3, Error, false);
        char idx[64]; std::snprintf(idx, sizeof(idx), "  [%d/%d \xE2\x97\x80\xE2\x96\xB6]", solutionIndex_ + 1, static_cast<int>(solutions_.size()));
        infoBox(solveLabel_ + " Y" + std::to_string(traceFn_ + 1) + (solutions_.size() > 1 ? idx : ""), "X=" + formatReal(s.first, df) + "  Y=" + formatReal(s.second, df));
    } else if (shadeIntegral_) {
        infoBox("\xE2\x88\xAB" "dx Y" + std::to_string(traceFn_ + 1) + "  [" + formatReal(intA_, df) + ", " + formatReal(intB_, df) + "]", "\xE2\x88\xAB" "dx=" + formatReal(intValue_, ctx.ws.settings.display));
    }
}

void GraphApp::draw(Context& ctx, gfx::Canvas& c) {
    ui::clear(c);
    ctx.drawStatus(c, "GRAPH");
    const auto& fns = ctx.ws.functions;
    switch (screen_) {
    case Screen::List: case Screen::Edit: {
        const int top = ui::ContentTop + 2, rowH = screen_ == Screen::Edit ? 34 : 44;
        for (int i = 0; i < 6; ++i) {
            const auto& f = fns[static_cast<std::size_t>(i)];
            const int y = top + i * rowH;
            const bool sel = i == row_;
            if (sel) c.rect(0, y, ui::ScreenW, rowH, SelectSoft, true);
            c.rect(4, y + rowH / 2 - 6, 4, 12, Graph[f.color % 6], true);
            char head[16]; std::snprintf(head, sizeof(head), "%s%d", f.type == GraphType::Polar ? "r" : "Y", i + 1);
            const std::string label = std::string(head) + (graphTypeLabel(f.type) + 1);
            // The '=' (or relation) is highlighted when the function is selected for drawing (CG50 style).
            const int lw = font::measure(font::Face::Medium, label.c_str()).width;
            if (f.selected && !f.expr.empty()) c.rect(12, y + rowH / 2 - 9, lw + 4, 18, Accent, true);
            ui::text(c, 14, y + rowH / 2 - 8, label.c_str(), f.selected && !f.expr.empty() ? 0xFFFF : Ink);
            if (!f.expr.empty()) {
                NaturalEditor view;
                try { view.setExpression(f.expr); } catch (...) { view.clear(); view.insertText("?"); }
                const auto sz = math::measure(view.root());
                auto l = math::layout(view.root(), 20 + lw, y + rowH / 2 - sz.height / 2 + sz.baseline, ui::ScreenW - 24 - lw, rowH);
                math::draw(c, l, Ink, Cursor, Placeholder);
                bool valid = true; try { (void)parseExpression(f.expr); } catch (...) { valid = false; }
                if (!valid) ui::textRight(c, ui::ScreenW - 4, y + 2, "Syntax!", Error, font::Face::Small);
            }
        }
        if (screen_ == Screen::Edit) {
            const int ey = top + 6 * rowH + 2;
            c.rect(0, ey, ui::ScreenW, ui::ContentBottom - ey, StatusBg, true);
            c.hLine(0, ey, ui::ScreenW, Faint);
            const math::CursorTarget cur{editor_.activeRowPtr(), editor_.cursorIndex()};
            const auto sz = math::measure(editor_.root());
            auto l = math::layout(editor_.root(), 6, ey + 4 + sz.baseline, ui::ScreenW - 12, ui::ContentBottom - ey - 4, math::Align::Left, &cur);
            math::draw(c, l, Ink, Cursor, Placeholder);
            const char* keys[6] = {"a/b", "\xE2\x88\x9A\xE2\x96\xA1", "\xE2\x96\xA1^", fns[static_cast<std::size_t>(row_)].type == GraphType::Polar ? "\xCE\xB8" : "X", "|\xE2\x96\xA1|", nullptr};
            ui::softKeys(c, keys);
        } else {
            const char* keys[6] = {"SELECT", "DELETE", "TYPE", "COLOR", "V-WIN", "DRAW"};
            ui::softKeys(c, keys);
        }
        break;
    }
    case Screen::Window: {
        c.rect(0, ui::ContentTop, ui::ScreenW, 20, SelectSoft, true);
        ui::text(c, 6, ui::ContentTop + 2, "View Window", Ink);
        window_.draw(c, ui::ContentTop + 22, ui::ContentBottom - 2);
        const char* keys[6] = {"INITIAL", nullptr, nullptr, nullptr, nullptr, "DRAW"};
        ui::softKeys(c, keys);
        break;
    }
    case Screen::Integral: {
        c.rect(0, ui::ContentTop, ui::ScreenW, 20, SelectSoft, true);
        ui::text(c, 6, ui::ContentTop + 2, "\xE2\x88\xAB" "dx: limiti di integrazione", Ink);
        integral_.draw(c, ui::ContentTop + 22, ui::ContentTop + 66);
        const char* keys[6] = {"CALC", nullptr, nullptr, nullptr, nullptr, "CALC"};
        ui::softKeys(c, keys);
        break;
    }
    case Screen::Plot: {
        drawPlot(ctx, c);
        const char* keys[6] = {trace_ ? "TRACE*" : "TRACE", "ZOOM", "V-WIN", "SKETCH", "G-SOLV", "G\xE2\x86\x94T"};
        ui::softKeys(c, keys);
        break;
    }
    }
    menu_.draw(c);
}
} // namespace fx::app
