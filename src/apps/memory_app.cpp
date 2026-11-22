// MEMORY: overview of stored data with selective deletion and manual save;
// v1.15.0 (F3): the user files by type (eActivity on both boards; Program,
// Spreadsheet, Python, Geometry, Picture Plot on the Pico Plus 2 W).
#include "apps.hpp"
#include "typed_files.hpp"
#include <algorithm>
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

void MemoryApp::enter(Context&) {
    page_ = Page::Data;
    confirm_ = -1;
    dialog_.close();
}

void MemoryApp::openTypes(Context& ctx) {
    page_ = Page::Types;
    types_.clear(); typeUsage_.clear(); typeStatus_.clear();
    files::FileSystem* fs = userFiles(ctx);
    for (std::size_t i = 0; i < files::FileTypeCount; ++i) {
        const auto t = static_cast<files::FileType>(i);
        if (!files::fileTypeAvailable(t)) continue;
        types_.push_back(t);
        std::size_t count = 0;
        std::uint64_t bytes = 0;
        if (fs) {
            files::TypedStore store(*fs);
            std::vector<files::TypedEntry> list;
            const files::Status st = store.list(t, list);
            if (st != files::Status::Ok) typeStatus_ = files::statusMessage(st);
            for (const auto& e : list) { ++count; bytes += e.size; }
        }
        typeUsage_.emplace_back(count, bytes);
    }
    if (!fs) typeStatus_ = "Archivio dei file non disponibile";
    typeSel_ = std::clamp(typeSel_, 0, std::max(0, static_cast<int>(types_.size()) - 1));
}

void MemoryApp::openFiles(Context& ctx) {
    entries_.clear();
    files::FileSystem* fs = userFiles(ctx);
    if (!fs || types_.empty()) throw MathError("Storage ERROR: archivio non disponibile");
    files::TypedStore store(*fs);
    const files::Status st = store.list(types_[static_cast<std::size_t>(typeSel_)], entries_);
    if (st != files::Status::Ok) throw MathError(std::string("Storage ERROR: ") + files::statusMessage(st));
    page_ = Page::Files;
    fileSel_ = 0;
}

void MemoryApp::filesKey(Context& ctx, const KeyEvent& ev) {
    if (dialog_.active()) {
        if (dialog_.key(ev) == 1 && !entries_.empty()) {
            files::FileSystem* fs = userFiles(ctx);
            if (!fs) throw MathError("Storage ERROR: archivio non disponibile");
            files::TypedStore store(*fs);
            const files::Status st = store.remove(types_[static_cast<std::size_t>(typeSel_)], entries_[static_cast<std::size_t>(fileSel_)].file);
            if (st != files::Status::Ok) throw MathError(std::string("Storage ERROR: ") + files::statusMessage(st));
            ctx.toast("Eliminato");
            openFiles(ctx);
            fileSel_ = std::min(fileSel_, std::max(0, static_cast<int>(entries_.size()) - 1));
        }
        return;
    }
    if (page_ == Page::Types) {
        const int n = static_cast<int>(types_.size());
        switch (ev.key) {
        case Key::Up: if (n) typeSel_ = (typeSel_ + n - 1) % n; return;
        case Key::Down: if (n) typeSel_ = (typeSel_ + 1) % n; return;
        case Key::Exe: case Key::F1: if (n) openFiles(ctx); return;
        case Key::Exit: page_ = Page::Data; return;
        default: return;
        }
    }
    const int n = static_cast<int>(entries_.size());
    switch (ev.key) {
    case Key::Up: if (n) fileSel_ = (fileSel_ + n - 1) % n; return;
    case Key::Down: if (n) fileSel_ = (fileSel_ + 1) % n; return;
    case Key::F1: case Key::Del: case Key::DelForward:
        if (n) dialog_.openConfirm("MEMORY", "Eliminare " + entries_[static_cast<std::size_t>(fileSel_)].file + "?");
        return;
    case Key::Exit: openTypes(ctx); return;
    default: return;
    }
}

void MemoryApp::key(Context& ctx, const KeyEvent& ev) {
    Workspace& ws = ctx.ws;
    if (page_ != Page::Data) { filesKey(ctx, ev); return; }
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
    case Key::F3: openTypes(ctx); return;                     // v1.15.0: FILES
    case Key::F6: ctx.markDirty(); ctx.toast("Salvataggio in corso"); return;
    case Key::Exit: ctx.home(); return;
    default: return;
    }
}

void MemoryApp::drawFiles(Context& ctx, gfx::Canvas& c) {
    (void)ctx;
    const int top = ui::ContentTop + 4;
    if (page_ == Page::Types) {
        ui::text(c, 8, top, "File per tipo", Accent, font::Face::Small);
        for (std::size_t i = 0; i < types_.size(); ++i) {
            const files::FileTypeInfo& t = files::fileType(types_[i]);
            char right[48];
            std::snprintf(right, sizeof right, "%u file, %lu byte", static_cast<unsigned>(typeUsage_[i].first), static_cast<unsigned long>(typeUsage_[i].second));
            ui::listRow(c, top + 16 + static_cast<int>(i) * 21, 20, t.name, right, static_cast<int>(i) == typeSel_);
        }
        if (!typeStatus_.empty()) ui::text(c, 8, top + 16 + static_cast<int>(types_.size()) * 21 + 6, typeStatus_.c_str(), Error, font::Face::Small);
        const char* k[6] = {"OPEN", nullptr, nullptr, nullptr, nullptr, nullptr};
        ui::softKeys(c, k);
        return;
    }
    const files::FileTypeInfo& t = files::fileType(types_[static_cast<std::size_t>(typeSel_)]);
    ui::text(c, 8, top, (std::string(t.name) + " (" + t.folder + ")").c_str(), Accent, font::Face::Small);
    if (entries_.empty()) ui::text(c, 8, top + 24, "Nessun file", Muted);
    const int visible = 11;
    const int first = std::max(0, fileSel_ - visible + 1);
    for (int i = 0; i < visible && first + i < static_cast<int>(entries_.size()); ++i) {
        const auto& e = entries_[static_cast<std::size_t>(first + i)];
        char right[32];
        std::snprintf(right, sizeof right, "%lu byte", static_cast<unsigned long>(e.size));
        ui::listRow(c, top + 16 + i * 21, 20, e.file.c_str(), right, first + i == fileSel_);
    }
    const char* k[6] = {"DELETE", nullptr, nullptr, nullptr, nullptr, nullptr};
    ui::softKeys(c, k);
    dialog_.draw(c);
}

void MemoryApp::draw(Context& ctx, gfx::Canvas& c) {
    ui::clear(c);
    ctx.drawStatus(c, "MEMORY");
    if (page_ != Page::Data) { drawFiles(ctx, c); return; }
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
    const char* k[6] = {"DELETE", nullptr, "FILES", nullptr, nullptr, "SAVE"};
    ui::softKeys(c, k);
    if (confirm_ >= 0) ui::errorBox(c, "ELIMINARE?", (std::string(kItems[confirm_]) + "  EXE = conferma").c_str());
}
} // namespace fx::app
