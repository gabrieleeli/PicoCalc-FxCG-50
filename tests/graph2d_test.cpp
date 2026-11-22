// v1.15.0 (F7, solo Pimoroni Pico Plus 2 W): grafico 2D condiviso con schermo
// diviso: disposizioni G+G/GtoT/T+G, sorgenti di punti generiche,
// campionamento a passi, TRACE e ZOOM comuni, disegno incrementale identico al
// ridisegno completo (pixel per pixel).
#include "graph2d.hpp"
#include "test_util.hpp"
#include "workspace.hpp"
#include <cmath>
#include <vector>

using namespace fx;
using namespace fx::graph2d;

namespace {
struct Screen {
    std::vector<std::uint16_t> px = std::vector<std::uint16_t>(320 * 320, 0xFFFF);
    gfx::Canvas c{px.data(), 320, 320};
};
} // namespace

int main() {
    ViewWindow v;                                               // -6.3..6.3, -3.1..3.1 (predefinita)
    Workspace ws;
    EvalContext ctx = ws.evalContext();
    // ---------------------------------------------------------------- F7.1 disposizioni
    {
        const Panes full = layout(Layout::Full, v, v);
        CHECK(!full.split && full.main.columns() == graphgeo::Columns && full.main.left() == 0);
        const Panes gg = layout(Layout::GraphGraph, v, v);
        CHECK(gg.split && !gg.subIsTable && gg.mainRect.x == 0 && gg.mainRect.w == 159 && gg.subRect.x == 160 && gg.subRect.w == 160);
        CHECK(gg.main.columns() == 159 && gg.sub.left() == 160 && gg.main.rows() == graphgeo::Rows);
        const Panes gt = layout(Layout::GraphTable, v, v);
        CHECK(gt.subIsTable && !gt.tableLeft && gt.mainRect.x == 0);
        const Panes tg = layout(Layout::TableGraph, v, v);
        CHECK(tg.subIsTable && tg.tableLeft && tg.mainRect.x == 160 && tg.subRect.x == 0);
        // la stessa V-Window: un punto finisce nella stessa x relativa dei due riquadri
        CHECK(std::abs((gg.main.screenX(1) - gg.main.left()) - (gg.sub.screenX(1) - gg.sub.left())) < 1.0);
    }
    // ---------------------------------------------------------------- F7.2 sorgenti e campionamento a passi
    {
        FunctionSource f("x^2-1", ctx);
        double x = 0, y = 0;
        CHECK(f.kind() == PointSource::Kind::Function && f.eval(2, x, y) && x == 2 && y == 3);
        FunctionSource g("1/x", ctx);
        CHECK(!g.eval(0, x, y));                                 // non definita: buco
        ParametricSource circle([](double t, double& px, double& py) { px = 2 * std::cos(t); py = 2 * std::sin(t); return true; }, 0, 6.283185307179586, 0.05);
        CHECK(circle.eval(0, x, y) && std::abs(x - 2) < 1e-12 && std::abs(y) < 1e-12);
        PointsSource seq({1, 2, 3}, {1, 4, 9}, false);
        CHECK(seq.count() == 3 && seq.eval(2, x, y) && y == 9 && !seq.eval(3, x, y) && !seq.eval(-1, x, y));
        const graph::Viewport vp(v);
        Plotter p(f, vp);
        CHECK(p.total() == static_cast<std::size_t>(vp.columns()) + 1);
        step::Runner r;
        std::uint32_t now = 0;
        r.start(p, step::Runner::Config{50, 0});
        int steps = 0;
        while (r.running()) { r.tick([&] { return now; }); ++steps; }
        CHECK(r.state() == step::State::Done && steps == 7 && p.finished() && !p.points().empty());
        // 1/x: la linea si spezza all'asintoto
        Plotter q(g, vp);
        q.runAll();
        int penUps = 0;
        for (std::size_t i = 0; i + 1 < q.points().size(); i += 2) penUps += q.points()[i] == graph::PenUp;
        CHECK(penUps >= 2);
        // punti isolati di una successione
        Plotter s(seq, vp);
        s.runAll();
        CHECK(s.points().size() == 3 * 4);                       // PenUp + punto per ciascuno
        Plotter c(circle, vp);
        c.runAll();
        CHECK(c.total() == 126 && c.points().size() > 200);
    }
    // ---------------------------------------------------------------- F7.4 incrementale == completo
    {
        FunctionSource f("sin(x)*3", ctx);
        const Panes gg = layout(Layout::GraphGraph, v, v);
        Screen whole, pieces;
        drawAxes(whole.c, gg.main, 0x0000, 0xC618, true);
        drawAxes(pieces.c, gg.main, 0x0000, 0xC618, true);
        Plotter a(f, gg.main);
        a.runAll();
        a.draw(whole.c, 0x001F);
        Plotter b(f, gg.main);
        std::size_t drawn = 0;
        step::Budget budget;
        while (!b.finished()) {
            step::Budget small(17, 0, nullptr);
            b.step(small);
            drawn = b.draw(pieces.c, 0x001F, drawn);
        }
        (void)budget;
        CHECK(whole.px == pieces.px);
        // niente fuori dal riquadro principale
        bool outside = false;
        for (int y = 0; y < 320; ++y)
            for (int x = gg.mainRect.x + gg.mainRect.w; x < 320; ++x) outside = outside || whole.px[static_cast<std::size_t>(y) * 320 + static_cast<std::size_t>(x)] != 0xFFFF;
        CHECK(!outside);
        // e qualcosa e' stato disegnato
        std::size_t ink = 0;
        for (auto p : whole.px) ink += p == 0x001F;
        CHECK(ink > 100);
    }
    // ---------------------------------------------------------------- F7.3 TRACE e ZOOM
    {
        const graph::Viewport vp(v);
        FunctionSource f("2x", ctx);
        TraceCursor t(f, vp);
        double x = 0, y = 0;
        CHECK(t.point(x, y) && std::abs(y - 2 * x) < 1e-12);
        const double x0 = t.parameter();
        t.right();
        CHECK(std::abs(t.parameter() - (x0 + vp.xdot())) < 1e-12);
        for (int i = 0; i < 1000; ++i) t.left();
        CHECK(t.parameter() == v.xmin);
        PointsSource pts({1, 2, 3}, {5, 6, 7}, true);
        TraceCursor tp(pts, vp);
        tp.right(); tp.right(); tp.right(); tp.right();
        CHECK(tp.parameter() == 2 && tp.point(x, y) && y == 7);
        const ViewWindow zi = zoomIn(v, 0, 0, 2, 2);
        CHECK(std::abs((zi.xmax - zi.xmin) - (v.xmax - v.xmin) / 2) < 1e-9);
        const ViewWindow zo = zoomOut(v, 1, 1, 2, 2);
        CHECK(std::abs((zo.xmax - zo.xmin) - (v.xmax - v.xmin) * 2) < 1e-9 && std::abs((zo.xmin + zo.xmax) / 2 - 1) < 1e-9);
        const ViewWindow zb = zoomBox(v, -1, -2, 3, 4);
        CHECK(zb.xmin == -1 && zb.xmax == 3 && zb.ymin == -2 && zb.ymax == 4);
        CHECK_THROWS(zoomIn(v, 0, 0, 0, 2));
    }
    return testutil::finish("graph2d_test");
}
