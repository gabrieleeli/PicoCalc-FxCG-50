// MEMORY: overview of stored data with selective deletion and manual save.
#include "apps.hpp"
#include <cstdio>

namespace fx::app {
using namespace ui::theme;

namespace {
// v1.14.0: i dati del 3D Graph come nel Memory Manager della fx-CG50 (3DGRAPH,
// 3DGMEM n, 3DVWIN_n): funzioni, V-Window e Setup; 3D G-Mem 1-20; 3D V-Win 1-6.
const char* kItems[] = {"Storia Run-Matrix", "Variabili A-Z e Ans", "Liste 1-26 e sottonomi", "Funzioni Graph", "Coefficienti Equation",
                        "Conversazioni AI", "3D Graph (funzioni)", "Matrici e vettori", "3D G-Mem 1-20", "3D V-Win 1-6"};
constexpr int ItemCount = 10;
}

void MemoryApp::key(Context& ctx, const KeyEvent& ev) {
    Workspace& ws = ctx.ws;
    if (confirm_ >= 0) {
        if (ev.is(Key::Exe)) {
            switch (confirm_) {
            case 0: ws.history.clear(); break;
            case 1: { const Settings s = ws.settings; ws.memory.clearAll(); ws.settings = s; break; }
            case 2: for (auto& l : ws.lists) l.clear(); for (auto& n : ws.listNames) n.clear(); break;
            case 3: for (std::size_t i = 0; i < ws.functions.size(); ++i) ws.functions[i] = emptyGraphFunction(i); break;
            case 4: ws.equation = EquationState{}; break;
            case 5: ws.chat.clear(); break;
            case 6: ws.graph3d.set = g3d::GraphSet{}; ws.graph3d.zoom = 1.0F; break;
            case 7: ws.matrices.clear(); break;
            case 8: for (auto& m : ws.graph3d.gmem) m.clear(); break;
            case 9: ws.graph3d.vmem = {}; ws.graph3d.vmemUsed = 0; break;
            default: break;
            }
            ctx.markDirty();
            ctx.toast("Eliminato");
        }
        confirm_ = -1;
        return;
    }
    switch (ev.key) {
    case Key::Up: selected_ = (selected_ + ItemCount - 1) % ItemCount; return;
    case Key::Down: selected_ = (selected_ + 1) % ItemCount; return;
    case Key::F1: case Key::DelForward: case Key::Del: confirm_ = selected_; return;
    case Key::F6: ctx.markDirty(); ctx.toast("Salvataggio in corso"); return;
    case Key::Exit: ctx.home(); return;
    default: return;
    }
}

void MemoryApp::draw(Context& ctx, gfx::Canvas& c) {
    ui::clear(c);
    ctx.drawStatus(c, "MEMORY");
    const Workspace& ws = ctx.ws;
    int vars = 0; for (char v = 'A'; v <= 'Z'; ++v) if (ws.memory.recall(v) != 0) ++vars;
    std::size_t listItems = 0; for (const auto& l : ws.lists) listItems += l.size();
    int fns = 0; for (const auto& f : ws.functions) if (!f.expr.empty()) ++fns;
    char counts[ItemCount][40];
    std::snprintf(counts[0], 40, "%u voci", static_cast<unsigned>(ws.history.size()));
    std::snprintf(counts[1], 40, "%d non nulle", vars);
    std::snprintf(counts[2], 40, "%u / %u valori", static_cast<unsigned>(listItems), static_cast<unsigned>(Workspace::MaxListElements));
    std::snprintf(counts[3], 40, "%d definite", fns);
    std::snprintf(counts[4], 40, "%d incognite", ws.equation.unknowns);
    std::snprintf(counts[5], 40, "%u messaggi", static_cast<unsigned>(ws.chat.size()));
    int fns3d = 0; for (const auto& f : ws.graph3d.set.fns) if (f.defined()) ++fns3d;
    std::snprintf(counts[6], 40, "%d definite", fns3d);
    int gmem = 0; for (const auto& m : ws.graph3d.gmem) gmem += m.empty() ? 0 : 1;
    std::snprintf(counts[8], 40, "%d usate, %u byte", gmem, static_cast<unsigned>(ws.graph3d.memoryBytes()));
    int vmem = 0; for (std::size_t k = 0; k < g3d::WindowMemories; ++k) vmem += (ws.graph3d.vmemUsed >> k) & 1u;
    std::snprintf(counts[9], 40, "%d usate", vmem);
    int defined = 0; for (const auto& m : ws.matrices.mat) defined += m.empty() ? 0 : 1;
    for (const auto& v : ws.matrices.vct) defined += v.empty() ? 0 : 1;
    std::snprintf(counts[7], 40, "%d, %u elementi", defined, static_cast<unsigned>(ws.matrices.elements()));
    const int top = ui::ContentTop + 4;
    for (int i = 0; i < ItemCount; ++i) ui::listRow(c, top + i * 21, 20, kItems[i], counts[i], i == selected_);
    const auto blob = serializeWorkspace(ws);
    const std::size_t cap = ctx.platform.storageCapacity();
    const int y = top + ItemCount * 21 + 6;
    char used[64]; std::snprintf(used, sizeof(used), "Flash: %u / %u byte", static_cast<unsigned>(blob.size()), static_cast<unsigned>(cap));
    ui::text(c, 8, y, used, Ink, font::Face::Small);
    c.rect(8, y + 16, ui::ScreenW - 16, 10, Faint, false);
    const int w = cap ? static_cast<int>((ui::ScreenW - 18) * std::min<std::size_t>(blob.size(), cap) / cap) : 0;
    c.rect(9, y + 17, w, 8, blob.size() * 10 > cap * 8 ? Warn : Ok, true);
    ui::text(c, 8, y + 34, "Salvataggio automatico A/B con CRC (flash interna).", Muted, font::Face::Small);
    const char* k[6] = {"DELETE", nullptr, nullptr, nullptr, nullptr, "SAVE"};
    ui::softKeys(c, k);
    if (confirm_ >= 0) ui::errorBox(c, "ELIMINARE?", (std::string(kItems[confirm_]) + "  EXE = conferma").c_str());
}
} // namespace fx::app
