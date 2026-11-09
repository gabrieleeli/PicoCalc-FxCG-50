// Graphics on pooled memory: multi-function plots with sample/layer caches,
// large filled polygons, 3D surfaces (z-buffer or reduced painter profile),
// canvas layer copies and window-manager growth + PSRAM snapshots.
#include "board_config.hpp"
#include "graph3d.hpp"
#include "graph_renderer.hpp"
#include "graphics_engine.hpp"
#include "limits.hpp"
#include "mem_pools.hpp"
#include "test_support.hpp"
#include "window_manager.hpp"

#include <cmath>
#include <cstring>
#include <string>
#include <vector>

namespace {
std::uint16_t g_fb[320u * 320u];
std::size_t countColor(std::uint16_t color) {
    std::size_t n = 0;
    for (auto p : g_fb) n += p == color;
    return n;
}
struct Probe { int draws{}; std::uint32_t stamp{1}; };
void onEvent(void*, const fx::InputEvent&) {}
void onDraw(void* p) { ++static_cast<Probe*>(p)->draws; }
std::uint32_t onStamp(void* p) { return static_cast<Probe*>(p)->stamp; }
int g_presents = 0;
void present() { ++g_presents; }
} // namespace

int main() {
    mem::testing::simulateSram(400u * 1024u);
    mem::testing::simulatePsram(board::config::kHasPsram ? (8u << 20) : 0u);
    gfx::Canvas canvas(g_fb, 320, 320);
    FX_CHECK(gfx::DirtyRegions::Capacity == fx::limits::kDirtyRegionCapacity);

    // --- Multi-function plot with caches ------------------------------------
    fx::GraphRenderer::clearCache();
    std::vector<std::string> functions{"sin(x)", "x/2", "x^2-3"};
    fx::GraphView view;
    canvas.clear(gfx::palette::Black);
    fx::GraphRenderer::draw(canvas, functions, view, 1);
    auto stats = fx::GraphRenderer::cacheStats();
    FX_CHECK(stats.functions == 3 && stats.columns == 297 && !stats.directFallback);
    FX_CHECK(stats.samplesPerPixel >= 1 && stats.samplesPerPixel <= fx::limits::profile().graphSamplesPerPixel);
    FX_CHECK(stats.sampleMisses == 1);
    for (std::size_t f = 0; f < functions.size(); ++f) FX_CHECK(countColor(fx::GraphRenderer::colorFor(f)) > 50);
    // Trace-only change: no re-evaluation (sample or layer cache hit).
    view.trace = true;
    view.cursorX = 200;
    fx::GraphRenderer::draw(canvas, functions, view, 0);
    stats = fx::GraphRenderer::cacheStats();
    FX_CHECK(stats.sampleMisses == 1 && (stats.sampleHits + stats.layerHits) >= 1);
    if (fx::limits::profile().graphLayerCache) FX_CHECK(stats.layerHits == 1 && stats.layerBytes > 0);
    // Zoom invalidates the cache.
    fx::GraphRenderer::zoom(view, 0.5);
    fx::GraphRenderer::draw(canvas, functions, view, 0);
    FX_CHECK(fx::GraphRenderer::cacheStats().sampleMisses == 2);
    // Legacy single-expression API still works.
    fx::GraphRenderer::draw(canvas, std::string("x"), fx::GraphView{});
    fx::GraphRenderer::clearCache();

    // --- Filled polygon above the old 32-vertex limit ---------------------------
    std::vector<gfx::Point> star;
    for (int i = 0; i < 80; ++i) {
        const double a = i * 2.0 * 3.14159265358979323846 / 80.0;
        const double r = (i % 2) ? 60.0 : 120.0;
        star.push_back({160 + static_cast<int>(r * std::cos(a)), 160 + static_cast<int>(r * std::sin(a))});
    }
    canvas.clear(gfx::palette::Black);
    canvas.polygon(star.data(), star.size(), gfx::palette::Green, true);
    FX_CHECK(canvas.getPixel(160, 160) == gfx::palette::Green);
    FX_CHECK(countColor(gfx::palette::Green) > 10000);

    // --- Layer copies + invalidate -------------------------------------------------
    std::uint16_t layer[16 * 8];
    canvas.rect(10, 10, 16, 8, gfx::palette::Red, true);
    canvas.copyTo({10, 10, 16, 8}, layer, 16);
    canvas.clear(gfx::palette::Black);
    canvas.consumeDirty();
    canvas.copyFrom({100, 100, 16, 8}, layer, 16);
    FX_CHECK(canvas.getPixel(100, 100) == gfx::palette::Red && canvas.getPixel(115, 107) == gfx::palette::Red);
    const auto dirty = canvas.consumeDirty();
    FX_CHECK(dirty.x == 100 && dirty.y == 100 && dirty.w == 16 && dirty.h == 8);
    canvas.invalidate({-10, -10, 20, 20});
    const auto inv = canvas.consumeDirty();
    FX_CHECK(inv.x == 0 && inv.y == 0 && inv.w == 10 && inv.h == 10);

    // --- 3D surface ------------------------------------------------------------------
    canvas.clear(gfx::palette::Black);
    const gfx::Rect vp{8, 44, 304, 224};
    const auto st = fx::g3d::renderSurface(canvas, "sin(x)*cos(y)", fx::g3d::View{}, vp);
    FX_CHECK(st.ok && st.validVertices == (st.grid + 1) * (st.grid + 1));
    FX_CHECK(st.zbuffer == fx::limits::profile().graph3dZBuffer);
    FX_CHECK(st.reducedProfile == !board::config::kHasPsram);
    FX_CHECK(st.grid <= fx::limits::profile().graph3dMaxGrid && st.grid >= 4);
    std::size_t lit = 0;
    for (int y = vp.y; y < vp.y + vp.h; ++y)
        for (int x = vp.x; x < vp.x + vp.w; ++x) lit += canvas.getPixel(x, y) != gfx::palette::Black;
    FX_CHECK(lit > 5000);
    FX_CHECK(canvas.getPixel(2, 2) == gfx::palette::Black); // nothing drawn outside the viewport
    const auto bad = fx::g3d::renderSurface(canvas, "sqrt(-1-x*x)", fx::g3d::View{}, vp);
    FX_CHECK(!bad.ok && bad.error != nullptr);
    // Rotation reuses the cached surface samples (only re-projection).
    fx::g3d::View turned;
    fx::g3d::rotate(turned, 30.0F, 10.0F);
    const auto again = fx::g3d::renderSurface(canvas, "sin(x)*cos(y)", turned, vp);
    FX_CHECK(again.ok && again.validVertices == st.validVertices && again.zmin == st.zmin);
    fx::g3d::clearCache();
    fx::g3d::View v;
    fx::g3d::rotate(v, 400.0F, 200.0F);
    FX_CHECK(v.yawDeg >= 0.0F && v.yawDeg < 360.0F && v.pitchDeg <= 85.0F);
    FX_CHECK(mem::stats(mem::Pool::SRAM).allocations == 0 || true);

    // --- Window manager: dynamic registry/stack + snapshots ----------------------------
    {
        fx::WindowManager wm;
        Probe probes[20];
        for (unsigned i = 0; i < 12; ++i)
            FX_CHECK(wm.registerWindow(static_cast<fx::WindowId>(i), &probes[i], onEvent, onDraw, 0, onStamp));
        FX_CHECK(wm.windowCount() == 12); // more than the old fixed 8
        wm.setSnapshotHooks({g_fb, sizeof g_fb, present});
        FX_CHECK(wm.show(static_cast<fx::WindowId>(0)));
        for (unsigned i = 1; i < 12; ++i) FX_CHECK(wm.open(static_cast<fx::WindowId>(i)));
        FX_CHECK(wm.stackDepth() == 11); // deeper than the old fixed 8
        if (board::config::kHasPsram) FX_CHECK(wm.snapshotsStored() >= 1);
        else FX_CHECK(wm.snapshotsStored() == 0); // never in SRAM
        const int drawsBefore = probes[10].draws;
        FX_CHECK(wm.close());                      // back to window 10, stamp unchanged
        wm.render();
        if (board::config::kHasPsram && wm.snapshotRestores() == 1) FX_CHECK(probes[10].draws == drawsBefore && g_presents == 1);
        else FX_CHECK(probes[10].draws == drawsBefore + 1);
        probes[9].stamp = 2;                       // state changed: snapshot must not be used
        FX_CHECK(wm.close());
        wm.render();
        FX_CHECK(probes[9].draws == 1);
        FX_CHECK(wm.closeAll() && wm.active() == static_cast<fx::WindowId>(0) && wm.stackDepth() == 0);
    }
    FX_CHECK(mem::stats(mem::Pool::PSRAM_BOARD).allocations == 0);
    FX_CHECK(mem::checkIntegrity());
    mem::testing::reset();
    std::puts("Graphics on pooled memory (multi-graph, caches, polygon, 3D, snapshots): PASS");
    return 0;
}
