// GRAPH engine: coordinate transform, function texts (range, overwrite),
// plots and variants, composite references through the shared evaluator,
// incremental sampling with discontinuities, inequality boundaries and the
// V-Window / ZOOM arithmetic.
#include "graph_engine.hpp"
#include "test_util.hpp"
#include "workspace.hpp"
#include <cmath>
#include <limits>

using namespace fx;
using namespace fx::graph;

namespace {
constexpr double kPi = 3.14159265358979323846;
struct Rig {
    Workspace ws;
    GraphEngine engine;
    Rig() { for (std::size_t i = 0; i < ws.functions.size(); ++i) ws.functions[i] = emptyGraphFunction(i); }
    GraphSource source() { return {&ws.functions, &ws.memory, &ws, &ws.lists}; }
    void set(std::size_t i, const std::string& e, GraphType t = GraphType::Y, const std::string& e2 = {}) {
        ws.functions[i].expr = e; ws.functions[i].type = t; ws.functions[i].expr2 = e2; ws.functions[i].selected = true;
    }
    void build(const std::vector<std::string>& pasted = {}) { engine.build(source(), pasted); }
};
// Draws a curve completely with the given evaluation budget per call.
Curve sampleAll(const GraphEngine& e, const Plot& p, const Viewport& vp, bool plotOnly = false, int perCall = 1 << 30) {
    Curve c;
    int guard = 0;
    while (!c.done && guard++ < 100000) { int budget = perCall; sampleStep(e, p, vp, plotOnly, c, budget); }
    return c;
}
std::size_t pieces(const Curve& c) {
    std::size_t n = 0; bool inPiece = false;
    for (std::size_t i = 0; i + 1 < c.xy.size(); i += 2) {
        if (c.xy[i] == PenUp) { inPiece = false; continue; }
        if (!inPiece) { ++n; inPiece = true; }
    }
    return n;
}
// True when some segment of the curve joins points on both sides of column x
// with a vertical extent larger than h (a line across an asymptote).
bool crossesColumn(const Curve& c, int x, int h) {
    for (std::size_t i = 0; i + 3 < c.xy.size(); i += 2) {
        if (c.xy[i] == PenUp || c.xy[i + 2] == PenUp) continue;
        const int x0 = c.xy[i], x1 = c.xy[i + 2], y0 = c.xy[i + 1], y1 = c.xy[i + 3];
        if (std::min(x0, x1) <= x && std::max(x0, x1) >= x && std::abs(y1 - y0) > h) return true;
    }
    return false;
}
bool allInside(const Curve& c, const Viewport& vp) {
    for (std::size_t i = 0; i + 1 < c.xy.size(); i += 2) {
        if (c.xy[i] == PenUp) continue;
        if (!vp.inside(c.xy[i], c.xy[i + 1], 4)) return false;
    }
    return true;
}
} // namespace

int main() {
    // ---------------------------------------------------------------- viewport
    {
        const Viewport vp;   // INITIAL
        CHECK_NEAR(vp.xdot(), 12.6 / 319, 1e-15);
        CHECK_NEAR(vp.ydot(), 6.2 / 281, 1e-15);
        CHECK_NEAR(vp.xAt(0), -6.3, 1e-15);
        CHECK_NEAR(vp.xAt(319), 6.3, 1e-12);
        CHECK_NEAR(vp.yAt(0), 3.1, 1e-15);
        CHECK_NEAR(vp.yAt(281), -3.1, 1e-12);
        CHECK_NEAR(vp.screenX(0), 159.5, 1e-12);
        CHECK_NEAR(vp.screenY(0), graphgeo::Top + 140.5, 1e-12);
        CHECK_NEAR(vp.mathX(vp.screenX(1.234)), 1.234, 1e-12);
        CHECK_NEAR(vp.mathY(vp.screenY(-2.5)), -2.5, 1e-12);
        double x0 = -1000, y0 = 100, x1 = 1000, y1 = 100;
        CHECK(vp.clip(x0, y0, x1, y1, 0) && x0 == 0 && x1 == 319 && y0 == 100);
        x0 = -10; y0 = -10; x1 = -5; y1 = 500;
        CHECK(!vp.clip(x0, y0, x1, y1, 0));
        x0 = 10; y0 = -1e9; x1 = 10; y1 = 1e9;   // huge but finite coordinates
        CHECK(vp.clip(x0, y0, x1, y1, 0));
        CHECK_NEAR(y0, graphgeo::Top, 1e-6); CHECK_NEAR(y1, graphgeo::Bottom, 1e-6); CHECK(x0 == 10 && x1 == 10);
        CHECK(vp.inside(0, graphgeo::Top) && !vp.inside(320, graphgeo::Top) && vp.inside(320, graphgeo::Top, 1));
    }
    // ---------------------------------------------------------------- function texts
    {
        FunctionText t = splitFunctionText("(x)^(2)+3x-2,[-2,4]");
        CHECK_EQ_STR(t.body, "(x)^(2)+3x-2");
        CHECK(t.hasRange && t.rangeLo == "-2" && t.rangeHi == "4" && !t.overwrite);
        t = splitFunctionText("Ax^2-3, [A=3,1,-1]");
        CHECK_EQ_STR(t.body, "Ax^2-3");
        CHECK(t.overwrite == 'A' && t.values.size() == 3 && t.values[2] == "-1");
        t = splitFunctionText("max(x,1),[b=1,max(2,3)");                 // closing ] may be omitted
        CHECK(t.overwrite == 'B' && t.values.size() == 2 && t.values[1] == "max(2,3)");
        t = splitFunctionText("sin(x)");
        CHECK(!t.hasRange && !t.overwrite && t.body == "sin(x)");
        CHECK_THROWS_AS(splitFunctionText("x,[1]"), SyntaxError);
        CHECK_THROWS_AS(splitFunctionText("x,[X=1,2]"), SyntaxError);  // X, Y, r, T are not allowed
        CHECK_THROWS_AS(splitFunctionText("x[1,2]"), SyntaxError);
        CHECK_THROWS_AS(splitFunctionText(",[1,2]"), SyntaxError);
        CHECK_THROWS_AS(splitFunctionText("x,[1,2]+1"), SyntaxError);
        CHECK_THROWS_AS(splitFunctionText("x,[A=1,]"), SyntaxError);
        CHECK_THROWS(splitFunctionText("x,[A=1,2,3,4,5,6,7,8,9,10,11]"));
    }
    // ---------------------------------------------------------------- plots and references
    {
        Rig r;
        r.set(0, "x^2");
        r.set(1, "3sin(2theta)", GraphType::Polar);
        r.set(2, "3sin(t)", GraphType::Param, "3cos(t)");
        r.set(3, "y^2", GraphType::X);
        r.set(4, "x", GraphType::YGreaterEq);
        r.set(5, "x+1");
        r.ws.functions[5].selected = false;                              // defined but not drawn
        r.set(6, "1+");                                                 // syntax error
        r.set(7, "@Y1(@Y6)");                                           // composite (Y6 not selected)
        r.build();
        const auto& p = r.engine.plots();
        CHECK(p.size() == 6);                                           // Y1 r2 Xt3 X4 Y5 Y8
        CHECK_EQ_STR(p[0].label, "Y1");
        CHECK_EQ_STR(p[1].label, "r2");
        CHECK_EQ_STR(p[2].label, "Xt3");
        CHECK_EQ_STR(p[3].label, "X4");
        CHECK(p[4].type == GraphType::YGreaterEq);
        CHECK_EQ_STR(p[5].label, "Y8");
        CHECK(!r.engine.error(6).empty() && r.engine.error(0).empty());
        double v = 0;
        CHECK(r.engine.eval(p[0], 3, v) && v == 9);
        CHECK(r.engine.eval(p[5], 2, v) && v == 9);                   // Y1(Y6(2)) = (2+1)^2
        CHECK(r.engine.eval(p[3], -2, v) && v == 4);                  // X = y^2 at y = -2
        double x, y;
        setAngleMode(AngleMode::Radians);
        CHECK(r.engine.point(p[2], kPi / 2, x, y));
        CHECK_NEAR(x, 3, 1e-12); CHECK_NEAR(y, 0, 1e-12);
        CHECK(r.engine.point(p[1], kPi / 4, x, y));                   // r = 3 at 45 degrees
        CHECK_NEAR(x, 3 / std::sqrt(2.0), 1e-12); CHECK_NEAR(y, 3 / std::sqrt(2.0), 1e-12);
        setAngleMode(AngleMode::Degrees);                               // theta follows the angle unit
        CHECK(r.engine.point(p[1], 45, x, y));
        CHECK_NEAR(x, 3 / std::sqrt(2.0), 1e-12); CHECK_NEAR(y, 3 / std::sqrt(2.0), 1e-12);
        setAngleMode(AngleMode::Radians);
        // Derivatives: dY/dX, dX/dY, parametric dy/dx = (-3 sin t)/(3 cos t)
        CHECK(r.engine.derivative(p[0], 1.5, v)); CHECK_NEAR(v, 3, 1e-9);
        CHECK(r.engine.derivative(p[3], 2, v)); CHECK_NEAR(v, 4, 1e-9);
        CHECK(r.engine.derivative(p[2], kPi / 4, v)); CHECK_NEAR(v, -1, 1e-9);
        // References resolved through the engine: kinds and errors
        const char* var = nullptr;
        CHECK(r.engine.function("y1", var) && std::string(var) == "x");
        CHECK(r.engine.function("r2", var) && std::string(var) == "theta");
        CHECK(r.engine.function("yt3", var) && std::string(var) == "t");
        CHECK(r.engine.function("x4", var) && std::string(var) == "y");
        CHECK(r.engine.function("y5", var));                             // Y>= is a Y type
        CHECK(!r.engine.function("y2", var));                            // r2 is polar
        CHECK(!r.engine.function("y7", var));                            // syntax error
        CHECK(!r.engine.function("y9", var));                            // empty
    }
    {
        // Coefficient assignment Y1(A=..), nested composites and the 5-level limit
        Rig r;
        r.ws.memory.store('A', 5);
        r.set(0, "Ax^2-1");
        r.set(1, "@Y1(A=-1)");
        r.set(2, "@Y1(A=2)+@Y1");
        r.set(3, "@Y4");                                                 // self reference
        r.set(4, "@Y1(@Y1(@Y1(@Y1(@Y1(x)))))");                          // 5 nested references
        r.set(5, "@Y1(@Y1(@Y1(@Y1(@Y1(@Y1(x))))))");                     // 6
        r.build();
        const auto& p = r.engine.plots();
        CHECK(p.size() == 6);
        double v = 0;
        CHECK(r.engine.eval(p[1], 2, v) && v == -5);                    // -1*4-1
        CHECK(r.engine.eval(p[2], 1, v) && v == 1 + 4);                 // (2-1)+(5-1)
        CHECK(r.ws.memory.recall('A') == 5);                            // memory unchanged
        CHECK(!r.engine.eval(p[3], 1, v));                               // endless recursion -> Stack ERROR
        CHECK(r.engine.eval(p[4], 0, v));
        CHECK(!r.engine.eval(p[5], 0, v));
        CHECK_THROWS_AS(r.engine.evalOrThrow(p[3], 1), MathError);
    }
    {
        // Typed letters Y1 still mean Y*1; references need the GRAPH engine.
        CalculatorMemory mem; mem.store('Y', 3);
        EvalContext ctx; ctx.memory = &mem;
        CHECK(evaluateReal(*parseExpression("Y1"), ctx) == 3);
        CHECK_THROWS_AS(evaluateReal(*parseExpression("@Y1(2)"), ctx), MathError);
        CHECK_THROWS_AS(parseExpression("@Y21"), SyntaxError);
        CHECK_THROWS_AS(parseExpression("@Z1"), SyntaxError);
        CHECK_THROWS_AS(parseExpression("@Y0"), SyntaxError);
        CHECK(isFunctionReferenceName("xt20") && isFunctionReferenceName("r1") && !isFunctionReferenceName("yt21") && !isFunctionReferenceName("y01"));
        exact::Value ev;
        CHECK(!evaluateExact(*parseExpression("@Y1"), ctx, ev));        // never exact
        // Xdot is a shared V-Window variable; list names exist only in GRAPH.
        Workspace ws;
        EvalContext wc = ws.evalContext();
        CHECK_NEAR(evaluateReal(*parseExpression("Xdot"), wc), 12.6 / 319, 1e-15);
        CHECK_THROWS_AS(evaluateReal(*parseExpression("List1"), wc), MathError);
    }
    {
        // Graph range, overwrite and List graphs
        Rig r;
        r.set(0, "x^2+3x-2,[-2,4]");
        r.set(1, "Ax^2-3,[A=3,1,-1]");
        r.ws.functions[1].color = static_cast<std::uint8_t>(GraphColor::Red);
        r.set(2, "List1x^2-List2");
        r.ws.lists[0] = {1, 2, 3}; r.ws.lists[1] = {4, 5, 6};
        r.set(3, "x,[3,1]");                                             // start >= end
        r.set(4, "List1x+List3");                                        // lists of different length
        r.ws.lists[2] = {1};
        r.build();
        const auto& p = r.engine.plots();
        CHECK(p.size() == 1 + 3 + 3);
        double v = 0;
        CHECK(p[0].hasRange && p[0].lo == -2 && p[0].hi == 4);
        CHECK(!r.engine.eval(p[0], -2.5, v) && r.engine.eval(p[0], 4, v) && v == 26);
        CHECK(p[1].variants == 3 && p[2].variant == 1 && p[3].variant == 2);
        CHECK(r.engine.eval(p[1], 1, v) && v == 0);                     // A=3
        CHECK(r.engine.eval(p[3], 1, v) && v == -4);                    // A=-1
        CHECK(p[1].color == static_cast<std::uint8_t>(GraphColor::Red)); // first: the line colour
        CHECK(p[2].color == static_cast<std::uint8_t>(GraphColor::Green) && p[3].color == static_cast<std::uint8_t>(GraphColor::Magenta));
        CHECK_EQ_STR(p[2].detail, "A=1");
        CHECK(r.engine.eval(p[4], 2, v) && v == 4 - 4);                 // 1*x^2-4
        CHECK(r.engine.eval(p[6], 2, v) && v == 12 - 6);                // 3*x^2-6
        CHECK(!r.engine.error(3).empty() && !r.engine.error(4).empty());
        // Pasted graphs: "Y=expr" and "expr", blue normal (manual 5-31)
        r.build({"Y=2x", "x+1", "1+"});
        CHECK(r.engine.plots().size() == 7 + 2);
        CHECK(r.engine.plots()[7].pasted && r.engine.plots()[7].color == 0);
        CHECK(r.engine.eval(r.engine.plots()[7], 3, v) && v == 6);
        // At most 30 plots in total
        for (std::size_t i = 0; i < MaxGraphFunctions; ++i) r.set(i, "x+" + std::to_string(i) + ",[A=1,2]");
        r.build({"x"});
        CHECK(r.engine.plots().size() == MaxPlots);
    }
    {
        // Modify: coefficients of an expression
        const auto body = parseExpression("Ax^2+Bx+C+sin(Dx)+e+y");
        CHECK_EQ_STR(GraphEngine::coefficients(*body, "x"), "ABCD");
        CHECK_EQ_STR(GraphEngine::coefficients(*parseExpression("x^2"), "x"), "");
        CHECK_EQ_STR(GraphEngine::coefficients(*parseExpression("Ksin(theta)+Ct"), "theta"), "CK");
    }
    // ---------------------------------------------------------------- sampling
    {
        Rig r;
        r.set(0, "x^2");
        r.set(1, "1/x");
        r.set(2, "tan(x)");
        r.set(3, "int(x)");
        r.set(4, "sqrt(x)");
        r.set(5, "1000x");
        r.set(6, "x", GraphType::YGreater);
        r.set(7, "sqrt(1-x^2)");
        setAngleMode(AngleMode::Radians);
        r.build();
        const auto& p = r.engine.plots();
        const Viewport vp;
        const Curve parabola = sampleAll(r.engine, p[0], vp);
        CHECK(parabola.done && !parabola.truncated && pieces(parabola) >= 1);
        CHECK(allInside(parabola, vp));
        // Incremental sampling gives exactly the same points as one call.
        const Curve parts = sampleAll(r.engine, p[0], vp, false, 7);
        CHECK(parts.xy == parabola.xy);
        // 1/x: two branches, never joined across x = 0 (column 159.5)
        const Curve hyper = sampleAll(r.engine, p[1], vp);
        CHECK(pieces(hyper) == 2);
        CHECK(!crossesColumn(hyper, 159, 20) || !crossesColumn(hyper, 160, 20));
        // tan(x): no vertical lines at the poles +-pi/2, +-3pi/2
        const Curve tangent = sampleAll(r.engine, p[2], vp);
        CHECK(pieces(tangent) >= 5);
        for (double pole : {-3 * kPi / 2, -kPi / 2, kPi / 2, 3 * kPi / 2})
            CHECK(!crossesColumn(tangent, static_cast<int>(std::lround(vp.screenX(pole))), 140));
        // int(x): jumps of one unit (45 px) are not joined; values -3..3 are visible
        const Curve steps = sampleAll(r.engine, p[3], vp);
        CHECK(pieces(steps) == 7);
        CHECK(!crossesColumn(steps, static_cast<int>(std::lround(vp.screenX(1))), 30));
        // sqrt(x): starts at the edge of the domain (close to the origin)
        const Curve root = sampleAll(r.engine, p[4], vp);
        CHECK(pieces(root) == 1);
        CHECK(std::abs(root.xy[0] - vp.screenX(0)) <= 1.0);
        CHECK(std::abs(root.xy[1] - vp.screenY(0)) <= 1.0);
        // 1000x is steep but continuous: one piece through the origin
        const Curve steep = sampleAll(r.engine, p[5], vp);
        CHECK(pieces(steep) == 1);
        // Inequality boundary per column
        const Curve ineq = sampleAll(r.engine, p[6], vp);
        CHECK(ineq.boundary.size() == static_cast<std::size_t>(graphgeo::Columns));
        CHECK_NEAR(ineq.boundary[0], vp.screenY(-6.3), 1e-3);
        // Plot draw type: isolated dots only
        const Curve dots = sampleAll(r.engine, p[0], vp, true);
        CHECK(pieces(dots) > 50);
        for (std::size_t i = 0; i + 3 < dots.xy.size(); i += 2) CHECK(dots.xy[i] == PenUp || dots.xy[i + 2] == PenUp);
        // Half circle ends on the x axis at both sides
        const Curve half = sampleAll(r.engine, p[7], vp);
        CHECK(pieces(half) == 1 && allInside(half, vp));
        // The budget is respected per call
        Curve partial; int budget = 50;
        CHECK(!sampleStep(r.engine, p[0], vp, false, partial, budget));
        CHECK(budget <= 0 && partial.next > 0 && partial.next < partial.steps);
    }
    {
        // Polar and parametric graphs: steps over T, closed circle
        Rig r;
        r.set(0, "2", GraphType::Polar);
        r.set(1, "2cos(t)", GraphType::Param, "2sin(t)");
        r.set(2, "1/(theta-1)", GraphType::Polar);                      // pole at theta = 1
        setAngleMode(AngleMode::Radians);
        r.build();
        const auto& p = r.engine.plots();
        ViewWindow w; w.xmin = -4; w.xmax = 4; w.ymin = -4; w.ymax = 4;
        const Viewport vp(w);
        CHECK(sampleCount(p[0], vp) == 121);                            // 0..2pi step pi/60
        CHECK_NEAR(sampleParameter(p[0], vp, 120), w.tmax, 1e-12);
        const Curve circle = sampleAll(r.engine, p[0], vp);
        CHECK(pieces(circle) == 1);
        CHECK(std::abs(circle.xy[2] - circle.xy[circle.xy.size() - 2]) <= 1);   // closed
        const Curve param = sampleAll(r.engine, p[1], vp);
        CHECK(param.xy == circle.xy);
        const Curve pole = sampleAll(r.engine, p[2], vp);
        CHECK(pieces(pole) >= 2);                                        // broken at the pole
        // Negative pitch when Tmax < Tmin (manual 5-6)
        ViewWindow back = w; back.tmin = 6.283185307179586; back.tmax = 0; back.tstep = -0.05235987755982988;
        CHECK(validateView(back).empty());
        CHECK(sampleCount(p[0], Viewport(back)) == 121);
        ViewWindow wrong = back; wrong.tstep = 0.1;
        CHECK(!validateView(wrong).empty());
        // A tiny pitch is capped at MaxParamSteps
        ViewWindow fine = w; fine.tstep = 1e-7;
        CHECK(sampleCount(p[0], Viewport(fine)) == MaxParamSteps + 1);
    }
    // ---------------------------------------------------------------- V-Window / ZOOM
    {
        const ViewWindow init = initialWindow(AngleMode::Radians);
        CHECK(init.xmin == -6.3 && init.xmax == 6.3 && init.ymin == -3.1 && init.ymax == 3.1 && init.tmin == 0);
        CHECK(initialWindow(AngleMode::Degrees).tmax == 360);
        const ViewWindow trig = trigWindow(init, AngleMode::Degrees);
        CHECK(trig.xmin == -540 && trig.xmax == 540 && trig.xscl == 90 && trig.ymin == -1.6 && trig.yscl == 0.5);
        CHECK_NEAR(trigWindow(init, AngleMode::Radians).xmax, 3 * kPi, 1e-12);
        const ViewWindow std10 = standardWindow(init);
        CHECK(std10.xmin == -10 && std10.ymax == 10 && std10.tmax == init.tmax);
        const ViewWindow dot = withXdot(init, 0.1);
        CHECK_NEAR(dot.xmax, -6.3 + 31.9, 1e-12);
        CHECK_NEAR(viewXdot(dot), 0.1, 1e-15);
        CHECK_THROWS_AS(withXdot(init, 0), MathError);
        const ViewWindow in = zoomAt(init, 1, 1, 2, 2);
        CHECK_NEAR(in.xmin, 1 - 3.15, 1e-12); CHECK_NEAR(in.xmax, 1 + 3.15, 1e-12); CHECK_NEAR(in.ymax, 1 + 1.55, 1e-12);
        const ViewWindow out = zoomAt(init, 0, 0, 0.5, 0.25);
        CHECK_NEAR(out.xmax, 12.6, 1e-12); CHECK_NEAR(out.ymax, 12.4, 1e-12);
        const ViewWindow box = zoomBox(init, 2, -1, -1, 3);
        CHECK(box.xmin == -1 && box.xmax == 2 && box.ymin == -1 && box.ymax == 3);
        CHECK_THROWS_AS(zoomBox(init, 1, 1, 1, 2), MathError);           // same x: no width
        const ViewWindow sq = squareWindow(init);
        CHECK_NEAR(viewXdot(sq), viewYdot(sq), 1e-15);
        CHECK_NEAR(sq.xmin + sq.xmax, 0, 1e-12);
        const ViewWindow xb = squareXBase(init);
        CHECK_NEAR(viewYdot(xb), viewXdot(xb), 1e-15);
        CHECK(xb.xmin == init.xmin);
        const ViewWindow rnd = roundWindow(init, 0.3, 0.2);
        CHECK_NEAR(viewXdot(rnd), 0.04, 1e-15);                          // 0.0395 -> 0.04
        CHECK_NEAR(viewYdot(rnd), 0.02, 1e-15);                          // 0.0221 -> 0.02
        CHECK_NEAR(rnd.xmin / 0.04, std::round(rnd.xmin / 0.04), 1e-9);
        const ViewWindow integer = integerWindow(init, 10.2, -3.7);
        CHECK(viewXdot(integer) == 1 && viewYdot(integer) == 1);
        CHECK(integer.xmin == std::round(integer.xmin) && integer.ymin == std::round(integer.ymin));
        CHECK(integer.xmin <= 10.2 && integer.xmax >= 10.2);
        ViewWindow au = init;
        CHECK(autoWindow(au, {-1, 0, 2, std::numeric_limits<double>::quiet_NaN()}));
        CHECK_NEAR(au.ymin, -1.15, 1e-12); CHECK_NEAR(au.ymax, 2.15, 1e-12);
        std::vector<double> poles(100, 1.0); poles[3] = 1e9; poles[4] = -1e9; poles[50] = 2;
        CHECK(autoWindow(au, poles) && au.ymax < 10);                   // pole values ignored
        CHECK(!autoWindow(au, {}));
        double first = 0; int count = 0;
        CHECK(tickRange(-6.3, 6.3, 1, 100, first, count) && first == -6 && count == 13);
        CHECK(!tickRange(-6.3, 6.3, 0, 100, first, count));
        CHECK(!tickRange(-1e6, 1e6, 1, 100, first, count));
        CHECK(!validateView(init).empty() == false);
        ViewWindow bad = init; bad.xmax = bad.xmin;
        CHECK(!validateView(bad).empty());
        bad = init; bad.ymin = 1e99;
        CHECK(!validateView(bad).empty());
        bad = init; bad.tstep = 0;
        CHECK(!validateView(bad).empty());
    }
    return testutil::finish("graph_engine_test");
}
