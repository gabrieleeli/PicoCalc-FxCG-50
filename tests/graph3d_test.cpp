// 3D Graph (v1.9.0): surface engine (sampling, z-buffer and painter's
// renderers, several surfaces, z clipping) and the 3D GRAPH mode (list,
// editor, V-Window, incremental sampling, rotation/zoom, AUTO, persistence,
// memory profiles with and without the PSRAM pool).
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
bool paraboloid(double x, double y, double& z) { z = x * x + y * y; return true; }
bool plane(double, double, double& z) { z = 2.0; return true; }
bool holed(double x, double y, double& z) { const double d = 1 - x * x - y * y; if (d < 0) return false; z = std::sqrt(d); return true; }
} // namespace

int main() {
    // ================================================================ engine
    {
        g3d::Mesh m;
        const g3d::Window w{-2, 2, -2, 2, 0, 8};
        g3d::sample(m, paraboloid, w, 16);
        CHECK(m.grid == 16 && m.verts.size() == 17 * 17 && m.valid == 17 * 17);
        CHECK(m.zmin == 0 && m.zmax == 8);
        // incremental sampling gives the same mesh
        g3d::Mesh inc;
        g3d::beginSample(inc, 16);
        std::size_t next = 0;
        int calls = 0;
        while (!g3d::sampleStep(inc, paraboloid, w, next, 37)) ++calls;
        CHECK(calls > 5 && inc.valid == m.valid && inc.zmax == m.zmax);
        bool same = true;
        for (std::size_t i = 0; i < m.verts.size(); ++i) same = same && m.verts[i].z == inc.verts[i].z && m.verts[i].valid == inc.verts[i].valid;
        CHECK(same);
        // undefined points become holes
        g3d::Mesh h;
        g3d::sample(h, holed, {-1.5, 1.5, -1.5, 1.5, 0, 1}, 20);
        CHECK(h.valid > 0 && h.valid < h.verts.size());

        gfx::Canvas c(fb, 320, 320);
        const gfx::Rect vp{10, 30, 300, 240};
        const g3d::Mesh* one[] = {&m};
        for (bool zbuffer : {true, false}) {
            c.clear(0xFFFF);
            const auto st = g3d::render(c, one, 1, w, g3d::View{}, vp, zbuffer);
            CHECK(st.ok && st.zbuffer == zbuffer && st.triangles == 2u * 16u * 16u && st.bufferBytes > 0);
            CHECK(countNot(0xFFFF, vp) > 5000);                              // the surface fills a good part of the box
            CHECK(countNot(0xFFFF, {0, 0, 320, 29}) == 0);                   // nothing outside the viewport
            CHECK(countNot(0xFFFF, {0, 271, 320, 49}) == 0);
        }
        // z clipping: the 3D V-Window cuts the paraboloid at z = 2
        c.clear(0xFFFF);
        const auto clipped = g3d::render(c, one, 1, {-2, 2, -2, 2, 0, 2}, g3d::View{}, vp, true);
        CHECK(clipped.ok && clipped.triangles < 2u * 16u * 16u / 2);
        // two surfaces share the depth buffer: seen from above, the plane z=2
        // hides the paraboloid where it is below 2 and is hidden where it is above
        g3d::Mesh p;
        g3d::sample(p, plane, w, 16);
        const g3d::Mesh* two[] = {&m, &p};
        c.clear(0xFFFF);
        const g3d::View top{0.0F, 85.0F, 1.0F};
        const auto both = g3d::render(c, two, 2, w, top, vp, true);
        CHECK(both.ok && both.triangles == 4u * 16u * 16u);
        const std::uint16_t centre = fb[(vp.y + vp.h * 52 / 100) * 320 + vp.x + vp.w / 2];
        CHECK(centre != 0xFFFF);
        // the centre pixel shows the plane colour family (surface 2), not the paraboloid
        c.clear(0xFFFF);
        const g3d::Mesh* onlyPlane[] = {nullptr, &p};
        (void)g3d::render(c, onlyPlane, 2, w, top, vp, true);
        CHECK(fb[(vp.y + vp.h * 52 / 100) * 320 + vp.x + vp.w / 2] == centre);
        // rotation keeps the pitch in range and the yaw in [0, 360)
        g3d::View v;
        g3d::rotate(v, 400.0F, 200.0F);
        CHECK(v.yawDeg >= 0.0F && v.yawDeg < 360.0F && v.pitchDeg <= 85.0F);
        g3d::rotate(v, -1000.0F, -500.0F);
        CHECK(v.yawDeg >= 0.0F && v.yawDeg < 360.0F && v.pitchDeg >= -5.0F);
        // distinct colour families for the three surfaces
        // each surface keeps its own dominant channel at every height and light
        // (Z1 blue, Z2 red, Z3 green): intersecting surfaces stay distinguishable
        for (float t : {0.0F, 0.25F, 0.5F, 0.75F, 1.0F})
            for (float light : {0.45F, 1.0F})
                for (std::size_t sfc = 0; sfc < 3; ++sfc) {
                    const std::uint16_t c = g3d::surfaceColor(sfc, t, light);
                    const int r = (c >> 11) * 2, g = (c >> 5) & 63, b = (c & 31) * 2;   // 6-bit scale
                    const int dominant = b > r && b > g ? 0 : r > g && r > b ? 1 : g > r && g > b ? 2 : -1;
                    CHECK(dominant == static_cast<int>(sfc));
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
        CHECK(ws.graph3d.model.z[0] == "sin(x)cos(y)" && ws.graph3d.model.selected[0]);
        // edit Z2 by typing
        r.key(pckey::Down);
        r.enter("x^2-y^2");
        CHECK(ws.graph3d.model.z[1] == "x^2-y^2" && ws.graph3d.model.selected[1]);
        // a syntax error keeps the editor open
        r.key(pckey::Down); r.enter("x+*");
        CHECK(r.error() && r.errorTitle() == "Syntax ERROR");
        r.closeError();
        CHECK(g->screen() == G3::Screen::Edit);
        r.key(pckey::Esc);
        CHECK(g->screen() == G3::Screen::List && ws.graph3d.model.z[2].empty());
        // V-Window: invalid range refused, valid one stored
        r.key(pckey::F3);
        CHECK(g->screen() == G3::Screen::Window);
        r.enter("5");                                                      // Xmin = 5 > Xmax = 3
        r.key(pckey::F6);
        CHECK(r.error() && r.errorTitle() == "Argument ERROR");
        r.closeError();
        r.key(pckey::Up); r.enter("-2");                                   // Xmin = -2
        r.key(pckey::Down); r.key(pckey::Down); r.key(pckey::Down); r.key(pckey::Down);
        r.key(pckey::Down);                                                // Griglia
        r.enter("128");
        r.key(pckey::Esc);                                                 // back to the list, stored
        CHECK(!r.error() && g->screen() == G3::Screen::List && ws.graph3d.model.xmin == -2 && ws.graph3d.model.grid == 128);
        // DRAW: sampling runs in ticks
        r.key(pckey::F6);
        CHECK(g->screen() == G3::Screen::Plot);
        r.settle();
        CHECK(!g->sampling() && !r.error());
        // memory profile: z-buffer and the full grid only with the PSRAM pool
        CHECK(g->zbuffer() == psram);
        if (psram) CHECK(g->grid() == 128);
        else CHECK(g->grid() <= limits::kSramProfile.graph3dMaxGrid);
        CHECK(g->mesh(0).valid == (g->grid() + 1) * (g->grid() + 1) && g->mesh(1).valid == g->mesh(0).valid);
        CHECK(g->mesh(2).verts.empty());                                    // Z3 empty: not sampled
        CHECK(g->lastRender().ok && g->lastRender().triangles > 0);
        {
            std::size_t drawn = 0;
            const gfx::Rect vp = G3::Viewport;
            for (int y = vp.y; y < vp.y + vp.h; ++y)
                for (int x = vp.x; x < vp.x + vp.w; ++x) drawn += board::graphics().getPixel(x, y) != 0xFFFF;
            CHECK(drawn > 3000);                                           // the surfaces are on the screen
        }
        // rotation, zoom, AUTO, view reset
        const float yaw = ws.graph3d.yaw;
        r.key(pckey::Right);
        CHECK(std::fabs(ws.graph3d.yaw - std::fmod(yaw + 15.0F, 360.0F)) < 1e-3F);
        r.key(pckey::F1);
        CHECK(ws.graph3d.zoom > 1.2F);
        r.key(pckey::F4);                                                  // AUTO: z range of x^2-y^2 on [-2,3]x[-3,3]
        CHECK(ws.graph3d.model.zmin < -8.9 && ws.graph3d.model.zmax > 8.9);
        r.key(pckey::F5);
        CHECK(ws.graph3d.yaw == 35.0F && ws.graph3d.zoom == 1.0F);
        CHECK(g->mesh(0).valid > 0);                                       // rotation and zoom kept the samples
        // F6 back to the list; F1 deselects Z1
        r.key(pckey::F6);
        CHECK(g->screen() == G3::Screen::List);
        r.key(pckey::Up); r.key(pckey::Up);
        r.key(pckey::F1);
        CHECK(!ws.graph3d.model.selected[0]);
        // nothing selected: Argument ERROR
        r.key(pckey::Down); r.key(pckey::F1);
        r.key(pckey::F6);
        CHECK(r.error() && r.errorTitle() == "Argument ERROR");
        r.closeError();
        // leaving the mode releases the meshes
        r.key(pckey::F1);                                                  // Z2 on again
        r.key(pckey::F6); r.settle();
        CHECK(g->mesh(1).valid > 0);
        r.home();
        CHECK(g->mesh(1).verts.empty());
        // persistence: the 3D state is part of the record (version 5)
        const auto blob = serializeWorkspace(ws);
        Workspace back;
        CHECK(deserializeWorkspace(blob.data(), blob.size(), back));
        CHECK(back.graph3d.model.z[1] == "x^2-y^2" && back.graph3d.model.grid == 128 && back.graph3d.model.xmin == -2);
        CHECK(back.graph3d.model.selected[1] && !back.graph3d.model.selected[0]);
        CHECK(std::fabs(back.graph3d.model.zmax - ws.graph3d.model.zmax) < 1e-12);
        ws.graph3d = Graph3DState{};
        mem::testing::reset();
    }
    return testutil::finish("graph3d_test");
}
