// v1.14.0 — Fuzzing del 3D Graph: insiemi delle 3D G-Mem (g3d::decode, andata e
// ritorno, geometria dei modelli, INTSECT/RELATION, sezioni, reticoli e disegno
// con z-buffer e pittore), espressioni dei grafici Z= e Param (parser unico,
// valutazione con x/y e s/t legate, campionamento, disegno) e V-Window (decodifica,
// proiezione, taglio delle rette). Primo byte: 0 insieme, 1 espressione, 2 finestra.
#include "expression.hpp"
#include "graph3d.hpp"
#include "graph3d_model.hpp"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
using namespace fx;
namespace {
std::uint16_t fb[96 * 96];
void check(bool ok, const char* what) { if (!ok) { std::fprintf(stderr, "FAIL %s\n", what); std::abort(); } }
bool finite(g3d::Vec3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
void draw(const g3d::Item* items, std::size_t n, const g3d::Window& w, const g3d::Setup& s) {
    gfx::Canvas c(fb, 96, 96);
    const gfx::Rect vp{0, 0, 96, 96};
    const g3d::Camera cam{w.theta, w.phi, 1.0F};
    for (bool zb : {true, false}) {
        c.clear(0xFFFF);
        const auto st = g3d::render(c, items, n, w, s, cam, vp, zb);
        check(st.ok, "render");
    }
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* d, size_t n) {
    if (n < 1) return 0;
    const std::string body(reinterpret_cast<const char*>(d + 1), n - 1);
    switch (d[0] % 3) {
    case 0: {
        g3d::GraphSet set;
        if (!g3d::decode(body, set)) return 0;
        const std::string again = g3d::encode(set);
        g3d::GraphSet b2;
        check(g3d::decode(again, b2) && g3d::encode(b2) == again, "set round trip");
        const g3d::Window& w = set.window;
        g3d::Shape shapes[g3d::Functions];
        bool ok[g3d::Functions] = {};
        for (std::size_t i = 0; i < g3d::Functions; ++i)
            if (g3d::isTemplate(set.fns[i].kind)) { ok[i] = g3d::shapeOf(set.fns[i], shapes[i]).empty(); check(ok[i], "decoded template valid"); }
        for (std::size_t a = 0; a < g3d::Functions; ++a)
            for (std::size_t b = a + 1; b < g3d::Functions; ++b) {
                if (!ok[a] || !ok[b] || !g3d::isLineOrPlane(set.fns[a]) || !g3d::isLineOrPlane(set.fns[b])) continue;
                const g3d::Intersection r = g3d::intersect(shapes[a], shapes[b]);
                if (r.kind == g3d::Intersection::Kind::Point || r.kind == g3d::Intersection::Kind::Line) check(finite(r.point) || std::fabs(shapes[a].p.x) > 1e150, "intersection");
                (void)g3d::relationName(g3d::relation(shapes[a], shapes[b]));
            }
        g3d::Mesh meshes[g3d::Functions][g3d::MaxPatches];
        g3d::Item items[g3d::Functions];
        std::size_t count = 0;
        g3d::Polyline curves[g3d::MaxSectionCurves];
        for (std::size_t i = 0; i < g3d::Functions; ++i) {
            if (!ok[i]) continue;
            for (int axis = 0; axis < 3; ++axis) {
                bool coincident = false;
                const double lo = axis == 0 ? w.xmin : axis == 1 ? w.ymin : w.zmin, hi = axis == 0 ? w.xmax : axis == 1 ? w.ymax : w.zmax;
                (void)g3d::crossSection(shapes[i], w, static_cast<g3d::Axis>(axis), (lo + hi) / 2, curves, coincident);
            }
            g3d::Item& it = items[count];
            g3d::itemColors(set.fns[i], it);
            if (shapes[i].kind == g3d::Kind::Line) {
                double t0, t1;
                if (!g3d::clipLine(w, shapes[i].p, shapes[i].d, t0, t1)) continue;
                it.line = true; it.a = shapes[i].p + shapes[i].d * t0; it.b = shapes[i].p + shapes[i].d * t1;
                ++count;
                continue;
            }
            g3d::PatchSpec spec[g3d::MaxPatches];
            const std::size_t np = g3d::templatePatches(shapes[i], w, 6, 6, spec);
            for (std::size_t p = 0; p < np; ++p) {
                const g3d::Shape& s = shapes[i];
                g3d::sample(meshes[i][p], [&](double u, double v, g3d::Vec3& o) { return g3d::templatePoint(s, p, u, v, o); }, spec[p].domain, spec[p].nu, spec[p].nv);
                it.patches[p] = &meshes[i][p];
            }
            it.patchCount = np;
            ++count;
        }
        draw(items, count, w, set.setup);
        return 0;
    }
    case 1: {
        // An expression of Z= (x, y) and of Param (s, t), never touching A-Z.
        CalculatorMemory memory;
        memory.store('X', 7);
        g3d::Mesh mesh;
        try {
            const CompiledExpression fz(body, "x");
            const CompiledExpression fs(body, "s");
            const g3d::PointFn z = [&](double u, double v, g3d::Vec3& o) {
                try { o = {u, v, fz.eval2(u, "y", v, &memory)}; return true; } catch (const std::bad_alloc&) { throw; } catch (...) { return false; }
            };
            const g3d::PointFn par = [&](double u, double v, g3d::Vec3& o) {
                try { const double r = fs.eval2(u, "t", v, &memory); o = {r, u, v}; return true; } catch (const std::bad_alloc&) { throw; } catch (...) { return false; }
            };
            g3d::Window w;
            g3d::Item it;
            it.patches[0] = &mesh; it.patchCount = 1; it.hasFill = it.hasWire = true; it.fill = 0x001F; it.wire = 0;
            for (const g3d::PointFn* f : {&z, &par}) {
                g3d::beginSample(mesh, 5, 5);
                std::size_t next = 0;
                while (!g3d::sampleStep(mesh, *f, {-3, 3, -3, 3}, next, 7)) {}
                draw(&it, 1, w, g3d::Setup{});
            }
        } catch (const std::bad_alloc&) {
            throw;
        } catch (...) {
            return 0;                                                  // Syntax ERROR and the like
        }
        check(memory.recall('X') == 7, "A-Z untouched");
        return 0;
    }
    default: {
        g3d::Window w;
        if (!g3d::decodeWindow(body, w)) return 0;
        g3d::Window b;
        check(g3d::decodeWindow(g3d::encodeWindow(w), b) && b == w, "window round trip");
        check(g3d::validate(w).empty(), "decoded window valid");
        double t0, t1;
        if (g3d::clipLine(w, {w.xmin, w.ymax, w.zmin}, {w.xmax - w.xmin, w.ymin - w.ymax, w.zmax - w.zmin}, t0, t1)) check(t0 <= t1, "clip");
        const g3d::Projector p(w, g3d::Camera{w.theta, w.phi, 1.0F}, {0, 0, 96, 96});
        for (int k = 0; k < 8; ++k) {
            const g3d::ScreenPoint s = p.fromNormalized(k & 1 ? 1 : -1, k & 2 ? 1 : -1, k & 4 ? 1 : -1);
            check(std::isfinite(s.x) && std::isfinite(s.y) && std::fabs(s.depth) <= 1.8F, "projection of the box");
        }
        draw(nullptr, 0, w, g3d::Setup{});
        return 0;
    }
    }
}
