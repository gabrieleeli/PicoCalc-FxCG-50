// "Memoria insufficiente" handling end to end, with operator new routed to a
// small simulated heap (src/mem_new.cpp): every failure must leave the state
// intact (rollback), leak nothing and never abort. Also covers the structural
// recursion guards that protect the SRAM stack.
#include "editors.hpp"
#include "expression.hpp"
#include "graph3d.hpp"
#include "graph_renderer.hpp"
#include "limits.hpp"
#include "math_engine.hpp"
#include "mem_pools.hpp"
#include "natural_editor.hpp"
#include "run_matrix.hpp"
#include "test_support.hpp"
#include "window_manager.hpp"

#include <new>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
std::vector<void*> g_hog;
void exhaust() {
    // Leave only a few dozen bytes: enough for nothing substantial. g_hog never
    // reallocates (capacity reserved up front).
    for (std::size_t chunk : {65536u, 4096u, 512u, 64u, 16u})
        while (g_hog.size() < g_hog.capacity()) {
            void* p = mem::tryAlloc(mem::Policy::PREFER_SRAM, chunk);
            if (!p) break;
            g_hog.push_back(p);
        }
}
void release() {
    for (void* p : g_hog) mem::free(p);
    g_hog.clear();
}
std::uint16_t g_framebuffer[320u * 320u];
void noop(void*, const fx::InputEvent&) {}
void noopDraw(void*) {}

void typeText(fx::RunMatrixController& c, const char* text) {
    for (const char* p = text; *p; ++p) c.handle({fx::InputAction::Character, *p});
}
} // namespace

int main() {
    // Pico 2 W-like conditions: a bounded SRAM heap, no PSRAM.
    mem::testing::simulateSram(256u * 1024u);
    mem::testing::simulatePsram(0);
    g_hog.reserve(16384);
    const std::size_t baseline = mem::stats(mem::Pool::SRAM).allocations;
    {
        // --- Run-Matrix: evaluation under memory exhaustion -------------------
        fx::RunMatrixController c;
        c.handle({fx::InputAction::Evaluate, 0}); // open RUN-MATRIX
        typeText(c, "1+2");
        c.handle({fx::InputAction::Evaluate, 0});
        FX_CHECK(c.result() == "3" && c.session().history().size() == 1);
        typeText(c, "*7+0+0+0+0+0+0"); // > 15 chars: the history copy needs the heap
        exhaust();
        c.handle({fx::InputAction::Evaluate, 0});
        FX_CHECK(c.message() == fx::limits::kOutOfMemoryMessage);
        FX_CHECK(c.result().empty());
        FX_CHECK(c.session().history().size() == 1);          // history unchanged
        FX_CHECK(c.session().memory().answer() == 3.0);        // Ans unchanged
        release();
        c.handle({fx::InputAction::Evaluate, 0});
        FX_CHECK(c.result() == "15" && c.session().history().size() == 2); // editor content preserved
        // A short entry under pressure degrades gracefully: the oldest history
        // entries make room instead of failing the calculation.
        c.handle({fx::InputAction::Clear, 0});
        typeText(c, "6*7");
        exhaust();
        c.handle({fx::InputAction::Evaluate, 0});
        FX_CHECK(c.result() == "42" || c.message() == fx::limits::kOutOfMemoryMessage);
        FX_CHECK(c.session().history().size() >= 1 && c.session().history().size() <= 3);
        release();

        // --- Editors: failed resize keeps the previous data ----------------------
        fx::MatrixEditor me;
        me.resize(0, 2, 2);
        me.set(0, 1, 1, 5.0);
        exhaust();
        bool oom = false;
        try { me.resize(0, fx::MatrixEditor::MaxRows, fx::MatrixEditor::MaxCols); } catch (const std::bad_alloc&) { oom = true; }
        FX_CHECK(oom && me.value(0).size() == 2 && me.get(0, 1, 1) == 5.0);
        release();
        bool range = false;
        try { me.resize(0, fx::MatrixEditor::MaxRows + 1, 1); } catch (const std::out_of_range&) { range = true; }
        FX_CHECK(range);

        fx::StatisticsEditor se;
        se.set(0, 0, 1.0);
        bool full = false;
        try { se.set(0, fx::limits::listMaxLength(), 2.0); } catch (const std::length_error&) { full = true; }
        FX_CHECK(full && se.list(0).size() == 1);
        FX_CHECK(fx::StatisticsEditor::Lists == 26);
        exhaust();
        oom = false;
        try { se.set(1, 900, 3.0); } catch (const std::bad_alloc&) { oom = true; }
        FX_CHECK(oom && se.list(1).empty());
        release();

        fx::Spreadsheet sheet;
        FX_CHECK(fx::Spreadsheet::Rows == 999 && fx::Spreadsheet::Cols == 26);
        sheet.set(998, 25, 4.0);
        sheet.set(0, 25, 2.0);
        FX_CHECK(sheet.get(998, 25) == 4.0 && sheet.usedCells() == 2 && sheet.sum(25, 0, 998) == 6.0);
        exhaust();
        oom = false;
        for (std::size_t r = 1; r < 900 && !oom; ++r) {
            try { sheet.set(r, 0, 1.0); } catch (const std::bad_alloc&) { oom = true; }
        }
        FX_CHECK(oom && sheet.get(998, 25) == 4.0);
        release();
        sheet.set(998, 25, 0.0); // clearing a cell frees it
        FX_CHECK(sheet.get(998, 25) == 0.0);

        // --- Natural editor: structural insert under OOM -------------------------
        fx::NaturalEditor ne;
        ne.insert('1');
        ne.insert('2');
        exhaust();
        ne.handle({fx::NaturalKey::Character, '3'});
        FX_CHECK(!ne.error().empty());
        release();
        FX_CHECK(ne.toExpression() == "12");
        bool nested = false;
        try { for (unsigned i = 0; i < fx::limits::kMaxEditorNesting + 2; ++i) ne.insertGroup(); }
        catch (const std::length_error&) { nested = true; }
        FX_CHECK(nested);

        // --- Window manager: registry growth fails cleanly -----------------------
        fx::WindowManager wm;
        FX_CHECK(wm.registerWindow(fx::WindowId::Home, nullptr, noop, noopDraw));
        exhaust();
        std::size_t registered = 1;
        for (unsigned i = 0; i < 40; ++i)
            if (wm.registerWindow(static_cast<fx::WindowId>(100 + i), nullptr, noop, noopDraw)) ++registered;
        FX_CHECK(registered == 8);                // growth beyond the first block refused under OOM
        FX_CHECK(wm.show(fx::WindowId::Home));    // still usable
        release();

        // --- Graph and 3D renderers degrade instead of failing -------------------
        gfx::Canvas canvas(g_framebuffer, 320, 320);
        const std::vector<std::string> functions{std::string("sin(x)"), std::string("x/2")};
        fx::GraphRenderer::clearCache();
        exhaust();
        fx::GraphRenderer::draw(canvas, functions, fx::GraphView{}, 0);
        FX_CHECK(fx::GraphRenderer::cacheStats().directFallback);
        const auto st = fx::g3d::renderSurface(canvas, "x*y", fx::g3d::View{}, {8, 44, 304, 224});
        FX_CHECK(!st.ok || st.grid < fx::limits::profile().graph3dMaxGrid);
        release();
        fx::GraphRenderer::clearCache();
        fx::g3d::clearCache();
    }
    FX_CHECK(mem::stats(mem::Pool::SRAM).allocations == baseline); // no leaks after all failures
    FX_CHECK(mem::checkIntegrity());

    // --- Structural guards: deep nesting is rejected before the stack runs out ---
    {
    std::string deep(400, '(');
    deep += "1";
    deep += std::string(400, ')');
    bool rejected = false;
    try { (void)fx::evaluate(deep); } catch (const std::runtime_error&) { rejected = true; }
    FX_CHECK(rejected);
    rejected = false;
    try { (void)fx::parseExpression(deep); } catch (const std::runtime_error&) { rejected = true; }
    FX_CHECK(rejected);
    std::string ok(20, '(');
    ok += "2";
    ok += std::string(20, ')');
    FX_CHECK(fx::evaluate(ok) == 2.0);
    rejected = false;
    try { (void)fx::evaluate(std::string(500, '-') + "1"); } catch (const std::runtime_error&) { rejected = true; }
    FX_CHECK(rejected);
    } // strings released before the simulated heap goes away

    std::vector<void*>().swap(g_hog); // its buffer lives in the simulated heap
    mem::testing::reset();
    std::puts("Out-of-memory handling and structural guards: PASS");
    return 0;
}
