// GRAPH verification against mathematically known references:
//  - every drawn point lies on the true curve within one pixel (Y=, X=,
//    polar, parametric) for polynomial, trigonometric, exponential,
//    logarithmic, rational and piecewise functions, in several V-Windows and
//    angle units;
//  - continuous curves are joined (segments of a few pixels), poles and jumps
//    are not (no segment crosses an asymptote), domain edges are reached;
//  - G-Solve, derivatives and integrals against closed-form values.
#include "graph_engine.hpp"
#include "graph_solve.hpp"
#include "math_engine.hpp"
#include "test_util.hpp"
#include "workspace.hpp"
#include <cmath>
#include <functional>
#include <string>
#include <vector>

using namespace fx;
using namespace fx::graph;

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kE = 2.71828182845904523536;
struct Rig {
    Workspace ws;
    GraphEngine engine;
    Rig() { for (std::size_t i = 0; i < ws.functions.size(); ++i) ws.functions[i] = emptyGraphFunction(i); }
    void set(std::size_t i, const std::string& e, GraphType t = GraphType::Y, const std::string& e2 = {}) {
        ws.functions[i].expr = e; ws.functions[i].type = t; ws.functions[i].expr2 = e2; ws.functions[i].selected = true;
    }
    void build() { engine.build({&ws.functions, &ws.memory, &ws, &ws.lists}); }
    const Plot& plot(std::size_t i) const { return engine.plots().at(i); }
    RealFunction fn(std::size_t i) const { const Plot& p = plot(i); return [this, &p](double x, double& y) { return engine.eval(p, x, y); }; }
};
Curve sampleAll(const GraphEngine& e, const Plot& p, const Viewport& vp) {
    Curve c;
    int guard = 0;
    while (!c.done && guard++ < 100000) { int budget = 600; sampleStep(e, p, vp, false, c, budget); }
    return c;
}
struct Pt { int x, y; };
std::vector<std::vector<Pt>> pieces(const Curve& c) {
    std::vector<std::vector<Pt>> out;
    bool open = false;
    for (std::size_t i = 0; i + 1 < c.xy.size(); i += 2) {
        if (c.xy[i] == PenUp) { open = false; continue; }
        if (!open) { out.emplace_back(); open = true; }
        out.back().push_back({c.xy[i], c.xy[i + 1]});
    }
    return out;
}
bool interior(const Viewport& vp, const Pt& p, int margin = 4) {
    return p.x >= vp.left() + margin && p.x <= vp.left() + vp.columns() - 1 - margin && p.y >= vp.top() + margin && p.y <= vp.top() + vp.rows() - 1 - margin;
}
// Distance in pixels between a drawn point and the curve y = f(x) near its column.
double offCurveY(const Viewport& vp, const std::function<double(double)>& f, const Pt& p) {
    double best = 1e9;
    for (int k = -10; k <= 10; ++k) {
        const double x = vp.mathX(p.x + k * 0.05);
        const double y = f(x);
        if (std::isfinite(y)) best = std::min(best, std::abs(vp.screenY(y) - p.y));
    }
    return best;
}
struct Report { int points = 0, bad = 0; double worst = 0; int badSegments = 0; std::size_t pieces = 0; };
// Distance in pixels between a screen point and the curve y = f(x) for x in [a, b].
double offCurve2D(const Viewport& vp, const std::function<double(double)>& f, double mx, double my, double a, double b) {
    double best = 1e9;
    for (int k = 0; k <= 200; ++k) {
        const double x = a + (b - a) * k / 200.0, y = f(x);
        if (std::isfinite(y)) best = std::min(best, std::hypot(vp.screenX(x) - mx, vp.screenY(y) - my));
    }
    return best;
}
// Accuracy of a Y= curve: every point within 1 px of the curve, and the middle
// of every segment within 1.5 px (the polyline follows the curve between points).
Report checkY(const Curve& c, const Viewport& vp, const std::function<double(double)>& f) {
    Report r;
    const auto ps = pieces(c);
    r.pieces = ps.size();
    for (const auto& piece : ps) {
        for (std::size_t i = 0; i < piece.size(); ++i) {
            const Pt& p = piece[i];
            if (!interior(vp, p)) continue;
            ++r.points;
            const double d = offCurveY(vp, f, p);
            r.worst = std::max(r.worst, d);
            if (d > 1.0) ++r.bad;
            if (i > 0 && interior(vp, piece[i - 1])) {
                const Pt& q = piece[i - 1];
                const double a = vp.mathX(std::min(p.x, q.x) - 0.5), b = vp.mathX(std::max(p.x, q.x) + 0.5);
                if (offCurve2D(vp, f, 0.5 * (p.x + q.x), 0.5 * (p.y + q.y), a, b) > 1.5) ++r.badSegments;
            }
        }
    }
    return r;
}
// True if a segment of the curve joins the two sides of the vertical line x = a.
bool crosses(const Curve& c, const Viewport& vp, double a) {
    const double col = vp.screenX(a);
    for (const auto& piece : pieces(c))
        for (std::size_t i = 1; i < piece.size(); ++i)
            if ((piece[i - 1].x < col - 1 && piece[i].x > col + 1) || (piece[i - 1].x > col + 1 && piece[i].x < col - 1)) return true;
    return false;
}
} // namespace

int main() {
    // ================================================================ Y= accuracy and continuity
    {
        struct Case { const char* expr; std::function<double(double)> f; ViewWindow view; AngleMode angle; bool continuous; };
        const ViewWindow init{};
        const ViewWindow wide{-10, 10, 1, -10, 10, 1, 0, 2 * kPi, kPi / 60};
        const ViewWindow logw{-1, 9, 1, -4, 4, 1, 0, 2 * kPi, kPi / 60};
        const ViewWindow degw{-360, 360, 90, -1.6, 1.6, 0.5, 0, 360, 3};
        const ViewWindow small{0.999, 1.001, 0.0005, 0.997, 1.003, 0.001, 0, 2 * kPi, kPi / 60};
        const std::vector<Case> cases = {
            {"x^2-3", [](double x) { return x * x - 3; }, init, AngleMode::Radians, true},
            {"x^3-4x", [](double x) { return x * x * x - 4 * x; }, wide, AngleMode::Radians, true},
            {"2sin(x)", [](double x) { return 2 * std::sin(x); }, init, AngleMode::Radians, true},
            {"cos(3x)+sin(x)", [](double x) { return std::cos(3 * x) + std::sin(x); }, init, AngleMode::Radians, true},
            {"sin(x)", [](double x) { return std::sin(x * kPi / 180); }, degw, AngleMode::Degrees, true},
            {"e^(x)", [](double x) { return std::exp(x); }, init, AngleMode::Radians, true},
            {"e^(-x^2)", [](double x) { return std::exp(-x * x); }, init, AngleMode::Radians, true},
            {"10^(x)-5", [](double x) { return std::pow(10.0, x) - 5; }, ViewWindow{-2, 2, 1, -6, 100, 10, 0, 2 * kPi, kPi / 60}, AngleMode::Radians, true},
            {"ln(x)", [](double x) { return std::log(x); }, logw, AngleMode::Radians, true},
            {"log(x)", [](double x) { return std::log10(x); }, logw, AngleMode::Radians, true},
            {"sqrt(x)", [](double x) { return std::sqrt(x); }, logw, AngleMode::Radians, true},
            {"abs(x-1)-2", [](double x) { return std::abs(x - 1) - 2; }, init, AngleMode::Radians, true},
            {"x^2", [](double x) { return x * x; }, small, AngleMode::Radians, true},
            {"1/x", [](double x) { return 1 / x; }, init, AngleMode::Radians, false},
            {"(x^2-1)/(x-2)", [](double x) { return (x * x - 1) / (x - 2); }, wide, AngleMode::Radians, false},
            {"tan(x)", [](double x) { return std::tan(x); }, init, AngleMode::Radians, false},
            {"tan(x)", [](double x) { return std::tan(x * kPi / 180); }, degw, AngleMode::Degrees, false},
            {"int(x)", [](double x) { return std::trunc(x); }, init, AngleMode::Radians, false},
        };
        for (const Case& t : cases) {
            Rig r;
            r.set(0, t.expr);
            r.ws.view = t.view;
            setAngleMode(t.angle);
            r.build();
            CHECK(r.engine.plots().size() == 1);
            if (r.engine.plots().size() != 1) continue;
            const Viewport vp(t.view);
            const Curve c = sampleAll(r.engine, r.plot(0), vp);
            CHECK(c.done && !c.truncated);
            const Report rep = checkY(c, vp, t.f);
            if (rep.bad || rep.badSegments || rep.points < 20 || (t.continuous && rep.pieces != 1))
                std::printf("  %s: %d points, %d off by >1 px (worst %.2f), %d bad segments, %zu pieces\n", t.expr, rep.points, rep.bad, rep.worst, rep.badSegments, rep.pieces);
            CHECK(rep.points >= 20);
            CHECK(rep.bad == 0);
            CHECK(rep.badSegments == 0);
            if (t.continuous) CHECK(rep.pieces == 1);                   // never interrupted
        }
        setAngleMode(AngleMode::Radians);
    }
    // ================================================================ discontinuities and domains
    {
        Rig r;
        r.set(0, "1/x");
        r.set(1, "tan(x)");
        r.set(2, "(x^2-1)/(x-2)");
        r.set(3, "ln(x)");
        r.set(4, "sqrt(4-x^2)");
        r.set(5, "int(x)");
        r.build();
        const Viewport vp;
        const Curve inv = sampleAll(r.engine, r.plot(0), vp);
        CHECK(!crosses(inv, vp, 0));                                    // no line through the pole
        CHECK(pieces(inv).size() == 2);
        const Curve tg = sampleAll(r.engine, r.plot(1), vp);
        for (double a : {-1.5 * kPi, -0.5 * kPi, 0.5 * kPi, 1.5 * kPi}) CHECK(!crosses(tg, vp, a));
        CHECK(pieces(tg).size() == 5);                                  // 4 asymptotes in [-6.3, 6.3]
        const Curve rat = sampleAll(r.engine, r.plot(2), vp);
        CHECK(!crosses(rat, vp, 2));
        // ln x: nothing for x <= 0, the curve reaches the bottom edge near x = 0
        const Curve ln = sampleAll(r.engine, r.plot(3), vp);
        bool left = false, bottom = false;
        for (const auto& piece : pieces(ln))
            for (const Pt& p : piece) { if (vp.mathX(p.x) < -vp.xdot()) left = true; if (p.y >= vp.top() + vp.rows() - 1 && std::abs(vp.mathX(p.x)) < 0.2) bottom = true; }
        CHECK(!left && bottom);
        // semicircle: one piece, both ends on the x axis at x = -2 and x = 2
        const Curve semi = sampleAll(r.engine, r.plot(4), vp);
        const auto sp = pieces(semi);
        CHECK(sp.size() == 1);
        if (sp.size() == 1) {
            CHECK(std::abs(vp.mathX(sp[0].front().x) + 2) <= vp.xdot() && std::abs(vp.mathY(sp[0].front().y)) <= 1.5 * vp.ydot());
            CHECK(std::abs(vp.mathX(sp[0].back().x) - 2) <= vp.xdot() && std::abs(vp.mathY(sp[0].back().y)) <= 1.5 * vp.ydot());
        }
        // int x: one piece per integer step, no vertical risers
        const Curve steps = sampleAll(r.engine, r.plot(5), vp);
        CHECK(pieces(steps).size() == 7);                               // y = -3 .. 3 visible (Int truncates: (-1,1) is one step)
        for (int k = -4; k <= 4; ++k) if (k) CHECK(!crosses(steps, vp, k));
    }
    // ================================================================ X=, polar and parametric accuracy
    {
        Rig r;
        r.set(0, "y^2-2", GraphType::X);
        r.set(1, "2", GraphType::Polar);
        r.set(2, "1+cos(theta)", GraphType::Polar);
        r.set(3, "3cos(t)", GraphType::Param, "2sin(t)");
        r.ws.view = {-4.5, 4.5, 1, -4.5 * 281 / 319, 4.5 * 281 / 319, 1, 0, 2 * kPi, kPi / 120};   // Xdot = Ydot (square)
        r.build();
        const Viewport vp(r.ws.view);
        CHECK(std::abs(vp.xdot() - vp.ydot()) < 1e-3 * vp.xdot());
        int bad = 0, n = 0;
        // X = y^2 - 2: x of every point matches its y
        for (const auto& piece : pieces(sampleAll(r.engine, r.plot(0), vp)))
            for (const Pt& p : piece) {
                if (!interior(vp, p)) continue;
                double best = 1e9;
                for (int k = -10; k <= 10; ++k) { const double y = vp.mathY(p.y + k * 0.05); best = std::min(best, std::abs(vp.screenX(y * y - 2) - p.x)); }
                ++n; if (best > 1.0) ++bad;
            }
        CHECK(n > 50 && bad == 0);
        // r = 2: every point at distance 2 from the origin
        n = bad = 0;
        const Curve circle = sampleAll(r.engine, r.plot(1), vp);
        for (const auto& piece : pieces(circle))
            for (const Pt& p : piece) { ++n; if (std::abs(std::hypot(p.x - vp.screenX(0), p.y - vp.screenY(0)) - 2 / vp.xdot()) > 1.0) ++bad; }
        CHECK(n > 100 && bad == 0);
        CHECK(pieces(circle).size() == 1);                              // closed, never interrupted
        // cardioid r = 1 + cos θ: every point within 1 px of the curve (2D distance)
        n = bad = 0;
        for (const auto& piece : pieces(sampleAll(r.engine, r.plot(2), vp)))
            for (const Pt& p : piece) {
                double best = 1e9;
                for (int k = 0; k < 7200; ++k) {
                    const double th = 2 * kPi * k / 7200, rr = 1 + std::cos(th);
                    best = std::min(best, std::hypot(vp.screenX(rr * std::cos(th)) - p.x, vp.screenY(rr * std::sin(th)) - p.y));
                }
                ++n; if (best > 1.0) ++bad;
            }
        CHECK(n > 100 && bad == 0);
        // ellipse x = 3 cos t, y = 2 sin t: (x/3)^2 + (y/2)^2 = 1
        n = bad = 0;
        for (const auto& piece : pieces(sampleAll(r.engine, r.plot(3), vp)))
            for (const Pt& p : piece) {
                const double x = vp.mathX(p.x), y = vp.mathY(p.y);
                const double t = std::atan2(y / 2, x / 3);
                ++n; if (std::hypot(vp.screenX(3 * std::cos(t)) - p.x, vp.screenY(2 * std::sin(t)) - p.y) > 1.5) ++bad;
            }
        CHECK(n > 100 && bad == 0);
    }
    // ================================================================ several graphs, V-Window changes
    {
        Rig r;
        r.set(0, "x^2");
        r.set(1, "2x+3");
        r.set(2, "e^(x)/5");
        r.build();
        CHECK(r.engine.plots().size() == 3);
        for (const ViewWindow& v : {ViewWindow{}, ViewWindow{-1, 4, 1, -2, 20, 5, 0, 2 * kPi, kPi / 60}, ViewWindow{-50, 50, 10, -100, 2600, 500, 0, 2 * kPi, kPi / 60}}) {
            const Viewport vp(v);
            const std::function<double(double)> f[3] = {[](double x) { return x * x; }, [](double x) { return 2 * x + 3; }, [](double x) { return std::exp(x) / 5; }};
            for (std::size_t i = 0; i < 3; ++i) {
                const Report rep = checkY(sampleAll(r.engine, r.plot(i), vp), vp, f[i]);
                CHECK(rep.points > 10 && rep.bad == 0 && rep.badSegments == 0 && rep.pieces == 1);
            }
        }
    }
    // ================================================================ G-Solve: reference values
    {
        Rig r;
        r.set(0, "ln(x)");
        r.set(1, "e^(x)-2");
        r.set(2, "xe^(-x)");
        r.set(3, "x+1/x");
        r.set(4, "(x^2-1)/(x-2)");
        r.set(5, "sin(x)");
        r.set(6, "e^(x)");
        r.set(7, "2-x");
        r.set(8, "x^2-2x+1");
        r.build();
        auto roots = findRoots(r.fn(0), 0.01, 6.3);
        CHECK(roots.size() == 1 && std::abs(roots[0] - 1) < 1e-12);
        roots = findRoots(r.fn(1), -6.3, 6.3);
        CHECK(roots.size() == 1 && std::abs(roots[0] - std::log(2.0)) < 1e-12);
        auto mx = findExtrema(r.fn(2), -1, 6.3, true);
        CHECK(mx.size() == 1 && std::abs(mx[0] - 1) < 1e-7);
        double y = 0;
        CHECK(r.fn(2)(mx.empty() ? 0 : mx[0], y) && std::abs(y - 1 / kE) < 1e-12);
        auto mn = findExtrema(r.fn(3), 0.1, 6.3, false);
        CHECK(mn.size() == 1 && std::abs(mn[0] - 1) < 1e-7);
        CHECK(r.fn(3)(mn.empty() ? 0 : mn[0], y) && std::abs(y - 2) < 1e-12);
        // rational: roots ±1, the pole at x = 2 (sign change) is not a root
        roots = findRoots(r.fn(4), -6.3, 6.3);
        CHECK(roots.size() == 2 && std::abs(roots[0] + 1) < 1e-12 && std::abs(roots[1] - 1) < 1e-12);
        // sin x: roots k·π in the initial window
        roots = findRoots(r.fn(5), -6.3, 6.3);
        CHECK(roots.size() == 5);
        for (std::size_t k = 0; k < roots.size(); ++k) CHECK(std::abs(roots[k] - (static_cast<double>(k) - 2) * kPi) < 1e-12);
        // e^x = 2 - x: x = 2 - W(e^2) = 0.442854401002389
        const auto is = findIntersections(r.fn(6), r.fn(7), -6.3, 6.3);
        CHECK(is.size() == 1 && std::abs(is[0] - 0.442854401002389) < 1e-12);
        // double root of (x-1)^2 (tangent to the axis)
        roots = findRoots(r.fn(8), -6.3, 6.3);
        CHECK(roots.size() == 1 && std::abs(roots[0] - 1) < 1e-6);
        // X-CAL / Y-CAL
        const auto xs = findValue(r.fn(6), 5, -6.3, 6.3);
        CHECK(xs.size() == 1 && std::abs(xs[0] - std::log(5.0)) < 1e-12);
        // integrals
        CHECK(std::abs(integrate(r.fn(0), 1, kE) - 1) < 1e-10);              // ∫ ln x = x ln x - x
        CHECK(std::abs(integrate(r.fn(5), 0, kPi) - 2) < 1e-10);
        CHECK(std::abs(integrate(r.fn(6), 0, 1) - (kE - 1)) < 1e-10);
        CHECK(std::abs(integrate(r.fn(5), -kPi, kPi)) < 1e-10);
        CHECK(std::abs(area(r.fn(5), -kPi, kPi) - 4) < 1e-9);
        CHECK(std::abs(integrate(r.fn(3), 1, 2) - (1.5 + std::log(2.0))) < 1e-10);
        bool threw = false;
        try { (void)integrate(r.fn(4), 0, 3); } catch (const std::exception&) { threw = true; }   // pole inside
        CHECK(threw);
        // poles: rejected with or without a sign change; integrable singularities
        // and narrow finite peaks are integrated
        Rig q;
        q.set(0, "1/x^2");
        q.set(1, "tan(x)");
        q.set(2, "1/sqrt(abs(x))");
        q.set(3, "1/(x^2+0.01)");
        q.set(4, "int(x)");
        q.build();
        auto fails = [&](std::size_t i, double a, double b) { try { (void)integrate(q.fn(i), a, b); } catch (const std::exception&) { return true; } return false; };
        CHECK(fails(0, -1, 1));
        CHECK(fails(1, 0, kPi));
        CHECK(!fails(1, -1, 1) && std::abs(integrate(q.fn(1), -1, 1)) < 1e-10);
        CHECK(fails(2, -1, 1));                                                 // x = 0 is evaluated: undefined there
        CHECK(!fails(2, -1.0001, 1));                                           // integrable singularity between the nodes
        // split at the singular point: about 4 digits (endpoint singularity, depth-limited quadrature)
        if (!fails(2, -1.0001, 1)) CHECK(std::abs(integrate(q.fn(2), -1.0001, 1) - (2 * std::sqrt(1.0001) + 2)) < 5e-4);
        CHECK(std::abs(integrate(q.fn(3), -1, 1) - 20 * std::atan(10.0)) < 1e-8);
        CHECK(std::abs(integrate(q.fn(4), -2.5, 2.5) - 0) < 1e-12);             // jumps: integrated piece by piece
        CHECK(std::abs(integrate(q.fn(4), 0, 3.5) - (0 + 1 + 2 + 1.5)) < 1e-12);
        CHECK(std::abs(integrate(q.fn(4), 3.5, 0) + 4.5) < 1e-12);              // reversed limits
        // derivatives used by TRACE and Tangent/Norm
        double d = 0;
        CHECK(r.engine.derivative(r.plot(6), 1, d) && std::abs(d - kE) < 1e-8);
        CHECK(r.engine.derivative(r.plot(0), 2, d) && std::abs(d - 0.5) < 1e-9);
        CHECK(r.engine.derivative(r.plot(5), 0, d) && std::abs(d - 1) < 1e-9);
        CHECK(r.engine.derivative(r.plot(4), 0, d) && std::abs(d - 0.25) < 1e-8);      // (x²-4x+1)/(x-2)² at 0
        CHECK(!r.engine.derivative(r.plot(0), -1, d));                                // outside the domain
    }
    // ================================================================ degrees and grads
    {
        Rig r;
        r.set(0, "sin(x)");
        r.build();
        setAngleMode(AngleMode::Degrees);
        auto roots = findRoots(r.fn(0), -360, 360);
        CHECK(roots.size() == 5 && std::abs(roots[0] + 360) < 1e-9 && std::abs(roots[4] - 360) < 1e-9);
        auto mx = findExtrema(r.fn(0), 0, 360, true);
        CHECK(mx.size() == 1 && std::abs(mx[0] - 90) < 1e-5);
        CHECK(std::abs(integrate(r.fn(0), 0, 180) - 360 / kPi) < 1e-8);
        setAngleMode(AngleMode::Gradians);
        mx = findExtrema(r.fn(0), 0, 400, true);
        CHECK(mx.size() == 1 && std::abs(mx[0] - 100) < 1e-5);
        setAngleMode(AngleMode::Radians);
    }
    return testutil::finish("graph_verify_test");
}
