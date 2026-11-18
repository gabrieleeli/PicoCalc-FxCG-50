// 3D Graph (v1.9.0, ported to the v1.14.0 API): lattice engine (sampling,
// incremental sampling, holes), z-buffer and painter's renderers, several
// surfaces sharing the depth test, clipping to the box, rotation limits, and
// the 3D GRAPH mode (list, Z= input, V-Window, incremental computation,
// rotation/zoom without recomputation, persistence, memory profiles with and
// without the PSRAM pool). The v1.14.0 features are in graph3d_v114_test.cpp.
#include "app_rig.hpp"
#include "board_config.hpp"
#include "graph3d.hpp"
#include "limits.hpp"
#include "mem_pools.hpp"
#include "storage.hpp"
#include "test_util.hpp"
#include <cmath>
#include <vector>

using namespace fx;
using G3 = app::Graph3DApp;

namespace {
std::uint16_t fb[320 * 320];
std::size_t countNot(std::uint16_t color, gfx::Rect r) {
    std::size_t n = 0;
    for (int y = r.y; y < r.y + r.h; ++y)
        for (int x = r.x; x < r.x + r.w; ++x) n += fb[y * 320 + x] != color;
    return n;
}
bool paraboloid(double x, double y, g3d::Vec3& p) { p = {x, y, x * x + y * y}; return true; }
bool flat(double x, double y, g3d::Vec3& p) { p = {x, y, 2.0}; return true; }
bool holed(double x, double y, g3d::Vec3& p) {
    const double d = 1 - x * x - y * y;
    if (d < 0) return false;
    p = {x, y, std::sqrt(d)};
    return true;
}
g3d::Item item(const g3d::Mesh& m, g3d::Color area) {
    g3d::Item it;
    it.patches[0] = &m; it.patchCount = 1;
    it.hasFill = true; it.fill = g3d::color565(area);
    it.hasWire = false;
    return it;
}
} // namespace

int main() {
    // ================================================================ engine
    {
        g3d::Mesh m;
        const g3d::Window w = [] { g3d::Window x; x.xmin = -2; x.xmax = 2; x.ymin = -2; x.ymax = 2; x.zmin = 0; x.zmax = 8; return x; }();
        const g3d::Domain d{-2, 2, -2, 2};
        g3d::sample(m, paraboloid, d, 17, 17);
        CHECK(m.nu == 17 && m.verts.size() == 17 * 17 && m.valid == 17 * 17);
        CHECK(m.zmin == 0 && m.zmax == 8);
        // incremental sampling gives the same lattice
        g3d::Mesh inc;
        g3d::beginSample(inc, 17, 17);
        std::size_t next = 0;
        int calls = 0;
        while (!g3d::sampleStep(inc, paraboloid, d, next, 37)) ++calls;
        CHECK(calls > 5 && inc.valid == m.valid && inc.zmax == m.zmax);
        bool same = true;
        for (std::size_t i = 0; i < m.verts.size(); ++i) same = same && m.verts[i].z == inc.verts[i].z && m.verts[i].valid == inc.verts[i].valid;
        CHECK(same);
        // undefined points become holes
        g3d::Mesh h;
        g3d::sample(h, holed, {-1.5, 1.5, -1.5, 1.5}, 21, 21);
        CHECK(h.valid > 0 && h.valid < h.verts.size());

        gfx::Canvas c(fb, 320, 320);
        const gfx::Rect vp{10, 30, 300, 240};
        const g3d::Item one[] = {item(m, g3d::Color::Blue)};
        const g3d::Setup noDecor{false, false, false, g3d::Color::Blue};
        for (bool zbuffer : {true, false}) {
            c.clear(0xFFFF);
            const auto st = g3d::render(c, one, 1, w, noDecor, g3d::Camera{}, vp, zbuffer);
            CHECK(st.ok && st.zbuffer == zbuffer && st.triangles >= 2u * 16u * 16u && st.bufferBytes > 0);
            CHECK(countNot(0xFFFF, vp) > 5000);                              // the surface fills a good part of the box
            CHECK(countNot(0xFFFF, {0, 0, 320, 29}) == 0);                   // nothing outside the viewport
            CHECK(countNot(0xFFFF, {0, 271, 320, 49}) == 0);
        }
        // clipping: the box cuts the paraboloid at z = 2 (fewer pixels, exact cut)
        g3d::Window low = w;
        low.zmax = 2;
        c.clear(0xFFFF);
        const auto full = countNot(0xFFFF, vp);
        (void)full;
        c.clear(0xFFFF);
        (void)g3d::render(c, one, 1, w, noDecor, g3d::Camera{}, vp, true);
        const std::size_t whole = countNot(0xFFFF, vp);
        c.clear(0xFFFF);
        const auto clipped = g3d::render(c, one, 1, low, noDecor, g3d::Camera{}, vp, true);
        CHECK(clipped.ok && countNot(0xFFFF, vp) > 1000 && countNot(0xFFFF, vp) != whole);
        // two surfaces share the depth buffer: seen from above, the plane z=2
        // hides the paraboloid where it is below 2
        g3d::Mesh p;
        g3d::sample(p, flat, d, 17, 17);
        const g3d::Item two[] = {item(m, g3d::Color::Blue), item(p, g3d::Color::Red)};
        g3d::Camera top;
        top.theta = -90; top.phi = 0;
        c.clear(0xFFFF);
        const auto both = g3d::render(c, two, 2, w, noDecor, top, vp, true);
        CHECK(both.ok && both.triangles >= 4u * 16u * 16u);
        const std::uint16_t centre = fb[(vp.y + vp.h / 2) * 320 + vp.x + vp.w / 2];
        CHECK(centre != 0xFFFF);
        c.clear(0xFFFF);
        const g3d::Item onlyPlane[] = {item(p, g3d::Color::Red)};
        (void)g3d::render(c, onlyPlane, 1, w, noDecor, top, vp, true);
        CHECK(fb[(vp.y + vp.h / 2) * 320 + vp.x + vp.w / 2] == centre);   // the plane (red) is in front at the centre
        // rotation keeps θ in (-180, 180] and φ in [0, 360)
        g3d::Camera v;
        g3d::rotate(v, 400.0, 500.0);
        CHECK(v.theta > -180.0 && v.theta <= 180.0 && v.phi >= 0.0 && v.phi < 360.0);
        g3d::rotate(v, -1000.0, -500.0);
        CHECK(v.theta > -180.0 && v.theta <= 180.0 && v.phi >= 0.0 && v.phi < 360.0);
        // the three default area colours keep their dominant channel (blue, red, green)
        const g3d::Color def[3] = {g3d::defaultAreaColor(0), g3d::defaultAreaColor(1), g3d::defaultAreaColor(2)};
        for (int sfc = 0; sfc < 3; ++sfc) {
            const std::uint16_t col = g3d::color565(def[sfc]);
            const int r = (col >> 11) * 2, g = (col >> 5) & 63, b = (col & 31) * 2;
            const int dominant = b > r && b > g ? 0 : r > g && r > b ? 1 : g > r && g > b ? 2 : -1;
            CHECK(dominant == sfc);
        }
    }

    // ================================================================ 3D GRAPH mode
    board::init();
    for (const bool psram : {false, true}) {
        mem::testing::simulatePsram(psram ? (8u << 20) : 0u);
        storage::RecordStore(board::storageFlash()).eraseAll();          // fresh workspace for each profile
        testrig::Rig r;
        auto& ws = r.ws();
        r.type("j");                                                       // main menu: 3D Graph
        G3* g = r.active<G3>();
        CHECK(g && g->screen() == G3::Screen::List);
        if (!g) continue;
        CHECK(ws.graph3d.set.fns[0].kind == g3d::Kind::None);              // empty list, as the fx-CG50
        // Z1 by typing on the list (extension), SET with F6
        r.type("sin(x)cos(y)");
        r.key(pckey::Enter);
        r.key(pckey::F6);
        CHECK(ws.graph3d.set.fns[0].kind == g3d::Kind::Z && ws.graph3d.set.fns[0].expr[0] == "sin(x)cos(y)" && ws.graph3d.set.fns[0].selected);
        // Z2 by TYPE > Z=
        r.key(pckey::Down);
        r.key(pckey::F3);
        CHECK(g->screen() == G3::Screen::Select);
        r.key(pckey::F2);
        CHECK(g->screen() == G3::Screen::Input);
        r.type("x^2");
        r.key(pckey::Right);                                               // out of the exponent (Math input)
        r.type("-y^2");
        r.key(pckey::Enter);
        r.key(pckey::F6);
        CHECK(ws.graph3d.set.fns[1].kind == g3d::Kind::Z && ws.graph3d.set.fns[1].expr[0] == "(x)^(2)-(y)^(2)" && ws.graph3d.set.fns[1].selected);
        // a syntax error keeps the editor open
        r.key(pckey::Down);
        r.type("x+*");
        r.key(pckey::Enter);
        CHECK(r.error() && r.errorTitle() == "Syntax ERROR");
        r.closeError();
        CHECK(g->screen() == G3::Screen::Input);
        r.key(pckey::Esc); r.key(pckey::Esc);
        CHECK(g->screen() == G3::Screen::List && ws.graph3d.set.fns[2].kind == g3d::Kind::None);
        // V-Window: an invalid range is refused, a valid one stored
        r.key(pckey::F7); r.key(pckey::F3);                                // SHIFT F3 (V-WIN)
        CHECK(g->screen() == G3::Screen::Window);
        r.enter("5");                                                      // Xmin = 5 > Xmax = 3
        r.key(pckey::Esc);
        CHECK(r.error() && r.errorTitle() == "Invalid Setting");
        r.closeError();
        r.key(pckey::Up); r.enter("-2");                                   // Xmin = -2
        r.key(pckey::Down);
        r.enter("50");                                                     // Xgrid = 50 (manual limit)
        r.key(pckey::Esc);
        CHECK(!r.error() && g->screen() == G3::Screen::List && ws.graph3d.set.window.xmin == -2 && ws.graph3d.set.window.xgrid == 50);
        // DRAW: computed in ticks
        r.key(pckey::F6);
        CHECK(g->screen() == G3::Screen::Plot);
        r.settle();
        CHECK(!g->sampling() && !r.error() && g->drawn());
        // memory profile: z-buffer and the full lattice only with the PSRAM pool
        CHECK(g->zbuffer() == psram);
        if (psram) CHECK(g->points() == 50);
        else CHECK(g->points() <= limits::kSramProfile.graph3dMaxGrid);
        CHECK(g->mesh(0).nu == std::min<std::size_t>(50, g->points()) && g->mesh(0).valid == g->mesh(0).verts.size());
        CHECK(g->mesh(1).valid == g->mesh(0).valid);
        CHECK(g->mesh(2).verts.empty());                                    // row 3 empty: not computed
        CHECK(g->lastRender().ok && g->lastRender().triangles > 0);
        {
            std::size_t drawn = 0;
            const gfx::Rect vp = G3::Viewport;
            for (int y = vp.y; y < vp.y + vp.h; ++y)
                for (int x = vp.x; x < vp.x + vp.w; ++x) drawn += board::graphics().getPixel(x, y) != 0xFFFF;
            CHECK(drawn > 3000);                                           // the surfaces are on the screen
        }
        // rotation and zoom: θ of the V-Window follows, the lattices are kept
        const double theta = ws.graph3d.set.window.theta;
        r.key(pckey::Right);
        CHECK(std::fabs(ws.graph3d.set.window.theta - g3d::normalizeTheta(theta + 15.0)) < 1e-9);
        r.type("+");
        CHECK(ws.graph3d.zoom > 1.4F);
        r.key(pckey::F2); r.key(pckey::F6);                                // ZOOM > ORIGINAL
        CHECK(ws.graph3d.set.window.theta == theta && ws.graph3d.zoom == 1.0F);
        CHECK(g->mesh(0).valid > 0);
        // F6 (G<->T) back to the list; F1 deselects row 1
        r.key(pckey::F6);
        CHECK(g->screen() == G3::Screen::List);
        r.key(pckey::Up); r.key(pckey::Up);
        r.key(pckey::F1);
        CHECK(!ws.graph3d.set.fns[0].selected);
        // nothing selected: the empty box is drawn (no error)
        r.key(pckey::Down); r.key(pckey::F1);
        r.key(pckey::F6); r.settle();
        CHECK(!r.error() && g->screen() == G3::Screen::Plot && g->drawn());
        r.key(pckey::Esc);
        // leaving the mode releases the lattices
        r.key(pckey::F1);                                                  // row 2 on again
        r.key(pckey::F6); r.settle();
        CHECK(g->mesh(1).valid > 0);
        r.home();
        CHECK(g->mesh(1).verts.empty());
        // persistence: the 3D state is part of the record (version 10)
        const auto blob = serializeWorkspace(ws);
        Workspace back;
        CHECK(deserializeWorkspace(blob.data(), blob.size(), back));
        CHECK(back.graph3d.set.fns[1].expr[0] == "(x)^(2)-(y)^(2)" && back.graph3d.set.window.xgrid == 50 && back.graph3d.set.window.xmin == -2);
        CHECK(back.graph3d.set.fns[1].selected && !back.graph3d.set.fns[0].selected);
        ws.graph3d = Graph3DState{};
        mem::testing::reset();
    }
    return testutil::finish("graph3d_test");
}
