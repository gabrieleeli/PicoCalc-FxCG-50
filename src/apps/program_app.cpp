// PROGRAM (v1.17.0, modalita' Base; solo Pimoroni Pico Plus 2 W).
// Funzioni minime della modalita' (capitolo 8 della fx-CG50 per l'elenco, i
// tasti e la gestione degli errori; il linguaggio e' quello scelto
// dall'utente nella v1.15.0: sottoinsiemi interpretati di C, C++ e C#):
// elenco dei programmi .pcprog con NEW, EDIT, DEL e RUN; editor di testo con
// evidenziazione minima; esecuzione a passi con l'interprete della v1.15.0
// (output sul display, input da tastiera, comandi grafici disegnati da Graph,
// variabili A-Z e liste); AC = Break; errori con riga e colonna e cursore
// sull'errore.
#include "apps.hpp"
#include "limits.hpp"
#include "line_editor.hpp"
#include "program_mode.hpp"
#include "text_editor.hpp"
#include <algorithm>
#include <cstdio>

namespace fx::app {
using namespace ui::theme;
namespace {
enum Act : int { ActRun = 0, ActEdit, ActNew, ActDel, ActLang, ActSave, ActGraph, ActOk };
const FKeyPage kListPage{{{"EXE", ActRun}, {"EDIT", ActEdit}, {"NEW", ActNew}, {"DEL", ActDel}, {"LANG", ActLang}, {}}};
const FKeyMenu kListMenu{&kListPage, 1, nullptr};
const FKeyPage kEditPage{{{"RUN", ActRun}, {"SAVE", ActSave}, {"LANG", ActLang}, {}, {}, {}}};
const FKeyMenu kEditMenu{&kEditPage, 1, nullptr};
const FKeyPage kRunPage{{{}, {"EDIT", ActEdit}, {}, {}, {}, {"GRAPH", ActGraph}}};
const FKeyMenu kRunMenu{&kRunPage, 1, nullptr};
enum Screen : int { List, Edit, Run };
constexpr int ConfirmDel = 1;
constexpr int EditorRows = 15;
std::uint16_t tokColor(progmode::Tok t) {
    switch (t) {
    case progmode::Tok::Keyword: return rgb888To565(0, 60, 200);
    case progmode::Tok::Comment: return rgb888To565(0, 130, 40);
    case progmode::Tok::String: return rgb888To565(190, 0, 0);
    case progmode::Tok::Number: return rgb888To565(160, 0, 160);
    case progmode::Tok::Preproc: return rgb888To565(140, 90, 0);
    default: return Ink;
    }
}
// Host del programma: lo stesso del nucleo (graph_commands, fx::calc, A-Z, liste) con la console della modalita'.
class ConsoleHost final : public prog::WorkspaceHost {
public:
    explicit ConsoleHost(Workspace& ws) : WorkspaceHost(ws) {}
    void clearText() override { output.clear(); }
};
} // namespace

struct ProgramApp::View {
    ModeShell shell;
    TypedFileMenu files{files::FileType::Program};
    std::vector<files::TypedEntry> entries;
    SelectList list;
    Screen screen{List};
    TextEditor editor;
    std::string file;
    prog::Dialect dialect{prog::Dialect::Cpp};
    bool modified{false};
    std::size_t first{0};
    int scroll{0};
    std::unique_ptr<ConsoleHost> host;
    std::unique_ptr<prog::Machine> machine;
    step::Runner runner;
    LineEditor input{200};
    bool finished{false};
    std::string status;
    int errLine{0}, errColumn{0};
};

ProgramApp::ProgramApp() = default;
ProgramApp::~ProgramApp() = default;

namespace {
void refresh(Context& ctx, ProgramApp::View& v) {
    v.entries.clear();
    v.list.items.clear();
    files::FileSystem* fs = userFiles(ctx);
    if (!fs) return;
    files::TypedStore store(*fs);
    if (store.list(files::FileType::Program, v.entries) != files::Status::Ok) v.entries.clear();
    for (const auto& e : v.entries) v.list.items.push_back(e.file);
    v.list.clampSelection();
    // L'ultimo programma aperto resta selezionato.
    for (std::size_t i = 0; i < v.entries.size(); ++i) if (v.entries[i].file == ctx.ws.program.lastFile) v.list.selected = static_cast<int>(i);
}
std::string selected(ProgramApp::View& v) {
    if (v.entries.empty()) throw MathError("ERROR: nessun programma (F3 NEW)");
    return v.entries[static_cast<std::size_t>(std::clamp(v.list.selected, 0, static_cast<int>(v.entries.size()) - 1))].file;
}
void openFile(Context& ctx, ProgramApp::View& v, const std::string& file) {
    const auto data = v.files.load(ctx, file);
    const std::string text(data.begin(), data.end());
    const prog::Dialect fallback = file.size() > 7 && file.compare(file.size() - 7, 7, ".pcprog") == 0 ? prog::Dialect::Cpp : prog::dialectOf(file);
    const std::string source = progmode::decodeProgram(text, fallback, v.dialect);
    if (!v.editor.setText(source)) throw MathError("Memory ERROR: programma troppo grande");
    v.file = file;
    v.modified = false;
    ctx.ws.program.lastFile = file;
    ctx.markDirty();
}
void save(Context& ctx, ProgramApp::View& v) {
    if (v.file.empty()) return;
    const bool pc = v.file.size() > 7 && v.file.compare(v.file.size() - 7, 7, ".pcprog") == 0;
    const std::string text = pc ? progmode::encodeProgram(v.dialect, v.editor.text()) : v.editor.text();
    v.files.save(ctx, v.file, std::vector<std::uint8_t>(text.begin(), text.end()));
    v.modified = false;
}
void edit(Context& ctx, ProgramApp::View& v) {
    v.editor.setWrap(false);
    v.editor.setVisibleRows(EditorRows);
    v.editor.moveTo(ctx.ws.program.cursorLine, ctx.ws.program.cursorColumn);
    v.screen = Edit;
    v.shell.fkeys.set(&kEditMenu);
}
void startRun(Context& ctx, ProgramApp::View& v) {
    v.runner.reset();
    v.machine.reset();
    v.errLine = v.errColumn = 0;
    std::shared_ptr<const prog::Program> program;
    try {
        program = prog::compile(v.editor.text(), v.dialect);
    } catch (const prog::Error& e) {
        // Errore di compilazione: il cursore va sull'errore (manuale 8-6).
        v.errLine = e.line; v.errColumn = e.column;
        if (e.line > 0) v.editor.moveTo(static_cast<std::size_t>(e.line - 1), static_cast<std::size_t>(std::max(0, e.column - 1)));
        edit(ctx, v);
        throw MathError(std::string(e.what()) + " (riga " + std::to_string(e.line) + ", colonna " + std::to_string(e.column) + ")");
    }
    v.host = std::make_unique<ConsoleHost>(ctx.ws);
    const std::size_t cells = limits::ram().graphLayerCache ? prog::DefaultMemoryCells : prog::DefaultMemoryCells / 10;
    v.machine = std::make_unique<prog::Machine>(program, *v.host, cells);
    v.runner.start(*v.machine, step::Runner::Config{2000, 25});
    v.finished = false;
    v.status = std::string("Running (") + prog::dialectName(v.dialect) + ")";
    v.input.clear();
    v.screen = Run;
    v.shell.fkeys.set(&kRunMenu);
    ctx.markDirty();
}
} // namespace

void ProgramApp::enter(Context& ctx) {
    v_ = std::make_unique<View>();
    v_->dialect = static_cast<prog::Dialect>(ctx.ws.program.newDialect);
    v_->shell.fkeys.set(&kListMenu);
    refresh(ctx, *v_);
}
void ProgramApp::leave(Context& ctx) {
    if (v_ && v_->screen == Edit && v_->modified) {
        try { save(ctx, *v_); } catch (...) {}      // EXIT/MENU dall'editor: il programma resta salvato
    }
    if (v_ && v_->screen == Edit) { ctx.ws.program.cursorLine = static_cast<std::uint32_t>(v_->editor.cursorLine()); ctx.ws.program.cursorColumn = static_cast<std::uint32_t>(v_->editor.cursorColumn()); }
    v_.reset();
}
std::string ProgramApp::screenName() const {
    if (!v_) return "closed";
    if (v_->screen == Run) return v_->finished ? "done" : v_->machine && v_->machine->state() == prog::Machine::State::WaitingInput ? "input" : "run";
    return v_->screen == Edit ? "edit" : "list";
}
bool ProgramApp::tick(Context& ctx) {
    if (!v_ || v_->screen != Run || !v_->runner.running()) return false;
    View& v = *v_;
    try {
        v.runner.tick([&ctx] { return ctx.now(); });
    } catch (const prog::Error& e) {
        v.finished = true;
        v.errLine = e.line; v.errColumn = e.column;
        v.status = "Errore alla riga " + std::to_string(e.line);
        throw MathError(std::string(e.what()) + " (riga " + std::to_string(e.line) + ", colonna " + std::to_string(e.column) + ")");
    }
    if (!v.runner.running()) {
        v.finished = true;
        v.status = v.runner.state() == step::State::Break ? "Break" : "Done";
    }
    return true;
}

void ProgramApp::key(Context& ctx, const KeyEvent& ev) {
    if (!v_) enter(ctx);
    View& v = *v_;
    if (v.files.active()) {
        if (v.files.key(ev) != 1) return;
        // NEW: il nome scelto con SAVE·AS; il programma d'esempio nel dialetto scelto.
        v.file = v.files.file();
        if (!v.editor.setText(progmode::templateFor(v.dialect))) throw MathError("Memory ERROR");
        save(ctx, v);
        ctx.ws.program.lastFile = v.file;
        ctx.ws.program.cursorLine = ctx.ws.program.cursorColumn = 0;
        ctx.markDirty();
        refresh(ctx, v);
        edit(ctx, v);
        return;
    }
    const bool modal = v.shell.modalKey(ev,
        [&](int tag, bool yes) {
            if (tag != ConfirmDel || !yes) return;
            v.files.remove(ctx, selected(v));
            refresh(ctx, v);
        },
        [](int) {}, [](int, int) {});
    if (modal) return;
    switch (v.screen) {
    case List: {
        const int a = v.shell.fkeys.key(ev, ctx.shift());
        switch (a) {
        case ActRun: case ActEdit: {
            openFile(ctx, v, selected(v));
            if (a == ActRun) { ctx.ws.program.cursorLine = ctx.ws.program.cursorColumn = 0; startRun(ctx, v); }
            else edit(ctx, v);
            return;
        }
        case ActNew: v.files.openSaveAs(ctx, ""); return;
        case ActDel: v.shell.confirm.open("Eliminare " + selected(v) + "?", ConfirmDel); return;
        case ActLang:
            ctx.ws.program.newDialect = static_cast<std::uint8_t>((ctx.ws.program.newDialect + 1) % 3);
            v.dialect = static_cast<prog::Dialect>(ctx.ws.program.newDialect);
            ctx.markDirty();
            return;
        default: break;
        }
        if (ev.is(Key::Exe)) { openFile(ctx, v, selected(v)); startRun(ctx, v); return; }
        if (ev.is(Key::Exit)) { ctx.home(); return; }
        (void)v.list.key(ev);
        return;
    }
    case Edit: {
        const int a = v.shell.fkeys.key(ev, false);
        if (a == ActRun) { save(ctx, v); startRun(ctx, v); return; }
        if (a == ActSave) { save(ctx, v); ctx.toast("Salvato " + v.file); return; }
        if (a == ActLang) { v.dialect = static_cast<prog::Dialect>((static_cast<int>(v.dialect) + 1) % 3); v.modified = true; return; }
        if (ev.is(Key::Exit)) {
            if (v.modified) save(ctx, v);
            ctx.ws.program.cursorLine = static_cast<std::uint32_t>(v.editor.cursorLine());
            ctx.ws.program.cursorColumn = static_cast<std::uint32_t>(v.editor.cursorColumn());
            ctx.markDirty();
            refresh(ctx, v);
            v.screen = List;
            v.shell.fkeys.set(&kListMenu);
            return;
        }
        if (ev.key == Key::Char && ev.ch == '\t') { (void)v.editor.insert("    "); v.modified = true; return; }
        if (v.editor.key(ev, &ctx.textClipboard)) v.modified = true;
        if (v.editor.full()) ctx.toast("Memory ERROR: programma al limite");
        return;
    }
    case Run: {
        if (v.runner.running() && ev.is(Key::AC)) { v.runner.interrupt(); v.finished = true; v.status = "Break"; throw MathError("Break: programma interrotto"); }
        const bool waiting = v.machine && v.machine->state() == prog::Machine::State::WaitingInput;
        if (waiting && !v.finished) {
            if (ev.is(Key::Exe)) {
                v.host->output += v.input.text() + "\n";
                v.host->input.push_back(v.input.text());
                v.input.clear();
                return;
            }
            (void)v.input.key(ev);
            return;
        }
        const int a = v.shell.fkeys.key(ev, false);
        if (a == ActEdit || (v.finished && ev.is(Key::Exe) && v.errLine > 0)) {
            if (v.errLine > 0) v.editor.moveTo(static_cast<std::size_t>(v.errLine - 1), static_cast<std::size_t>(std::max(0, v.errColumn - 1)));
            v.runner.reset();
            edit(ctx, v);
            return;
        }
        if (a == ActGraph && v.host && !v.host->commands.empty()) { ctx.open(AppId::Graph); return; }
        if (v.finished && (ev.is(Key::Exit) || ev.is(Key::Exe))) {
            v.runner.reset();
            v.machine.reset();
            v.screen = List;
            v.shell.fkeys.set(&kListMenu);
            refresh(ctx, v);
        }
        return;
    }
    }
}

void ProgramApp::draw(Context& ctx, gfx::Canvas& c) {
    if (!v_) enter(ctx);
    View& v = *v_;
    ui::clear(c);
    ctx.drawStatus(c, "PROGRAM");
    const int top = ui::ContentTop;
    switch (v.screen) {
    case List: {
        drawModeHeader(c, std::string("Programmi  (NEW: ") + prog::dialectName(static_cast<prog::Dialect>(ctx.ws.program.newDialect)) + ")");
        if (v.entries.empty()) ui::textCenter(c, ui::ScreenW / 2, 120, userFiles(ctx) ? "Nessun programma: F3 NEW" : "Archivio dei file non disponibile", Muted);
        else v.list.draw(c, gfx::Rect{0, top + 18, ui::ScreenW, ui::ContentBottom - top - 18}, 20);
        break;
    }
    case Edit: {
        drawModeHeader(c, v.file + "  [" + prog::dialectName(v.dialect) + "]" + (v.modified ? " *" : "") + "  L" + std::to_string(v.editor.cursorLine() + 1));
        const std::size_t cl = v.editor.cursorLine();
        if (cl < v.first) v.first = cl;
        if (cl >= v.first + EditorRows) v.first = cl - EditorRows + 1;
        // scorrimento orizzontale: la colonna del cursore resta visibile
        const int charW = 8, cols = (ui::ScreenW - 8) / charW;
        const int cc = static_cast<int>(v.editor.cursorColumn());
        if (cc < v.scroll) v.scroll = cc;
        if (cc >= v.scroll + cols) v.scroll = cc - cols + 1;
        bool inComment = false;
        for (std::size_t l = 0; l < v.first && l < v.editor.lineCount(); ++l) (void)progmode::highlight(v.editor.line(l), v.dialect, inComment);
        for (int r = 0; r < EditorRows && v.first + static_cast<std::size_t>(r) < v.editor.lineCount(); ++r) {
            const std::size_t li = v.first + static_cast<std::size_t>(r);
            const std::string& line = v.editor.line(li);
            const auto cls = progmode::highlight(line, v.dialect, inComment);
            const int y = top + 18 + r * 16;
            if (v.errLine > 0 && li + 1 == static_cast<std::size_t>(v.errLine)) c.rect(0, y, ui::ScreenW, 16, ErrorHighlight, true);
            for (std::size_t k = static_cast<std::size_t>(v.scroll); k < line.size() && static_cast<int>(k) < v.scroll + cols; ++k) {
                const char s[2] = {line[k], 0};
                font::draw(c, font::Face::Medium, 4 + static_cast<int>(k - static_cast<std::size_t>(v.scroll)) * charW, y, s, tokColor(cls[k]));
            }
            if (li == cl) c.vLine(4 + (cc - v.scroll) * charW, y + 1, 14, Cursor);
        }
        break;
    }
    case Run: {
        drawModeHeader(c, v.file + "  " + v.status);
        const std::string& out = v.host ? v.host->output : std::string();
        std::vector<std::string> lines(1);
        for (char ch : out) { if (ch == '\n') lines.emplace_back(); else lines.back() += ch; }
        const bool waiting = !v.finished && v.machine && v.machine->state() == prog::Machine::State::WaitingInput;
        const int rows = 15 - (waiting ? 1 : 0);
        const std::size_t from = lines.size() > static_cast<std::size_t>(rows) ? lines.size() - static_cast<std::size_t>(rows) : 0;
        for (std::size_t i = from; i < lines.size(); ++i) ui::textFit(c, 4, top + 18 + static_cast<int>(i - from) * 16, lines[i], ui::ScreenW - 8, Ink);
        if (waiting) {
            const int y = top + 18 + rows * 16;
            ui::text(c, 4, y, "?", Accent);
            v.input.draw(c, 16, y, ui::ScreenW - 20, Ink);
        }
        if (v.runner.running() && !waiting) ui::textRight(c, ui::ScreenW - 4, ui::ContentBottom - 16, "AC: Break", Warn, font::Face::Small);
        break;
    }
    }
    v.shell.fkeys.draw(c, ctx.shift());
    v.shell.drawModals(c);
    v.files.draw(c);
}
} // namespace fx::app
