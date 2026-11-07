// MEMORY: overview of stored data with selective deletion and manual save.
#include "apps.hpp"
#include <cstdio>

namespace fx::app {
using namespace ui::theme;

namespace {
const char* kItems[] = {"Storia Run-Matrix", "Variabili A-Z e Ans", "Liste statistiche", "Funzioni Graph", "Coefficienti Equation", "Conversazioni AI"};
constexpr int ItemCount = 6;
}

void MemoryApp::key(Context& ctx, const KeyEvent& ev) {
    Workspace& ws = ctx.ws;
    if (confirm_ >= 0) {
        if (ev.is(Key::Exe)) {
            switch (confirm_) {
            case 0: ws.history.clear(); break;
            case 1: { const Settings s = ws.settings; ws.memory.clearAll(); ws.settings = s; break; }
            case 2: for (auto& l : ws.lists) l.clear(); break;
            case 3: for (std::size_t i = 0; i < ws.functions.size(); ++i) ws.functions[i] = GraphFunction{{}, GraphType::Y, true, static_cast<std::uint8_t>(i)}; break;
            case 4: ws.equation = EquationState{}; break;
            case 5: ws.chat.clear(); break;
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
    char counts[ItemCount][32];
    std::snprintf(counts[0], 32, "%u voci", static_cast<unsigned>(ws.history.size()));
    std::snprintf(counts[1], 32, "%d non nulle", vars);
    std::snprintf(counts[2], 32, "%u valori", static_cast<unsigned>(listItems));
    std::snprintf(counts[3], 32, "%d definite", fns);
    std::snprintf(counts[4], 32, "%d incognite", ws.equation.unknowns);
    std::snprintf(counts[5], 32, "%u messaggi", static_cast<unsigned>(ws.chat.size()));
    const int top = ui::ContentTop + 4;
    for (int i = 0; i < ItemCount; ++i) ui::listRow(c, top + i * 24, 22, kItems[i], counts[i], i == selected_);
    const auto blob = serializeWorkspace(ws);
    const std::size_t cap = ctx.platform.storageCapacity();
    const int y = top + ItemCount * 24 + 16;
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
