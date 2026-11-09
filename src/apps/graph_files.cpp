// GRAPH files: .pcgraph projects (New, Open, Save, Save As, Rename, Delete),
// graph memories G-Mem1..20 (flash workspace), PICT images (Pict1..20 and
// named files, background and recalled pictures, BGV-WIN) and BMP export on
// the microSD card. A project is the editable GRAPH state; a picture is only
// an image of the graph screen.
#include "apps.hpp"
#include "graph_file.hpp"
#include <algorithm>
#include <cstdio>

namespace fx::app {
using namespace ui::theme;

namespace {
[[noreturn]] void fail(files::Status st) { throw MathError(std::string("Storage ERROR: ") + files::statusMessage(st)); }
void check(files::Status st) { if (st != files::Status::Ok) fail(st); }
std::string baseName(const std::string& path) {
    const auto slash = path.rfind('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
}
} // namespace

files::FileSystem* GraphApp::fileSystem(Context& ctx, bool required) {
    files::FileSystem* fs = userFiles(ctx);
    if (!fs) { if (required) throw MathError("Storage ERROR: archivio file non disponibile in questa build"); return nullptr; }
    const files::Status st = fs->status();
    if (st != files::Status::Ok) { if (required) fail(st); return nullptr; }
    return fs;
}

// ---------------------------------------------------------------- projects
void GraphApp::newProject(Context& ctx) {
    Workspace& ws = ctx.ws;
    for (std::size_t i = 0; i < ws.functions.size(); ++i) ws.functions[i] = emptyGraphFunction(i);
    ws.view = graph::initialWindow(ws.settings.angle);
    const auto gmem = ws.graph.gmem;                 // graph memories belong to the calculator, not to a project
    ws.graph = GraphState{};
    ws.graph.gmem = gmem;
    overlay_ = pict::Image{}; overlayPath_.clear(); background_ = pict::Image{}; backgroundPath_.clear();
    pasted_.clear(); marks_.clear(); zoomStack_.clear(); haveOriginal_ = false; shadeIntegral_ = false; shade_.clear();
    row_ = 0; firstRow_ = 0;
    changed(ctx);
    ws.graph.projectModified = false;
    screen_ = Screen::List;
    ctx.toast("Nuovo progetto");
}

void GraphApp::saveProject(Context& ctx, const std::string& name) {
    if (!files::validName(name, 16)) throw MathError("Argument ERROR: nome di 1-16 caratteri A-Z 0-9 _ -");
    files::FileSystem* fs = fileSystem(ctx, true);
    const Workspace& ws = ctx.ws;
    auto holder = std::make_unique<graphfile::Project>();      // ~2 KB: not on the 32 KB stack
    graphfile::Project& p = *holder;
    p.functions = ws.functions;
    p.view = ws.view;
    p.angle = ws.settings.angle;
    p.settings = ws.graph.settings;
    p.vmem = ws.graph.vmem;
    p.vmemUsed = ws.graph.vmemUsed;
    p.sketch = ws.graph.sketch;
    p.overlay = ws.graph.overlay;
    p.tableStart = ws.table.start; p.tableEnd = ws.table.end; p.tableStep = ws.table.step;
    p.tableFromList = ws.table.variable == TableRange::Variable::List; p.tableList = ws.table.list; p.tableFunc = ws.table.funcType;
    const std::string text = graphfile::encode(p);
    if (text.size() > graphfile::MaxProjectBytes) throw MathError("Memory ERROR: progetto troppo grande");
    check(fs->makeDirs(files::GraphFolder));
    std::uint64_t free = 0;
    if (fs->freeSpace(free) == files::Status::Ok && free < 2 * text.size() + 4096) fail(files::Status::Full);
    check(files::writeAtomic(*fs, files::join(files::GraphFolder, name + files::ProjectExtension),
                             reinterpret_cast<const std::uint8_t*>(text.data()), text.size()));
    ctx.ws.graph.project = name;
    ctx.ws.graph.projectModified = false;
    ctx.markDirty();
    ctx.toast("Progetto salvato: " + name);
}

void GraphApp::openProject(Context& ctx, const std::string& name) {
    files::FileSystem* fs = fileSystem(ctx, true);
    std::vector<std::uint8_t> data;
    const files::Status st = files::readAll(*fs, files::join(files::GraphFolder, name + files::ProjectExtension), data, graphfile::MaxProjectBytes);
    if (st == files::Status::TooLarge) throw MathError("Storage ERROR: progetto troppo grande");
    check(st);
    auto holder = std::make_unique<graphfile::Project>();
    graphfile::Project& p = *holder;
    const std::string err = graphfile::decode(std::string(data.begin(), data.end()), p);
    if (!err.empty()) throw MathError("Storage ERROR: " + name + " non valido (" + err + ")");
    Workspace& ws = ctx.ws;
    ws.functions = p.functions;
    ws.view = p.view;
    ws.settings.angle = p.angle;
    const auto gmem = ws.graph.gmem;
    ws.graph = GraphState{};
    ws.graph.gmem = gmem;
    ws.graph.settings = p.settings;
    ws.graph.vmem = p.vmem;
    ws.graph.vmemUsed = p.vmemUsed;
    ws.graph.sketch = p.sketch;
    ws.graph.overlay = p.overlay;
    ws.graph.project = name;
    ws.table.start = p.tableStart; ws.table.end = p.tableEnd; ws.table.step = p.tableStep;
    ws.table.variable = p.tableFromList ? TableRange::Variable::List : TableRange::Variable::Range; ws.table.list = p.tableList; ws.table.funcType = p.tableFunc;
    ws.table.rows.clear(); ws.table.generated = false;      // the table of the opened project is generated again in TABLE
    ws.apply();
    pasted_.clear(); marks_.clear(); zoomStack_.clear(); haveOriginal_ = false; shadeIntegral_ = false; shade_.clear();
    overlay_ = pict::Image{}; overlayPath_.clear(); background_ = pict::Image{}; backgroundPath_.clear();
    row_ = 0; firstRow_ = 0;
    changed(ctx);
    ws.graph.projectModified = false;
    screen_ = Screen::List;
    std::string note;
    // Pictures referenced by the project are optional: a missing one is reported, not fatal.
    try { loadBackground(ctx); } catch (const std::exception&) { note = " (sfondo mancante)"; }
    if (!ws.graph.overlay.empty()) {
        try { recallPicture(ctx, ws.graph.overlay, false); } catch (const std::exception&) { ws.graph.overlay.clear(); note += " (immagine mancante)"; }
        ws.graph.projectModified = false;
    }
    ctx.toast("Progetto aperto: " + name + note, 2200);
}

// ---------------------------------------------------------------- file list
void GraphApp::openFiles(Context& ctx, Browse purpose) {
    files_ = FileList{};
    files_.purpose = purpose;
    files_.folder = purpose == Browse::Projects ? files::GraphFolder : files::PictFolder;
    if (screen_ != Screen::Files) returnScreen_ = screen_ == Screen::Edit ? Screen::List : screen_;
    screen_ = Screen::Files;
    files::FileSystem* fs = userFiles(ctx);
    if (!fs) { files_.status = "archivio file non disponibile in questa build"; return; }
    files::Status st = fs->status();
    if (st == files::Status::Ok) st = fs->makeDirs(files_.folder);
    if (st == files::Status::Ok) st = files::recover(*fs, files_.folder);   // finish interrupted saves
    std::vector<files::Entry> all;
    if (st == files::Status::Ok) st = fs->list(files_.folder, all);
    if (st != files::Status::Ok) { files_.status = files::statusMessage(st); return; }
    const char* ext = purpose == Browse::Projects ? files::ProjectExtension : files::PictExtension;
    for (auto& e : all) if (!e.directory && files::hasExtension(e.name, ext)) files_.entries.push_back(std::move(e));
    std::sort(files_.entries.begin(), files_.entries.end(), [](const files::Entry& a, const files::Entry& b) { return a.name < b.name; });
    std::uint64_t free = 0;
    if (fs->freeSpace(free) == files::Status::Ok) {
        char buf[48];
        std::snprintf(buf, sizeof(buf), "%u file, liberi %llu KB", static_cast<unsigned>(files_.entries.size()), static_cast<unsigned long long>(free / 1024));
        files_.status = buf;
    }
    if (files_.entries.empty()) files_.status = "Nessun file in " + files_.folder;
}

void GraphApp::deleteFile(Context& ctx) {
    if (files_.entries.empty()) return;
    files::FileSystem* fs = fileSystem(ctx, true);
    const std::string name = files_.entries[static_cast<std::size_t>(files_.selected)].name;
    check(fs->remove(files::join(files_.folder, name)));
    if (files_.purpose == Browse::Projects && files::stem(name) == ctx.ws.graph.project) { ctx.ws.graph.project.clear(); ctx.ws.graph.projectModified = true; ctx.markDirty(); }
    ctx.toast("Eliminato: " + name);
    openFiles(ctx, files_.purpose);
    files_.selected = 0;
}

void GraphApp::filesKey(Context& ctx, const KeyEvent& ev) {
    const int n = static_cast<int>(files_.entries.size());
    auto selectedName = [&]() { return files_.entries[static_cast<std::size_t>(files_.selected)].name; };
    switch (ev.key) {
    case Key::Up: if (n) files_.selected = (files_.selected + n - 1) % n; return;
    case Key::Down: if (n) files_.selected = (files_.selected + 1) % n; return;
    case Key::Exit: screen_ = returnScreen_; if (screen_ == Screen::Plot) mode_ = Mode::View; return;
    case Key::F1: case Key::Exe: {
        if (!n) return;
        const std::string name = selectedName();
        if (files_.purpose == Browse::Projects) { openProject(ctx, files::stem(name)); return; }
        const std::string path = files::join(files_.folder, name);
        const bool bg = files_.purpose == Browse::Background;
        screen_ = returnScreen_;
        if (screen_ == Screen::Plot) mode_ = Mode::View;
        recallPicture(ctx, path, bg);
        return;
    }
    case Key::F2: if (n) confirm(Confirm::DeleteFile, "Eliminare " + selectedName() + "?"); return;
    case Key::F3: if (n) askValue(Ask::ProjectRename, "Nuovo nome:", files::stem(selectedName()), true); return;
    case Key::F4:
        if (n && files_.purpose != Browse::Projects) {
            // BMP copy of a picture, readable on a PC.
            files::FileSystem* fs = fileSystem(ctx, true);
            std::vector<std::uint8_t> data;
            check(files::readAll(*fs, files::join(files_.folder, selectedName()), data, pict::maxEncodedBytes() + 256));
            pict::Image img;
            const std::string err = pict::parse(data.data(), data.size(), img);
            if (!err.empty()) throw MathError("Storage ERROR: " + err);
            const std::string bmp = files::join(files_.folder, files::stem(selectedName()) + files::BmpExtension);
            check(files::writeAtomic(*fs, bmp, [&](files::Writer& w) { return pict::writeBmp(img, w); }));
            ctx.toast("Esportato " + baseName(bmp));
        }
        return;
    default: return;
    }
}

void GraphApp::drawFiles(Context& ctx, gfx::Canvas& c) {
    (void)ctx;
    c.rect(0, ui::ContentTop, ui::ScreenW, 20, SelectSoft, true);
    const char* title = files_.purpose == Browse::Projects ? "Progetti GRAPH (.pcgraph)" : files_.purpose == Browse::Background ? "Sfondo (.pcpict)" : "Immagini PICT (.pcpict)";
    ui::text(c, 6, ui::ContentTop + 2, title, Ink);
    const int top = ui::ContentTop + 22, rowH = 20;
    const int visible = (ui::ContentBottom - top - 20) / rowH;
    if (files_.selected < files_.first) files_.first = files_.selected;
    if (files_.selected >= files_.first + visible) files_.first = files_.selected - visible + 1;
    for (int i = 0; i < visible && files_.first + i < static_cast<int>(files_.entries.size()); ++i) {
        const auto& e = files_.entries[static_cast<std::size_t>(files_.first + i)];
        char size[24];
        std::snprintf(size, sizeof(size), "%u B", static_cast<unsigned>(e.size));
        const bool current = files_.purpose == Browse::Projects && files::stem(e.name) == ctx.ws.graph.project;
        ui::listRow(c, top + i * rowH, rowH, (std::string(current ? "\xE2\x96\xB6 " : "  ") + e.name).c_str(), size, files_.first + i == files_.selected);
    }
    ui::scrollBar(c, ui::ScreenW - 4, top, visible * rowH, static_cast<int>(files_.entries.size()), visible, files_.first);
    c.hLine(0, ui::ContentBottom - 19, ui::ScreenW, Faint);
    ui::textFit(c, 6, ui::ContentBottom - 16, files_.status, ui::ScreenW - 12, files_.entries.empty() ? Error : Muted, font::Face::Small);
    const bool any = !files_.entries.empty();
    const char* keys[6] = {any ? "OPEN" : nullptr, any ? "DELETE" : nullptr, any ? "RENAME" : nullptr,
                           any && files_.purpose != Browse::Projects ? "BMP" : nullptr, nullptr, nullptr};
    ui::softKeys(c, keys);
}

// ---------------------------------------------------------------- graph memories
void GraphApp::storeGraphMemory(Context& ctx, int n) {
    GraphState& g = ctx.ws.graph;
    std::string text = graphfile::encodeMemory(ctx.ws.functions, ctx.ws.view);
    const std::size_t others = graphMemoryBytes(g) - g.gmem[static_cast<std::size_t>(n - 1)].size();
    if (others + text.size() > MaxGraphMemoryBytes) throw MathError("Memory ERROR: memorie grafiche piene");
    g.gmem[static_cast<std::size_t>(n - 1)] = std::move(text);
    ctx.markDirty();
    ctx.toast("G-Mem" + std::to_string(n) + " memorizzata");
}
void GraphApp::recallGraphMemory(Context& ctx, int n) {
    const std::string& text = ctx.ws.graph.gmem[static_cast<std::size_t>(n - 1)];
    if (text.empty()) throw MathError("Memory ERROR: G-Mem" + std::to_string(n) + " vuota");
    std::array<GraphFunction, MaxGraphFunctions> fns{};
    ViewWindow view;
    const std::string err = graphfile::decodeMemory(text, fns, view);
    if (!err.empty()) throw MathError("Memory ERROR: G-Mem" + std::to_string(n) + " danneggiata");
    // Recalling replaces the whole relation list (manual 5-20).
    ctx.ws.functions = fns;
    ctx.ws.view = view;
    haveOriginal_ = false; zoomStack_.clear();
    changed(ctx);
    ctx.toast("G-Mem" + std::to_string(n) + " richiamata");
}

// ---------------------------------------------------------------- pictures
void GraphApp::storePicture(Context& ctx, const std::string& path) {
    (void)fileSystem(ctx, true);                    // report a missing card before drawing
    finishRendering(ctx);
    capturePath_ = path;
    captureBmp_ = false;
    capturePending_ = true;                          // captured by the next drawPlot(), written by tick()
    screen_ = Screen::Plot;
    if (mode_ != Mode::View && mode_ != Mode::Trace) mode_ = Mode::View;
}

void GraphApp::writeCapture(Context& ctx) {
    captureReady_ = false;
    pict::Image img = std::move(capture_);
    capture_ = pict::Image{};
    files::FileSystem* fs = fileSystem(ctx, true);
    check(fs->makeDirs(files::PictFolder));
    if (captureBmp_) {
        check(files::writeAtomic(*fs, capturePath_, [&](files::Writer& w) { return pict::writeBmp(img, w); }));
    } else {
        const auto data = pict::serialize(img);
        check(files::writeAtomic(*fs, capturePath_, data.data(), data.size()));
    }
    ctx.toast("Salvato " + baseName(capturePath_));
}

void GraphApp::recallPicture(Context& ctx, const std::string& path, bool asBackground) {
    files::FileSystem* fs = fileSystem(ctx, true);
    std::vector<std::uint8_t> data;
    const files::Status st = files::readAll(*fs, path, data, pict::maxEncodedBytes() + 256);
    if (st == files::Status::TooLarge) throw MathError("Memory ERROR: immagine troppo grande");
    check(st);
    pict::Image img;
    const std::string err = pict::parse(data.data(), data.size(), img);
    if (!err.empty()) throw MathError("Storage ERROR: " + baseName(path) + " " + err);
    if (asBackground) {
        GraphSettings& g = ctx.ws.graph.settings;
        background_ = std::move(img);
        backgroundPath_ = path;
        // Pict01..Pict20 are the picture memories; other files are referenced by path.
        const std::string base = baseName(path);
        int n = 0;
        if (base.size() == 13 && base.compare(0, 4, "PICT") == 0 && std::sscanf(base.c_str() + 4, "%2d", &n) == 1 && n >= 1 && n <= pict::PictMemories) {
            g.background = static_cast<std::uint8_t>(n); g.backgroundFile.clear();
        } else { g.background = BackgroundFile; g.backgroundFile = path; }
        g.fade = background_.fade;
        ctx.markDirty(); ctx.ws.graph.projectModified = true;
        confirm(Confirm::BgvLoad, "Caricare la V-Window dello sfondo?");
        return;
    }
    overlay_ = std::move(img);
    overlayPath_ = path;
    if (ctx.ws.graph.overlay != path) { ctx.ws.graph.overlay = path; ctx.markDirty(); ctx.ws.graph.projectModified = true; }
}

void GraphApp::loadBackground(Context& ctx) {
    const GraphSettings& g = ctx.ws.graph.settings;
    std::string path;
    if (g.background >= 1 && g.background <= pict::PictMemories) path = files::join(files::PictFolder, pict::memoryFileName(g.background));
    else if (g.background == BackgroundFile) path = g.backgroundFile;
    if (path.empty()) { background_ = pict::Image{}; backgroundPath_.clear(); return; }
    if (path == backgroundPath_ && !background_.empty()) { background_.fade = g.fade; return; }
    background_ = pict::Image{}; backgroundPath_.clear();
    files::FileSystem* fs = fileSystem(ctx, false);
    if (!fs) throw MathError("Storage ERROR: sfondo non disponibile (microSD)");
    std::vector<std::uint8_t> data;
    check(files::readAll(*fs, path, data, pict::maxEncodedBytes() + 256));
    pict::Image img;
    const std::string err = pict::parse(data.data(), data.size(), img);
    if (!err.empty()) throw MathError("Storage ERROR: sfondo " + err);
    background_ = std::move(img);
    background_.fade = g.fade;
    backgroundPath_ = path;
}

// BGV-WIN SAVE / SAVE•AS: the background image with the current V-Window (manual 5-11).
void GraphApp::bgvSave(Context& ctx, const std::string& path) {
    if (background_.empty()) throw MathError("Argument ERROR: nessuna immagine di sfondo");
    if (path.empty()) throw MathError("Argument ERROR: sfondo senza file");
    files::FileSystem* fs = fileSystem(ctx, true);
    pict::Image img = background_;
    img.view = ctx.ws.view;
    img.fade = ctx.ws.graph.settings.fade;
    const auto data = pict::serialize(img);
    check(fs->makeDirs(files::PictFolder));
    check(files::writeAtomic(*fs, path, data.data(), data.size()));
    if (path != backgroundPath_) {
        background_ = std::move(img);
        backgroundPath_ = path;
        ctx.ws.graph.settings.background = BackgroundFile;
        ctx.ws.graph.settings.backgroundFile = path;
        ctx.markDirty(); ctx.ws.graph.projectModified = true;
    } else {
        background_.view = ctx.ws.view;
    }
    ctx.toast("Sfondo aggiornato: " + baseName(path));
}
} // namespace fx::app
