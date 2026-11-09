// Graphics on pooled memory (v1.9.0): canvas features ported from the 1.9.0
// line (filled polygons above the old 32-vertex limit with a pooled scratch,
// layer copies, invalidate, dirty-region capacity per board) and the GRAPH /
// PICT budgets that follow the PSRAM pool: the dense curves that the SRAM
// budget truncates are drawn completely when the PSRAM pool exists.
#include "app_rig.hpp"
#include "board_config.hpp"
#include "graph_engine.hpp"
#include "limits.hpp"
#include "mem_pools.hpp"
#include "pict.hpp"
#include "storage.hpp"
#include "test_util.hpp"
#include <algorithm>
#include <cmath>
#include <vector>

using namespace fx;

namespace {
std::uint16_t fb[320 * 320];
std::size_t countColor(std::uint16_t color) {
    std::size_t n = 0;
    for (auto p : fb) n += p == color;
    return n;
}
// Draws 20 dense Lissajous curves in GRAPH and returns (points, truncated curves).
std::pair<std::size_t, std::size_t> denseGraphs(testrig::Rig& r) {
    auto& ws = r.ws();
    r.home(); r.type("5");
    for (std::size_t i = 0; i < MaxGraphFunctions; ++i) {
        ws.functions[i] = emptyGraphFunction(i);
        ws.functions[i].type = GraphType::Param;
        ws.functions[i].expr = "6sin(" + std::to_string(37 + i) + "t)";
        ws.functions[i].expr2 = "3cos(" + std::to_string(41 + i) + "t)";
        ws.functions[i].selected = true;
    }
    ws.view = ViewWindow{};
    ws.view.tstep = 0.0005;
    r.key(pckey::F6);
    r.settle(20000);
    std::size_t points = 0, truncated = 0;
    if (auto* g = r.active<app::GraphApp>())
        for (const auto& c : g->curves()) { points += c.xy.size() / 2; truncated += c.truncated ? 1 : 0; }
    r.key(pckey::Esc);
    return {points, truncated};
}
} // namespace

int main() {
    gfx::Canvas canvas(fb, 320, 320);
    CHECK(gfx::DirtyRegions::Capacity == limits::kDirtyRegionCapacity);
    CHECK(limits::kDirtyRegionCapacity == (board::config::kHasPsram ? 32u : 16u));

    // ---- filled polygon above the old 32-vertex limit (scratch from the pools)
    for (const bool psram : {false, true}) {
        mem::testing::simulatePsram(psram ? (8u << 20) : 0u);
        std::vector<gfx::Point> star;
        for (int i = 0; i < 80; ++i) {
            const double a = i * 2.0 * 3.14159265358979323846 / 80.0;
            const double rr = (i % 2) ? 60.0 : 120.0;
            star.push_back({160 + static_cast<int>(rr * std::cos(a)), 160 + static_cast<int>(rr * std::sin(a))});
        }
        canvas.clear(gfx::palette::Black);
        canvas.polygon(star.data(), star.size(), gfx::palette::Green, true);
        CHECK(canvas.getPixel(160, 160) == gfx::palette::Green);
        CHECK(countColor(gfx::palette::Green) > 10000);
        // above the RAM profile limit only the outline is drawn
        std::vector<gfx::Point> huge(limits::ram().polygonMaxVertices + 1);
        for (std::size_t i = 0; i < huge.size(); ++i) {
            const double a = static_cast<double>(i) * 2.0 * 3.14159265358979323846 / static_cast<double>(huge.size());
            huge[i] = {160 + static_cast<int>(100 * std::cos(a)), 160 + static_cast<int>(100 * std::sin(a))};
        }
        canvas.clear(gfx::palette::Black);
        canvas.polygon(huge.data(), huge.size(), gfx::palette::Red, true);
        CHECK(canvas.getPixel(160, 160) == gfx::palette::Black && countColor(gfx::palette::Red) > 200);
    }
    mem::testing::reset();

    // ---- layer copies + invalidate
    {
        std::uint16_t layer[16 * 8];
        canvas.clear(gfx::palette::Black);
        canvas.rect(10, 10, 16, 8, gfx::palette::Red, true);
        canvas.copyTo({10, 10, 16, 8}, layer, 16);
        canvas.clear(gfx::palette::Black);
        canvas.consumeDirty();
        canvas.copyFrom({100, 100, 16, 8}, layer, 16);
        CHECK(canvas.getPixel(100, 100) == gfx::palette::Red && canvas.getPixel(115, 107) == gfx::palette::Red);
        const auto dirty = canvas.consumeDirty();
        CHECK(dirty.x == 100 && dirty.y == 100 && dirty.w == 16 && dirty.h == 8);
        canvas.invalidate({-10, -10, 20, 20});
        const auto inv = canvas.consumeDirty();
        CHECK(inv.x == 0 && inv.y == 0 && inv.w == 10 && inv.h == 10);
    }

    // ---- PICT budget: a noisy picture larger than 96 KB of RLE data
    {
        std::vector<std::uint16_t> noisy(300 * 200);
        std::uint32_t s = 12345;
        for (auto& p : noisy) { s ^= s << 13; s ^= s >> 17; s ^= s << 5; p = static_cast<std::uint16_t>(s); }
        pict::Image img;
        mem::testing::simulatePsram(0);
        CHECK(pict::maxEncodedBytes() == 96u * 1024u && !pict::encode(noisy.data(), 300, 300, 200, img));
        mem::testing::simulatePsram(8u << 20);
        CHECK(pict::maxEncodedBytes() == 1024u * 1024u && pict::encode(noisy.data(), 300, 300, 200, img));
        CHECK(img.rle.size() > 96u * 1024u && img.width == 300 && img.height == 200);
        mem::testing::reset();
    }

    // ---- GRAPH: 20 dense parametric curves
    board::init();
    {
        storage::RecordStore(board::storageFlash()).eraseAll();
        testrig::Rig r;
        mem::testing::simulatePsram(0);
        const auto sram = denseGraphs(r);
        CHECK(sram.first <= limits::kSramProfile.graphTotalCurvePoints && sram.second > 0);   // truncated (v1.8.0 budget)
        mem::testing::simulatePsram(8u << 20);
        const auto psram = denseGraphs(r);
        CHECK(psram.second == 0 && psram.first > 4 * limits::kSramProfile.graphTotalCurvePoints);   // complete with PSRAM
        CHECK(psram.first <= limits::kPsramProfile.graphTotalCurvePoints);
        // TRACE over the dense graphs: the static layers are kept in PSRAM and
        // the cached frame is pixel-identical to a full redraw.
        r.key(pckey::F6); r.settle(20000);                                // plot again (Esc left it)
        auto* g = r.active<app::GraphApp>();
        CHECK(g != nullptr);
        if (g) {
            r.key(pckey::F1);                                             // TRACE
            for (int i = 0; i < 5; ++i) r.key(pckey::Right);
            CHECK(g->mode() == app::GraphApp::Mode::Trace && g->layerCached());
            std::vector<std::uint16_t> cached(board::graphics().pixels(), board::graphics().pixels() + 320 * 320);
            mem::testing::simulatePsram(0);                               // SRAM profile: no cache, full redraw
            r.fw.draw(board::graphics());
            CHECK(!g->layerCached());
            const bool same = std::equal(cached.begin(), cached.end(), board::graphics().pixels());
            CHECK(same);
            mem::testing::simulatePsram(8u << 20);
            r.key(pckey::Right); r.key(pckey::Right);
            CHECK(g->layerCached());
            r.key(pckey::Esc);                                            // any other key drops the cache
            CHECK(!g->layerCached());
            r.key(pckey::Esc);
        }
        r.ws().functions = Workspace{}.functions;
        r.ws().view = ViewWindow{};
        mem::testing::reset();
    }
    return testutil::finish("graphics_memory_test");
}
