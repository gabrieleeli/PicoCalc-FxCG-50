// PYTHON (v1.17.0, modalita' Base; solo Pimoroni Pico Plus 2 W).
// Funzioni minime del capitolo 17 del manuale fx-CG50 (descrizione del
// comportamento, nessun codice copiato) con MicroPython v1.24.1 (porta embed,
// licenza MIT): elenco degli script .py, editor, RUN, SHELL (REPL) e output sul
// display; moduli math e casioplot (disegno minimo); heap separato (PSRAM) e
// liberato all'uscita; AC = KeyboardInterrupt; input() dalla tastiera.
#include "apps.hpp"
#include "line_editor.hpp"
#include "mem_pools.hpp"
#include "python_port.hpp"
#include "text_editor.hpp"
#include <algorithm>
#include <cstdio>

namespace fx::app {
using namespace ui::theme;
namespace {
enum Act : int { ActRun = 0, ActOpen, ActNew, ActDel, ActShell, ActSave };
const FKeyPage kListPage{{{"RUN", ActRun}, {"OPEN", ActOpen}, {"NEW", ActNew}, {"DEL", ActDel}, {"SHELL", ActShell}, {}}};
const FKeyMenu kListMenu{&kListPage, 1, nullptr};
const FKeyPage kEditPage{{{"RUN", ActRun}, {"SAVE", ActSave}, {}, {}, {}, {}}};
const FKeyMenu kEditMenu{&kEditPage, 1, nullptr};
const FKeyPage kNonePage{{{}, {}, {}, {}, {}, {}}};
const FKeyMenu kNoneMenu{&kNonePage, 1, nullptr};
enum Screen : int { List, Edit, Shell, Plot };
constexpr int ConfirmDel = 1;
constexpr int PlotW = ui::ScreenW, PlotH = ui::ContentBottom - ui::ContentTop;

// casioplot: disegno in un buffer fuori schermo (nella PSRAM se c'e'),
// mostrato da show_screen() e alla fine dello script. Nessuna eccezione verso
// MicroPython: le chiamate arrivano da codice C.
class PlotCanvas final : public py::Canvas {
public:
    ~PlotCanvas() override { mem::free(px_); }
    bool used() const noexcept { return used_; }
    const std::uint16_t* pixels() const noexcept { return px_; }
    void setPixel(int x, int y, std::uint8_t r, std::uint8_t g, std::uint8_t b) override {
        if (!ensure() || x < 0 || y < 0 || x >= PlotW || y >= PlotH) return;
        px_[y * PlotW + x] = gfx::rgb888To565(r, g, b);
    }
    bool getPixel(int x, int y, std::uint8_t& r, std::uint8_t& g, std::uint8_t& b) override {
        if (!ensure() || x < 0 || y < 0 || x >= PlotW || y >= PlotH) return false;
        const auto c = gfx::rgb565To888(px_[y * PlotW + x]);
        r = c.r; g = c.g; b = c.b;
        return true;
    }
    void drawString(int x, int y, const std::string& s, std::uint8_t r, std::uint8_t g, std::uint8_t b, bool large) override {
        if (!ensure()) return;
        try {
            gfx::Canvas view(px_, PlotW, PlotH);
            font::draw(view, large ? font::Face::Medium : font::Face::Small, x, y, s.c_str(), gfx::rgb888To565(r, g, b));
        } catch (...) {}
    }
    void clear() override { if (ensure()) std::fill(px_, px_ + PlotW * PlotH, 0xFFFF); }
    void show() override { shown_ = true; }
    bool shown() const noexcept { return shown_; }
    void clearShown() noexcept { shown_ = false; }
private:
    bool ensure() noexcept {
        if (px_) return true;
        px_ = static_cast<std::uint16_t*>(mem::tryAlloc(mem::Policy::PREFER_PSRAM, PlotW * PlotH * 2, 4));
        if (!px_) return false;
        std::fill(px_, px_ + PlotW * PlotH, 0xFFFF);
        used_ = true;
        return true;
    }
    std::uint16_t* px_{nullptr};
    bool used_{false}, shown_{false};
};
} // namespace

struct PythonApp::View {
    ModeShell shell;
    TypedFileMenu files{files::FileType::Python};
    std::vector<files::TypedEntry> entries;
    SelectList list;
    Screen screen{List};
    TextEditor editor;
    std::string file;
    bool modified{false};
    PlotCanvas plot;
    std::unique_ptr<py::Session> session;
    step::Runner runner;
    LineEditor input{240};
    std::string pending;                         // righe di un blocco della SHELL (dopo ':')
    int historyAt{-1};
    bool script{false};
    bool lastWasShell{false};                    // l'ultimo lavoro e' una riga della SHELL
};

PythonApp::PythonApp() = default;
PythonApp::~PythonApp() = default;

namespace {
void refresh(Context& ctx, PythonApp::View& v) {
    v.entries.clear();
    v.list.items.clear();
    if (files::FileSystem* fs = userFiles(ctx)) {
        files::TypedStore store(*fs);
        if (store.list(files::FileType::Python, v.entries) != files::Status::Ok) v.entries.clear();
    }
    for (const auto& e : v.entries) v.list.items.push_back(e.file);
    v.list.clampSelection();
    for (std::size_t i = 0; i < v.entries.size(); ++i) if (v.entries[i].file == ctx.ws.python.lastScript) v.list.selected = static_cast<int>(i);
}
std::string selected(PythonApp::View& v) {
    if (v.entries.empty()) throw MathError("ERROR: nessuno script (F3 NEW)");
    return v.entries[static_cast<std::size_t>(std::clamp(v.list.selected, 0, static_cast<int>(v.entries.size()) - 1))].file;
}
void ensureSession(Context& ctx, PythonApp::View& v) {
    if (v.session) return;
    v.session = std::make_unique<py::Session>([&ctx] { return ctx.now(); }, &v.plot);   // std::bad_alloc -> Memory ERROR
}
void startRun(Context& ctx, PythonApp::View& v) {
    ensureSession(ctx, v);
    v.session->runScript(v.editor.text());
    v.lastWasShell = false;
    v.runner.reset();
    v.runner.start(*v.session, step::Runner::Config{64000, 25});
    v.script = true;
    v.screen = Shell;
    v.shell.fkeys.set(&kNoneMenu);
}
void save(Context& ctx, PythonApp::View& v) {
    if (v.file.empty()) return;
    const std::string t = v.editor.text();
    v.files.save(ctx, v.file, std::vector<std::uint8_t>(t.begin(), t.end()));
    v.modified = false;
}
} // namespace

void PythonApp::enter(Context& ctx) {
    v_ = std::make_unique<View>();
    v_->editor.setWrap(false);
    v_->shell.fkeys.set(&kListMenu);
    refresh(ctx, *v_);
}
void PythonApp::leave(Context& ctx) {
    if (v_ && v_->screen == Edit && v_->modified) { try { save(ctx, *v_); } catch (...) {} }
    v_.reset();                                   // la sessione (heap di MicroPython) si libera qui
}
std::string PythonApp::screenName() const {
    if (!v_) return "closed";
    if (v_->screen == Shell) return v_->session && v_->session->waitingInput() ? "input" : v_->runner.running() ? "run" : "shell";
    static const char* const n[] = {"list", "edit", "shell", "plot"};
    return n[v_->screen];
}
bool PythonApp::tick(Context& ctx) {
    (void)ctx;
    if (!v_ || !v_->runner.running()) return false;
    View& v = *v_;
    v.runner.tick([&ctx] { return ctx.now(); });
    // show_screen() (anche dalla SHELL) o la fine di uno script che ha disegnato: il grafico.
    if (v.plot.shown() || (!v.runner.running() && v.script && v.plot.used())) {
        v.plot.clearShown();
        v.screen = Plot;
        v.shell.fkeys.set(&kNoneMenu);
    }
    if (!v.runner.running()) v.script = false;
    return true;
}

void PythonApp::key(Context& ctx, const KeyEvent& ev) {
    if (!v_) enter(ctx);
    View& v = *v_;
    if (v.files.active()) {
        if (v.files.key(ev) != 1) return;
        v.file = v.files.file();
        (void)v.editor.setText("from math import *\nprint(\"Ciao\")\n");
        save(ctx, v);
        ctx.ws.python.lastScript = v.file;
        ctx.markDirty();
        refresh(ctx, v);
        v.screen = Edit;
        v.shell.fkeys.set(&kEditMenu);
        return;
    }
    const bool modal = v.shell.modalKey(ev,
        [&](int tag, bool yes) { if (tag == ConfirmDel && yes) { v.files.remove(ctx, selected(v)); refresh(ctx, v); } },
        [](int) {}, [](int, int) {});
    if (modal) return;
    switch (v.screen) {
    case List: {
        const int a = v.shell.fkeys.key(ev, ctx.shift());
        if (a == ActRun || a == ActOpen || ev.is(Key::Exe)) {
            const std::string f = selected(v);
            const auto data = v.files.load(ctx, f);
            if (!v.editor.setText(std::string(data.begin(), data.end()))) throw MathError("Memory ERROR: script troppo grande");
            v.file = f;
            v.modified = false;
            ctx.ws.python.lastScript = f;
            ctx.markDirty();
            if (a == ActOpen) { v.screen = Edit; v.shell.fkeys.set(&kEditMenu); }
            else startRun(ctx, v);
            return;
        }
        if (a == ActNew) { v.files.openSaveAs(ctx, ""); return; }
        if (a == ActDel) { v.shell.confirm.open("Eliminare " + selected(v) + "?", ConfirmDel); return; }
        if (a == ActShell) { ensureSession(ctx, v); v.screen = Shell; v.shell.fkeys.set(&kNoneMenu); return; }
        if (ev.is(Key::Exit)) { ctx.home(); return; }
        (void)v.list.key(ev);
        return;
    }
    case Edit: {
        const int a = v.shell.fkeys.key(ev, false);
        if (a == ActRun) { save(ctx, v); startRun(ctx, v); return; }
        if (a == ActSave) { save(ctx, v); ctx.toast("Salvato " + v.file); return; }
        if (ev.is(Key::Exit)) { if (v.modified) save(ctx, v); refresh(ctx, v); v.screen = List; v.shell.fkeys.set(&kListMenu); return; }
        if (ev.key == Key::Char && ev.ch == '\t') { (void)v.editor.insert("    "); v.modified = true; return; }
        if (v.editor.key(ev, &ctx.textClipboard)) v.modified = true;
        return;
    }
    case Plot:
        if (ev.is(Key::AC) && v.runner.running()) { v.session->interrupt(); return; }
        // EXIT torna alla SHELL (o all'editor dello script eseguito)
        if (ev.is(Key::Exit) || ev.is(Key::Exe) || ev.is(Key::AC)) {
            const bool toEditor = !v.runner.running() && !v.file.empty() && v.editor.lineCount() > 0 && !v.lastWasShell;
            v.screen = toEditor ? Edit : Shell;
            v.shell.fkeys.set(toEditor ? &kEditMenu : &kNoneMenu);
        }
        return;
    case Shell: {
        if (ev.is(Key::AC) && v.runner.running()) { v.session->interrupt(); return; }   // KeyboardInterrupt
        if (v.runner.running() && !v.session->waitingInput()) return;
        if (ev.is(Key::Exe)) {
            const std::string line = v.input.text();
            v.input.clear();
            v.historyAt = -1;
            if (v.session->waitingInput()) { v.session->provideInput(line); return; }
            auto& h = ctx.ws.python.history;
            if (!line.empty() && (h.empty() || h.back() != line)) { h.push_back(line); if (h.size() > 20) h.erase(h.begin()); ctx.markDirty(); }
            const std::string text = v.pending.empty() ? line : v.pending + "\n" + line;
            v.session->write(v.pending.empty() ? ">>> " : "... ", 4);
            v.session->write(line.data(), line.size());
            v.session->write("\n", 1);
            if (!line.empty() && v.session->needsMoreInput(text)) { v.pending = text; return; }
            v.pending.clear();
            if (text.empty()) return;
            v.session->runLine(text);
            v.lastWasShell = true;
            v.runner.reset();
            v.runner.start(*v.session, step::Runner::Config{64000, 25});
            return;
        }
        if (ev.is(Key::Up) || ev.is(Key::Down)) {
            const auto& h = ctx.ws.python.history;
            if (h.empty()) return;
            if (v.historyAt < 0) v.historyAt = static_cast<int>(h.size());
            v.historyAt = std::clamp(v.historyAt + (ev.is(Key::Up) ? -1 : 1), 0, static_cast<int>(h.size()) - 1);
            v.input.set(h[static_cast<std::size_t>(v.historyAt)]);
            return;
        }
        if (ev.is(Key::Exit) && !v.session->waitingInput()) {
            v.pending.clear();
            if (v.plot.used()) { v.screen = Plot; return; }
            v.screen = v.file.empty() ? List : Edit;
            v.shell.fkeys.set(v.file.empty() ? &kListMenu : &kEditMenu);
            if (v.screen == List) refresh(ctx, v);
            return;
        }
        (void)v.input.key(ev);
        return;
    }
    }
}

void PythonApp::draw(Context& ctx, gfx::Canvas& c) {
    if (!v_) enter(ctx);
    View& v = *v_;
    ui::clear(c);
    ctx.drawStatus(c, "PYTHON");
    const int top = ui::ContentTop;
    switch (v.screen) {
    case List:
        drawModeHeader(c, "Script .py (MicroPython)");
        if (v.entries.empty()) ui::textCenter(c, ui::ScreenW / 2, 120, userFiles(ctx) ? "Nessuno script: F3 NEW, F5 SHELL" : "Archivio dei file non disponibile", Muted);
        else v.list.draw(c, gfx::Rect{0, top + 18, ui::ScreenW, ui::ContentBottom - top - 18}, 20);
        break;
    case Edit:
        drawModeHeader(c, v.file + (v.modified ? " *" : "") + "  L" + std::to_string(v.editor.cursorLine() + 1));
        v.editor.setVisibleRows(15);
        v.editor.draw(c, gfx::Rect{2, top + 18, ui::ScreenW - 4, ui::ContentBottom - top - 20}, Ink);
        break;
    case Plot:
        if (v.plot.pixels()) c.bitmap(0, top, PlotW, PlotH, v.plot.pixels());
        break;
    case Shell: {
        const std::string head = std::string("SHELL  heap ") + std::to_string((v.session ? v.session->heapBytes() : 0) / 1024) + " KB" +
                                 (v.runner.running() && !(v.session && v.session->waitingInput()) ? "  AC: KeyboardInterrupt" : "");
        drawModeHeader(c, head);
        const int rows = 14;
        if (v.session) {
            const auto& lines = v.session->console();
            const std::size_t from = lines.size() > static_cast<std::size_t>(rows) ? lines.size() - static_cast<std::size_t>(rows) : 0;
            for (std::size_t i = from; i < lines.size(); ++i) ui::textFit(c, 4, top + 18 + static_cast<int>(i - from) * 16, lines[i], ui::ScreenW - 8, Ink);
        }
        const int y = top + 18 + rows * 16;
        const bool idle = !v.runner.running() || (v.session && v.session->waitingInput());
        if (idle) {
            ui::text(c, 4, y, v.session && v.session->waitingInput() ? "?" : v.pending.empty() ? ">>>" : "...", Accent);
            v.input.draw(c, 36, y, ui::ScreenW - 40, Ink);
        }
        break;
    }
    }
    v.shell.fkeys.draw(c, ctx.shift());
    v.shell.drawModals(c);
    v.files.draw(c);
}
} // namespace fx::app
