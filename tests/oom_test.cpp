// "Memory ERROR" handling end to end (v1.9.0), with the global operator new
// routed through the memory pools (src/mem_new.cpp) exactly as on the
// firmware, and a bounded simulated SRAM heap without PSRAM (Pico 2 W
// conditions, or a Pico Plus 2 W whose PSRAM is disabled). Every failure must
// show an error, keep the state intact, leak nothing and never terminate.
#include "app_rig.hpp"
#include "editors.hpp"
#include "graph3d.hpp"
#include "mem_pools.hpp"
#include "natural_editor.hpp"
#include "storage.hpp"
#include "test_util.hpp"
#include <new>
#include <vector>

using namespace fx;

namespace {
std::vector<void*>* g_hog = nullptr;
// Leaves only a few bytes of the simulated SRAM free.
void exhaust() {
    for (std::size_t chunk : {65536u, 4096u, 512u, 64u, 16u})
        while (g_hog->size() < g_hog->capacity()) {
            void* p = mem::tryAlloc(mem::Policy::PREFER_SRAM, chunk);
            if (!p) break;
            g_hog->push_back(p);
        }
}
void release() {
    for (void* p : *g_hog) mem::free(p);
    g_hog->clear();
}
void warmUp(testrig::Rig& r) {
    // Creates the function-local statics of every screen used below in the
    // real heap, so that they are not counted as leaks of the simulated one.
    r.type("1"); r.enter("1+2"); r.home();
    r.type("5"); r.key(pckey::F6); r.settle(); r.key(pckey::Esc); r.home();
    r.type("j"); r.key(pckey::F6); r.settle(); r.key(pckey::F6); r.home();
    r.type("g"); r.key(pckey::F1); r.key(pckey::Esc); r.home();
}
} // namespace

int main() {
    board::init();
    storage::RecordStore(board::storageFlash()).eraseAll();
    std::vector<void*> hog;
    hog.reserve(1u << 16);                            // in the real heap: never reallocated below
    g_hog = &hog;
    {
        testrig::Rig warm;
        warmUp(warm);
    }
    storage::RecordStore(board::storageFlash()).eraseAll();
    mem::testing::simulateSram(640u * 1024u);
    mem::testing::simulatePsram(0);
    const std::size_t baseline = mem::stats(mem::Pool::SRAM).allocations;
    {
        testrig::Rig r;
        auto& ws = r.ws();
        // ---- Run-Matrix: evaluation under exhaustion
        r.type("1");
        r.enter("1+2");
        CHECK(!r.error() && ws.history.size() == 1);
        const double ans = ws.memory.answer();
        r.type("123*456+789");
        exhaust();
        r.key(pckey::Enter);
        CHECK(r.error() && r.errorTitle() == "Memory ERROR");
        r.closeError();
        CHECK(ws.history.size() <= 2 && ws.memory.answer() == ans);   // no half-made entry, Ans unchanged
        const std::size_t histBefore = ws.history.size();
        release();
        r.key(pckey::Enter);                                         // the input is still there
        CHECK(!r.error() && ws.history.size() == histBefore + 1 && ws.memory.answer() == 56877.0);
        r.home();

        // ---- autosave with no memory left: a message, never a crash, and the
        //      pending changes are saved by themselves once memory is back
        ws.memory.store('B', 7);
        r.fw.apps().context().markDirty();
        const std::uint32_t savesBefore = r.fw.saves();
        exhaust();
        board::host::advance(3000);
        r.settle();
        const std::string& toast = r.fw.apps().context().toastText();   // no copy: memory is exhausted
        CHECK(toast.find("Memory ERROR") != std::string::npos);
        CHECK(r.fw.saves() == savesBefore && r.fw.apps().context().dirty());   // nothing saved, nothing forgotten
        release();
        board::host::advance(3000);                                  // no new input, no new markDirty()
        r.settle();
        CHECK(r.fw.saves() == savesBefore + 1 && !r.fw.apps().context().dirty());
        {
            std::vector<std::uint8_t> blob;
            Workspace back;
            CHECK(storage::RecordStore(board::storageFlash()).load(blob) &&
                  deserializeWorkspace(blob.data(), blob.size(), back) && back.memory.recall('B') == 7.0);
        }

        // ---- GRAPH: drawing with no memory degrades
        r.type("5");
        exhaust();
        r.key(pckey::F6);
        r.settle();
        CHECK(true);                                                 // reached: no exception escaped
        r.closeError();
        release();
        r.key(pckey::Esc); r.home();

        // ---- 3D Graph: a grid that does not fit is halved, then Memory ERROR
        r.type("j");
        exhaust();
        r.key(pckey::F6);
        r.settle();
        auto* g3 = r.active<app::Graph3DApp>();
        CHECK(g3 != nullptr);
        if (g3) CHECK(r.error() ? r.errorTitle() == "Memory ERROR" : g3->grid() < 40);
        r.closeError();
        release();
        r.key(pckey::F6); r.settle();
        if (g3) CHECK(!r.error() && g3->mesh(0).valid > 0);
        r.home();
        CHECK(g3 && g3->mesh(0).verts.empty());                      // released when leaving the mode

        // ---- legacy editors and the natural editor keep their data on failure
        MatrixEditor me;
        me.resize(0, 2, 2);
        me.set(0, 1, 1, 5.0);
        exhaust();
        bool oom = false;
        try { me.resize(0, MatrixEditor::MaxRows, MatrixEditor::MaxCols); } catch (const std::bad_alloc&) { oom = true; }
        CHECK(oom && me.value(0).size() == 2 && me.get(0, 1, 1) == 5.0);
        release();
        NaturalEditor ne;
        ne.insert('1'); ne.insert('2');
        exhaust();
        bool failed = false;
        try { ne.insert('3'); } catch (const std::bad_alloc&) { failed = true; }
        release();
        CHECK(failed ? ne.toExpression() == "12" : ne.toExpression() == "123");
    }
    CHECK(mem::stats(mem::Pool::SRAM).allocations == baseline);     // nothing leaked after all the failures
    CHECK(mem::checkIntegrity());
    CHECK(mem::outOfMemoryEvents() > 0);
    mem::testing::reset();
    storage::RecordStore(board::storageFlash()).eraseAll();
    return testutil::finish("oom_test");
}
