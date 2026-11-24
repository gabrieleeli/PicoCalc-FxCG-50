// PROGRAM (v1.20.0, solo Pimoroni Pico Plus 2 W).
// Capitolo 8 della fx-CG50 con i linguaggi scelti dall'utente (C, C++, C#;
// Java dalla v1.21.0, Python nella sua modalita'): programmi interpretati dal
// sorgente, salvati come file standard sulla microSD in PICOCALC/CODE/<LING>
// (progetti = cartelle). Schermate:
//  * gestore dei file: cartelle, EXE/RUN, EDIT, NEW (file, progetto o
//    cartella dai modelli), DEL, DEL·A con conferma, SEARCH per nome,
//    RENAME, COPY, MOVE, ordinamento, password come sulla fx-CG50, LIB,
//    SET UP, DEBUG, migrazione dei programmi della v1.17.0;
//  * editor: numeri di riga, evidenziazione per linguaggio, rientro
//    automatico, parentesi corrispondenti, trova/sostituisci, copia/incolla,
//    annulla/ripeti, salto alla riga dell'errore, file grandi a blocchi;
//  * esecuzione a passi (AC = Break) con console, input, grafici di Graph;
//  * DEBUG a passi con variabili, pila delle chiamate e punti d'interruzione;
//  * LIB: librerie installate, INFO, INSTALL/UPDATE dal repository (Wi-Fi e
//    HTTPS) o da una cartella della SD, REMOVE, VERIFY.
// Errori con file, riga e colonna e cursore sull'errore (manuale 8-6).
#include "apps.hpp"
#include "code_store.hpp"
#include "lib_fetch.hpp"
#include "lib_manager.hpp"
#include "limits.hpp"
#include "line_editor.hpp"
#include "mem_pools.hpp"
#include "net.hpp"
#include "program_mode.hpp"
#include "text_editor.hpp"
#include <algorithm>
#include <cstdio>
#include <map>
#include <set>

namespace fx::app {
using namespace ui::theme;
namespace {
enum Act : int {
    ActRun = 0, ActEdit, ActNew, ActDel, ActDelAll, ActSearch, ActRename, ActCopy, ActMove, ActSort, ActPass, ActLib, ActSetup, ActDebug, ActMigrate,
    ActSave, ActFind, ActReplace, ActGoto, ActUndo, ActRedo, ActSaveAs, ActBreak, ActTop, ActBottom, ActErr, ActGraph,
    ActStep, ActOver, ActOut, ActCont, ActVars, ActStop,
    ActInfo, ActInstall, ActUpdate, ActRemove, ActVerify, ActLocal, ActIndex, ActOk,
};
// gestore dei file
const FKeyPage kFilePages[] = {
    {{{"EXE", ActRun}, {"EDIT", ActEdit}, {"NEW", ActNew}, {"DEL", ActDel}, {"DEL\xC2\xB7" "A", ActDelAll}, {}}},
    {{{"SRC", ActSearch}, {"REN", ActRename}, {"COPY", ActCopy}, {"MOVE", ActMove}, {"SORT", ActSort}, {}}},
    {{{"PASS", ActPass}, {"LIB", ActLib}, {"SETUP", ActSetup}, {"DEBUG", ActDebug}, {"MIGR", ActMigrate}, {}}},
};
const FKeyMenu kFileMenu{kFilePages, 3, nullptr};
const FKeyPage kEditPages[] = {
    {{{"RUN", ActRun}, {"SAVE", ActSave}, {"FIND", ActFind}, {"REPL", ActReplace}, {"JUMP", ActGoto}, {}}},
    {{{"UNDO", ActUndo}, {"REDO", ActRedo}, {"SAV\xC2\xB7" "AS", ActSaveAs}, {"BRK", ActBreak}, {"DEBUG", ActDebug}, {}}},
    {{{"TOP", ActTop}, {"BTM", ActBottom}, {"ERR", ActErr}, {}, {}, {}}},
};
const FKeyMenu kEditMenu{kEditPages, 3, nullptr};
const FKeyPage kRunPage{{{}, {"EDIT", ActEdit}, {}, {}, {}, {"GRAPH", ActGraph}}};
const FKeyMenu kRunMenu{&kRunPage, 1, nullptr};
const FKeyPage kDebugPage{{{"STEP", ActStep}, {"OVER", ActOver}, {"OUT", ActOut}, {"CONT", ActCont}, {"VARS", ActVars}, {"STOP", ActStop}}};
const FKeyMenu kDebugMenu{&kDebugPage, 1, nullptr};
const FKeyPage kVarsPage{{{}, {}, {}, {}, {}, {}}};
const FKeyMenu kVarsMenu{&kVarsPage, 1, nullptr};
const FKeyPage kLibPages[] = {
    {{{"INFO", ActInfo}, {"INST", ActInstall}, {"UPD", ActUpdate}, {"DEL", ActRemove}, {"VERIF", ActVerify}, {}}},
    {{{"LOCAL", ActLocal}, {"INDEX", ActIndex}, {}, {}, {}, {}}},
};
const FKeyMenu kLibMenu{kLibPages, 2, nullptr};
const FKeyPage kSetupPage{{{"OK", ActOk}, {}, {}, {}, {"MIGR", ActMigrate}, {}}};
const FKeyMenu kSetupMenu{&kSetupPage, 1, nullptr};
const FKeyPage kInfoPage{{{}, {}, {}, {}, {}, {}}};
const FKeyMenu kInfoMenu{&kInfoPage, 1, nullptr};

enum Screen : int { Files, Edit, Run, Vars, Lib, Info, Setup };
// domande e nomi (FileDialog) e menu
enum Ask : int {
    AskNone = 0, AskNewName, AskRename, AskSearch, AskPick, AskPassOld, AskPassNew, AskEditPass, AskFind, AskReplaceWhat, AskReplaceWith,
    AskGoto, AskSaveAs, AskCopyTo, AskMoveTo, AskLibChoose, AskConfirmPlan,
};
enum Confirm : int { ConfirmDel = 1, ConfirmDelAll, ConfirmMigrate, ConfirmMigrateRemove, ConfirmLibDel, ConfirmPlan, ConfirmLeaveBlock };
enum MenuTag : int { MenuNewKind = 1, MenuNewLang };
constexpr int EditorRows = 15;
constexpr int ListRowH = 18;
constexpr std::size_t BlockBytes = 128u * 1024u;          // blocco di un file grande nell'editor
constexpr const char* ImportFolder = "PICOCALC/LIB/IMPORT";   // repository copiato dal PC (index.json e cartelle)

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
std::string shortPath(const std::string& p) { return p.rfind("PICOCALC/", 0) == 0 ? p.substr(9) : p; }
std::string baseName(const std::string& p) { const std::size_t s = p.rfind('/'); return s == std::string::npos ? p : p.substr(s + 1); }
std::string parentOf(const std::string& p) { const std::size_t s = p.rfind('/'); return s == std::string::npos ? std::string() : p.substr(0, s); }
bool langOfPath(const std::string& path, code::Lang& out) {
    // PICOCALC/CODE/<LING>/...
    const std::string root = std::string(code::CodeRoot) + "/";
    if (path.size() <= root.size() || path.compare(0, root.size(), root) != 0) return false;
    std::string seg = path.substr(root.size());
    seg = seg.substr(0, seg.find('/'));
    for (char& c : seg) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    for (std::size_t i = 0; i < code::LangCount; ++i) if (seg == code::folderOf(static_cast<code::Lang>(i))) { out = static_cast<code::Lang>(i); return true; }
    return false;
}
bool isLangRoot(const std::string& dir) {
    code::Lang l;
    return langOfPath(dir, l) && dir.size() == code::rootOf(l).size();
}
// Annulla/ripeti: istantanee del testo (con un limite di memoria).
struct History {
    struct Snap { std::string text; std::size_t line{0}, col{0}; };
    std::vector<Snap> undo, redo;
    std::size_t bytes{0};
    bool typing{false};
    static constexpr std::size_t MaxBytes = 1024u * 1024u, MaxSnaps = 64;
    void clear() { undo.clear(); redo.clear(); bytes = 0; typing = false; }
    void push(const TextEditor& e, bool typingKey) {
        if (typingKey && typing) return;                  // una parola digitata = un passo
        typing = typingKey;
        Snap s{e.text(), e.cursorLine(), e.cursorColumn()};
        bytes += s.text.size();
        undo.push_back(std::move(s));
        while (!undo.empty() && (bytes > MaxBytes || undo.size() > MaxSnaps)) { bytes -= undo.front().text.size(); undo.erase(undo.begin()); }
        redo.clear();
    }
    bool apply(TextEditor& e, std::vector<Snap>& from, std::vector<Snap>& to) {
        if (from.empty()) return false;
        to.push_back({e.text(), e.cursorLine(), e.cursorColumn()});
        Snap s = std::move(from.back());
        from.pop_back();
        bytes = 0;
        for (const Snap& x : undo) bytes += x.text.size();
        if (!e.setText(s.text)) return false;
        e.moveTo(s.line, s.col);
        typing = false;
        return true;
    }
};
// Host del programma: lo stesso del nucleo (graph_commands, fx::calc, A-Z,
// liste, file nella cartella del programma) con la console e i tasti della modalita'.
class ConsoleHost final : public prog::WorkspaceHost {
public:
    ConsoleHost(Workspace& ws, app::Platform& p) : WorkspaceHost(ws), platform_(p) {}
    void clearText() override { output.clear(); }
    std::uint32_t millis() override { return platform_.millis(); }
    int getKey() override {
        if (keys.empty()) return 0;
        const int k = keys.front();
        keys.erase(keys.begin());
        return k;
    }
    std::vector<int> keys;
private:
    app::Platform& platform_;
};
} // namespace

struct ProgramApp::View {
    ModeShell shell;
    Screen screen{Files};
    Screen back{Files};                                 // da dove si e' arrivati (Run, Lib...)
    // ---- gestore dei file
    std::string dir;
    std::vector<code::Item> items;
    SelectList list;
    std::string status;                                 // "Inserire la scheda SD"...
    FileDialog dialog;
    Ask ask{AskNone};
    std::string target;                                 // file o cartella dell'operazione
    std::vector<std::string> picks;                     // risultati di SEARCH, cartelle di COPY/MOVE
    int newKind{0};                                     // 0 file, 1 progetto, 2 cartella
    code::Lang newLang{code::Lang::Cpp};
    std::string passOld;
    // ---- editor
    TextEditor editor{code::MaxEditBytes};
    std::string file;
    code::Lang lang{code::Lang::Cpp};
    bool modified{false};
    std::size_t first{0};
    int scroll{0};
    History history;
    std::string findText, replaceText;
    std::map<std::string, std::set<int>> breakpoints;  // percorso -> righe (1-based)
    // file grande: blocco aperto
    bool block{false};
    std::uint64_t blockOffset{0}, blockLength{0}, fileSize{0};
    std::size_t blockFirstLine{0};
    std::vector<std::pair<std::uint64_t, std::size_t>> blockStack;   // blocchi precedenti (offset, prima riga)
    // ---- esecuzione e debug
    std::unique_ptr<ConsoleHost> host;
    std::unique_ptr<code::ProjectSources> sources;
    std::unique_ptr<prog::Machine> machine;
    step::Runner runner;
    LineEditor input{200};
    bool finished{false};
    bool debugging{false};
    std::string runStatus;
    std::string runFolder;                              // cartella del programma in esecuzione
    std::map<std::string, std::vector<std::string>> runSources;   // file (relativo) -> righe, per il debug
    int errLine{0}, errColumn{0};
    std::string errFile;                                // percorso completo del file con l'errore
    // ---- variabili (debug)
    std::vector<prog::VarInfo> vars;
    SelectList varList;
    std::vector<std::pair<std::string, std::uint32_t>> varPath;   // espansioni aperte
    // ---- librerie
    std::vector<libs::Installed> libsInstalled;
    SelectList libList;
    std::vector<libs::IndexEntry> index;
    bool indexLoaded{false};
    std::string indexSource;                            // "rete" o "SD"
    std::string libStatus;
    std::vector<libs::PlanStep> plan;
    std::unique_ptr<libs::FolderSource> folderSource;
    std::unique_ptr<net::HttpsSource> httpsSource;
    std::unique_ptr<libs::Installer> installer;
    std::string pendingLib;                             // libreria scelta per INST/UPD
    bool fetchingIndex{false};
    std::string indexText;
    ResultList info;
    // ---- SET UP
    Form setup;
};

ProgramApp::ProgramApp() = default;
ProgramApp::~ProgramApp() = default;

namespace {
using V = ProgramApp::View;
files::FileSystem* sd(Context& ctx) { return ctx.platform.files(); }
[[noreturn]] void storeError(files::Status st, const std::string& what = {}) {
    throw MathError((what.empty() ? std::string() : what + ": ") + code::message(st));
}
code::Store store(Context& ctx) { return code::Store(sd(ctx)); }

// ================================================================ gestore dei file
void refresh(Context& ctx, V& v) {
    v.items.clear();
    v.list.items.clear();
    v.status.clear();
    code::Store st = store(ctx);
    const files::Status rs = st.ready();
    if (rs != files::Status::Ok) { v.status = code::message(rs); return; }
    if (v.dir.empty() || v.dir.rfind(code::CodeRoot, 0) != 0 || !st.isFolder(v.dir)) v.dir = code::CodeRoot;
    const files::Status ls = st.list(v.dir, v.items, static_cast<code::SortBy>(ctx.ws.program.sort));
    if (ls != files::Status::Ok) { v.status = code::message(ls); return; }
    for (const code::Item& it : v.items) {
        std::string row = it.dir ? "[" + it.name + "]" : it.name;
        if (it.locked) row += "  (password)";
        if (!it.dir) {
            char sz[24];
            if (it.size < 10240) std::snprintf(sz, sizeof sz, "  %u B", static_cast<unsigned>(it.size));
            else std::snprintf(sz, sizeof sz, "  %u KB", static_cast<unsigned>(it.size / 1024));
            row += sz;
        }
        v.list.items.push_back(row);
    }
    v.list.clampSelection();
    ctx.ws.program.folder = v.dir;
}
void selectName(V& v, const std::string& name) {
    for (std::size_t i = 0; i < v.items.size(); ++i) if (v.items[i].name == name) v.list.selected = static_cast<int>(i);
}
const code::Item* selectedItem(V& v) {
    if (v.items.empty()) return nullptr;
    return &v.items[static_cast<std::size_t>(std::clamp(v.list.selected, 0, static_cast<int>(v.items.size()) - 1))];
}
std::string selectedPath(V& v) {
    const code::Item* it = selectedItem(v);
    if (!it) throw MathError(v.status.empty() ? "nessun file qui (F3 NEW)" : v.status);
    return v.dir + "/" + it->name;
}
void toFiles(Context& ctx, V& v) {
    v.screen = Files;
    v.shell.fkeys.set(&kFileMenu);
    refresh(ctx, v);
}
std::vector<std::string> foldersUnder(code::Store& st, const std::string& root) {
    std::vector<std::string> out{root};
    for (std::size_t i = 0; i < out.size() && out.size() < 200; ++i) {
        std::vector<code::Item> items;
        if (st.list(out[i], items) != files::Status::Ok) continue;
        for (const code::Item& it : items) if (it.dir) out.push_back(out[i] + "/" + it.name);
    }
    std::sort(out.begin(), out.end());
    return out;
}
void nameRule(FileDialog& d, std::size_t maxLen) {
    d.setNameRule([](char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-' || c == '.'; }, [](char c) { return c; }, maxLen);
    d.setExisting({});
    d.setExtension("");
}

// ================================================================ editor
void ensureVisible(V& v) {
    const std::size_t cl = v.editor.cursorLine();
    if (cl < v.first) v.first = cl;
    if (cl >= v.first + EditorRows) v.first = cl - EditorRows + 1;
}
void toEdit(Context& ctx, V& v) {
    (void)ctx;
    v.editor.setWrap(false);
    v.editor.setVisibleRows(EditorRows);
    v.screen = Edit;
    v.shell.fkeys.set(&kEditMenu);
    ensureVisible(v);
}
void loadBlock(Context& ctx, V& v, std::uint64_t offset, std::size_t firstLine) {
    code::Store st = store(ctx);
    std::string text;
    std::uint64_t total = 0;
    const files::Status rs = st.readBlock(v.file, offset, BlockBytes + 4096, text, &total);
    if (rs != files::Status::Ok) storeError(rs, "lettura del blocco");
    // il blocco finisce a fine riga
    std::uint64_t len = text.size();
    if (offset + len < total) {
        const std::size_t nl = text.rfind('\n', BlockBytes);
        if (nl == std::string::npos) throw MathError("Memory ERROR: riga troppo lunga per l'editor");
        text.resize(nl + 1);
        len = nl + 1;
    }
    std::string shown = text;
    if (!shown.empty() && shown.back() == '\n') shown.pop_back();
    if (!v.editor.setText(shown)) throw MathError("Memory ERROR: blocco troppo grande");
    v.block = true;
    v.blockOffset = offset;
    v.blockLength = len;
    v.fileSize = total;
    v.blockFirstLine = firstLine;
    v.modified = false;
    v.first = 0;
    v.history.clear();
}
std::string blockText(V& v) {
    std::string t = v.editor.text();
    if (v.blockOffset + v.blockLength < v.fileSize || (v.blockLength > 0)) t += "\n";
    return t;
}
void saveFile(Context& ctx, V& v) {
    if (v.file.empty()) return;
    code::Store st = store(ctx);
    files::Status rs;
    if (v.block) {
        const std::string t = blockText(v);
        rs = st.spliceBlock(v.file, v.blockOffset, v.blockLength, t);
        if (rs == files::Status::Ok) { v.fileSize = v.fileSize - v.blockLength + t.size(); v.blockLength = t.size(); }
    } else rs = st.writeText(v.file, v.editor.text());
    if (rs != files::Status::Ok) storeError(rs, "salvataggio non riuscito (il file sulla SD e' quello di prima)");
    v.modified = false;
}
void openEditor(Context& ctx, V& v, const std::string& path, int line = 0, int column = 0) {
    code::Store st = store(ctx);
    std::uint64_t size = 0;
    const files::Status ss = st.fileSize(path, size);
    if (ss != files::Status::Ok) storeError(ss, baseName(path));
    if (size > code::MaxFileBytes) throw MathError("Memory ERROR: il file supera " + std::to_string(code::MaxFileBytes / (1024 * 1024)) + " MB (limite dell'editor)");
    v.file = path;
    if (!code::langOfFile(path, v.lang)) v.lang = code::Lang::Cpp;
    v.block = false;
    v.blockStack.clear();
    v.history.clear();
    v.first = 0;
    v.scroll = 0;
    if (size > code::MaxEditBytes) {
        loadBlock(ctx, v, 0, 0);
        ctx.toast("File grande: aperto a blocchi di " + std::to_string(BlockBytes / 1024) + " KB");
    } else {
        std::string text;
        const files::Status rs = st.readText(path, text);
        if (rs != files::Status::Ok) storeError(rs, baseName(path));
        if (!v.editor.setText(text)) throw MathError("Memory ERROR: file troppo grande per l'editor");
    }
    v.modified = false;
    v.editor.moveTo(static_cast<std::size_t>(std::max(0, line - 1)), static_cast<std::size_t>(std::max(0, column - 1)));
    progmode::addRecent(ctx.ws.program, path);
    ctx.ws.program.lastFile = path;
    ctx.markDirty();
    toEdit(ctx, v);
}
void editFile(Context& ctx, V& v, const std::string& path) {
    code::Store st = store(ctx);
    if (st.locked(path)) {
        v.target = path;
        v.ask = AskEditPass;
        v.dialog.openName("Password di " + baseName(path), "");
        nameRule(v.dialog, 8);
        return;
    }
    openEditor(ctx, v, path);
}
// Parentesi corrispondente a quella sotto (o prima del) cursore.
bool matchBracket(const TextEditor& e, std::size_t& l0, std::size_t& c0, std::size_t& l1, std::size_t& c1) {
    const std::string& line = e.line(e.cursorLine());
    std::size_t col = e.cursorColumn();
    auto isB = [](char c) { return c == '(' || c == ')' || c == '[' || c == ']' || c == '{' || c == '}'; };
    if (col >= line.size() || !isB(line[col])) { if (col > 0 && col - 1 < line.size() && isB(line[col - 1])) --col; else return false; }
    const char ch = line[col];
    const char open = ch == ')' ? '(' : ch == ']' ? '[' : ch == '}' ? '{' : ch;
    const char close = open == '(' ? ')' : open == '[' ? ']' : '}';
    const bool forward = ch == open;
    int depth = 0;
    std::size_t L = e.cursorLine();
    long C = static_cast<long>(col);
    for (std::size_t steps = 0; steps < 200000; ++steps) {
        const std::string& s = e.line(L);
        if (C >= 0 && static_cast<std::size_t>(C) < s.size()) {
            const char x = s[static_cast<std::size_t>(C)];
            if (x == open) depth += forward ? 1 : -1;
            else if (x == close) depth += forward ? -1 : 1;
            if (depth == 0) { l0 = e.cursorLine(); c0 = col; l1 = L; c1 = static_cast<std::size_t>(C); return true; }
        }
        if (forward) {
            ++C;
            if (C >= static_cast<long>(s.size())) { if (L + 1 >= e.lineCount()) return false; ++L; C = 0; if (e.line(L).empty()) C = -1; }
        } else {
            --C;
            if (C < 0) { if (L == 0) return false; --L; C = static_cast<long>(e.line(L).size()) - 1; }
        }
        if (C < 0 && forward) C = 0;
    }
    return false;
}
// Rientro automatico: quello della riga, piu' uno dopo '{' o ':' (Python).
std::string indentFor(const V& v) {
    const std::string& line = v.editor.line(v.editor.cursorLine());
    std::string ind;
    for (char c : line) { if (c == ' ' || c == '\t') ind += c; else break; }
    std::string head = line.substr(0, v.editor.cursorColumn());
    while (!head.empty() && head.back() == ' ') head.pop_back();
    if (!head.empty() && (head.back() == '{' || (v.lang == code::Lang::Python && head.back() == ':'))) ind += "    ";
    return ind;
}
int replaceAll(V& v, const std::string& what, const std::string& with) {
    if (what.empty()) return 0;
    std::string t = v.editor.text();
    int n = 0;
    std::size_t p = 0;
    while ((p = t.find(what, p)) != std::string::npos) { t.replace(p, what.size(), with); p += with.size(); ++n; }
    if (n > 0) {
        v.history.push(v.editor, false);
        const std::size_t l = v.editor.cursorLine(), c = v.editor.cursorColumn();
        if (!v.editor.setText(t)) throw MathError("Memory ERROR: testo troppo grande");
        v.editor.moveTo(l, c);
        v.modified = true;
    }
    return n;
}
void blockMove(Context& ctx, V& v, bool next) {
    if (!v.block) return;
    if (v.modified) saveFile(ctx, v);
    if (next) {
        if (v.blockOffset + v.blockLength >= v.fileSize) { ctx.toast("Fine del file"); return; }
        v.blockStack.emplace_back(v.blockOffset, v.blockFirstLine);
        const std::size_t lines = v.editor.lineCount();
        loadBlock(ctx, v, v.blockOffset + v.blockLength, v.blockFirstLine + lines);
        v.editor.moveTo(0, 0);
    } else {
        if (v.blockStack.empty()) { ctx.toast("Inizio del file"); return; }
        const auto [off, firstLine] = v.blockStack.back();
        v.blockStack.pop_back();
        loadBlock(ctx, v, off, firstLine);
        v.editor.moveTo(v.editor.lineCount() - 1, 0);
    }
    ensureVisible(v);
}

// ================================================================ esecuzione
void leaveRun(V& v) {
    v.runner.reset();
    v.machine.reset();
    v.sources.reset();
}
std::string projectFor(code::Store& st, const std::string& path) {
    // un file nella cartella di un progetto: si esegue tutto il progetto
    if (st.isFolder(path)) return path;
    const std::string parent = parentOf(path);
    if (!isLangRoot(parent) && parent.rfind(code::CodeRoot, 0) == 0 && parent.size() > std::string(code::CodeRoot).size()) return parent;
    return path;
}
void failCompile(Context& ctx, V& v, const prog::Error& e) {
    v.errLine = e.line;
    v.errColumn = e.column;
    v.errFile = e.file.empty() ? v.file : (v.runFolder.empty() ? e.file : v.runFolder + "/" + e.file);
    std::string where = (e.file.empty() ? std::string() : e.file + ", ") + "riga " + std::to_string(e.line) + ", colonna " + std::to_string(e.column);
    // il cursore va sull'errore (manuale 8-6), anche in un altro file del progetto
    try {
        if (!v.errFile.empty() && v.errFile != v.file && store(ctx).exists(v.errFile)) {
            if (v.modified && !v.file.empty()) saveFile(ctx, v);
            openEditor(ctx, v, v.errFile, e.line, e.column);
        } else if (!v.file.empty() && (v.errFile == v.file || v.errFile.empty())) {
            if (e.line > 0) v.editor.moveTo(static_cast<std::size_t>(e.line - 1), static_cast<std::size_t>(std::max(0, e.column - 1)));
            toEdit(ctx, v);
        }
    } catch (...) {
    }
    throw MathError(std::string(e.what()) + " (" + where + ")");
}
void startRun(Context& ctx, V& v, const std::string& path, bool debug) {
    leaveRun(v);
    v.errLine = v.errColumn = 0;
    v.errFile.clear();
    code::Store st = store(ctx);
    const files::Status rs = st.ready();
    if (rs != files::Status::Ok) storeError(rs);
    code::Lang l;
    const std::string what = projectFor(st, path);
    if (!st.isFolder(what) && code::langOfFile(what, l)) {
        if (l == code::Lang::Python) throw MathError("i programmi Python si eseguono nella modalita' Python (CODE/PY)");
        if (l == code::Lang::Java) throw MathError("Java arriva nella v1.21.0");
    }
    prog::Project project;
    std::string folder, err;
    const std::string edited = v.screen == Edit && !v.block ? v.file : std::string();
    const std::string editedText = edited.empty() ? std::string() : v.editor.text();
    if (!code::loadProject(st, what, project, v.sources, folder, err, edited, edited.empty() ? nullptr : &editedText)) throw MathError(err);
    v.runFolder = folder;
    std::shared_ptr<const prog::Program> program;
    try {
        program = prog::compile(project);
    } catch (const prog::Error& e) {
        failCompile(ctx, v, e);
    }
    v.runSources.clear();
    for (const prog::SourceFile& sf : project.files) {
        std::vector<std::string> lines(1);
        for (char c : sf.text) { if (c == '\n') lines.emplace_back(); else lines.back() += c; }
        v.runSources[sf.path] = std::move(lines);
    }
    v.host = std::make_unique<ConsoleHost>(ctx.ws, ctx.platform);
    v.host->fs = sd(ctx);
    v.host->folder = folder;
    const prog::Limits limits = prog::defaultLimits(mem::available(mem::Pool::PSRAM_BOARD));
    v.machine = std::make_unique<prog::Machine>(program, *v.host, limits);
    v.debugging = debug;
    if (debug) {
        v.machine->setDebug(true);
        std::vector<std::pair<std::string, int>> bps;
        for (const auto& [file, lines] : v.breakpoints) {
            if (file.size() <= folder.size() || file.compare(0, folder.size() + 1, folder + "/") != 0) continue;
            for (int ln : lines) bps.emplace_back(file.substr(folder.size() + 1), ln);
        }
        v.machine->setBreakpoints(bps);
    }
    v.runner.start(*v.machine, step::Runner::Config{2000, 25});
    v.finished = false;
    v.runStatus = std::string(debug ? "Debug" : "Esecuzione") + " (" + prog::dialectName(project.dialect) + ")";
    v.input.clear();
    if (v.screen != Run) v.back = v.screen == Edit ? Edit : Files;
    v.screen = Run;
    v.shell.fkeys.set(debug ? &kDebugMenu : &kRunMenu);
    progmode::addRecent(ctx.ws.program, path);
    ctx.ws.program.lastFile = path;
    ctx.markDirty();
}
void refreshVars(V& v) {
    v.vars.clear();
    v.varList.items.clear();
    if (!v.machine) return;
    if (v.varPath.empty()) v.vars = v.machine->variables(0);
    else v.vars = v.machine->children(v.varPath.back().second);
    for (const prog::VarInfo& vi : v.vars) v.varList.items.push_back((vi.expandable ? "+ " : "  ") + vi.name + " = " + vi.value);
    v.varList.clampSelection();
}

// ================================================================ librerie
std::string libLang(const V& v) {
    code::Lang l;
    if (langOfPath(v.dir, l)) return code::libLanguage(l);
    return {};
}
void refreshLibs(Context& ctx, V& v) {
    v.libsInstalled.clear();
    v.libList.items.clear();
    files::FileSystem* fs = sd(ctx);
    if (!fs || fs->status() != files::Status::Ok) { v.libStatus = code::message(files::Status::NoCard); return; }
    std::string note;
    (void)libs::recoverInstall(*fs, note);
    v.libsInstalled = libs::listInstalled(*fs, libLang(v));
    for (const libs::Installed& l : v.libsInstalled)
        v.libList.items.push_back(l.manifest.name + " " + (l.manifestOk ? l.manifest.version.text() : std::string("?")) + "  [" + l.manifest.language + "]" +
                                  (l.manifestOk ? "" : "  (manifest non valido)"));
    v.libList.clampSelection();
}
void toLib(Context& ctx, V& v) {
    v.screen = Lib;
    v.shell.fkeys.set(&kLibMenu);
    v.libStatus.clear();
    refreshLibs(ctx, v);
}
const libs::Installed* selectedLib(V& v) {
    if (v.libsInstalled.empty()) return nullptr;
    return &v.libsInstalled[static_cast<std::size_t>(std::clamp(v.libList.selected, 0, static_cast<int>(v.libsInstalled.size()) - 1))];
}
libs::Version firmwareVersion() {
    libs::Version v;
    (void)libs::Version::parse(FXAI_VERSION_STRING, v);
    return v;
}
void startIndexFetch(Context& ctx, V& v) {
    net::Fetcher* f = ctx.platform.fetcher();
    if (!f) throw MathError("download non disponibile: copia il repository in " + shortPath(ImportFolder) + " e usa LOCAL");
    if (net::ChatTransport* t = ctx.platform.chat()) (void)t->configureWifi(ctx.ws.settings.wifiSsid, ctx.ws.settings.wifiPassword);
    if (!f->begin(ctx.ws.program.libIndexUrl)) throw MathError("indice: " + f->error());
    v.indexText.clear();
    v.fetchingIndex = true;
    v.libStatus = "Download dell'indice...";
}
void chooseLibrary(V& v, const std::string& lang) {
    // l'ultima versione di ogni libreria del linguaggio
    std::map<std::string, const libs::IndexEntry*> latest;
    for (const libs::IndexEntry& e : v.index) {
        if (!lang.empty() && e.language != lang && !(lang == "cpp" && e.language == "c")) continue;
        auto& slot = latest[e.language + "/" + e.name];
        if (!slot || slot->version < e.version) slot = &e;
    }
    std::vector<std::string> names;
    v.picks.clear();
    for (const auto& [k, e] : latest) {
        names.push_back(e->name + " " + e->version.text() + " [" + e->language + "]  " + e->description);
        v.picks.push_back(k);
    }
    if (names.empty()) throw MathError("nessuna libreria " + (lang.empty() ? std::string() : "per " + lang + " ") + "nell'indice");
    v.ask = AskLibChoose;
    v.dialog.setExtension("");
    v.dialog.setEmptyText("Indice vuoto");
    v.dialog.openList("Installa (" + v.indexSource + ")", names);
}
void proposePlan(Context& ctx, V& v, const std::string& key) {
    const std::size_t slash = key.find('/');
    const std::string lang = key.substr(0, slash), name = key.substr(slash + 1);
    std::string err;
    files::FileSystem* fs = sd(ctx);
    if (!fs) storeError(files::Status::NoCard);
    if (!libs::makePlan(v.index, libs::listInstalled(*fs, ""), name, lang, firmwareVersion(), v.plan, err)) throw MathError(err);
    std::string text;
    bool any = false, down = false;
    for (const libs::PlanStep& s : v.plan) {
        if (s.upToDate) continue;
        any = true;
        text += (text.empty() ? "" : ", ") + s.entry.name + " " + (s.fromVersion.empty() ? std::string() : s.fromVersion + "->") + s.entry.version.text();
        down = down || s.downgrade;
    }
    if (!any) { ctx.toast(name + " e' gia' aggiornata"); return; }
    v.shell.confirm.open(std::string(down ? "ATTENZIONE versione piu' vecchia! " : "") + "Installare " + text + "?", ConfirmPlan);
}
void startInstall(Context& ctx, V& v) {
    files::FileSystem* fs = sd(ctx);
    if (!fs) storeError(files::Status::NoCard);
    libs::Source* src = nullptr;
    if (v.indexSource == "SD") {
        v.folderSource = std::make_unique<libs::FolderSource>(*fs, ImportFolder);
        src = v.folderSource.get();
    } else {
        net::Fetcher* f = ctx.platform.fetcher();
        if (!f) throw MathError("download non disponibile");
        v.httpsSource = std::make_unique<net::HttpsSource>(*f, net::urlFolder(ctx.ws.program.libIndexUrl));
        src = v.httpsSource.get();
    }
    v.installer = std::make_unique<libs::Installer>(*fs, *src);
    v.installer->start(v.plan, firmwareVersion());
    v.libStatus = "Installazione...";
}
void loadLocalIndex(Context& ctx, V& v) {
    files::FileSystem* fs = sd(ctx);
    if (!fs || fs->status() != files::Status::Ok) storeError(files::Status::NoCard);
    std::string err;
    if (!libs::localIndex(*fs, ImportFolder, v.index, err))
        throw MathError(err.find("assente") != std::string::npos ? "manca " + shortPath(ImportFolder) + " (copia la cartella libraries/ del repository o usa picocalc_link.py send-library)" : err);
    if (v.index.empty()) throw MathError("nessuna libreria in " + shortPath(ImportFolder));
    v.indexLoaded = true;
    v.indexSource = "SD";
}

// ================================================================ SET UP
void openSetup(Context& ctx, V& v) {
    v.setup.fields.clear();
    FormField url;
    url.label = "Indice LIB";
    url.type = FormField::Type::Text;
    url.text = ctx.ws.program.libIndexUrl;
    v.setup.fields.push_back(url);
    FormField sort;
    sort.label = "Ordine";
    sort.type = FormField::Type::Choice;
    sort.choices = {"Nome", "Tipo", "Dimensione"};
    sort.choice = ctx.ws.program.sort;
    v.setup.fields.push_back(sort);
    v.setup.selected = 0;
    v.back = v.screen == Setup ? v.back : v.screen;
    v.screen = Setup;
    v.shell.fkeys.set(&kSetupMenu);
}
void applySetupForm(Context& ctx, V& v) {
    std::string url = v.setup.fields[0].text;
    std::string err;
    net::Url u;
    if (!net::parseHttpsUrl(url, u, err)) throw MathError("Indice LIB: " + err);
    ctx.ws.program.libIndexUrl = url;
    ctx.ws.program.sort = static_cast<std::uint8_t>(v.setup.fields[1].choice);
    v.indexLoaded = false;
    ctx.markDirty();
}

// ================================================================ migrazione
void offerMigration(Context& ctx, V& v, bool manual) {
    files::FileSystem* flash = ctx.platform.internalFiles();
    files::FileSystem* card = sd(ctx);
    const bool fromFlash = flash && code::Store::hasLegacy(*flash);
    const bool fromCard = card && card->status() == files::Status::Ok && code::Store::hasLegacy(*card);
    if (!fromFlash && !fromCard) {
        if (manual) ctx.toast("Nessun programma della v1.17.0 da convertire");
        ctx.ws.program.migrationAsked = true;
        ctx.markDirty();
        return;
    }
    v.shell.confirm.open("Programmi della v1.17.0 trovati: convertirli in file sorgente in PICOCALC/CODE?", ConfirmMigrate);
}
void migrate(Context& ctx, V& v, bool removeFlash) {
    code::Store st = store(ctx);
    if (st.ready() != files::Status::Ok) throw MathError(code::message(files::Status::NoCard));
    int converted = 0, failed = 0;
    if (files::FileSystem* flash = ctx.platform.internalFiles()) {
        const auto r = st.migrate(*flash, removeFlash);
        converted += r.converted;
        failed += r.failed;
    }
    if (files::FileSystem* card = sd(ctx)) {
        const auto r = st.migrate(*card, false);
        converted += r.converted;
        failed += r.failed;
    }
    ctx.ws.program.migrationAsked = true;
    ctx.markDirty();
    refresh(ctx, v);
    ctx.toast("Convertiti " + std::to_string(converted) + " programmi" + (failed ? ", " + std::to_string(failed) + " non riusciti" : std::string()));
}
} // namespace

// ================================================================ App
void ProgramApp::enter(Context& ctx) {
    v_ = std::make_unique<View>();
    View& v = *v_;
    v.dir = ctx.ws.program.folder;
    toFiles(ctx, v);
    if (!ctx.ws.program.lastFile.empty()) selectName(v, baseName(ctx.ws.program.lastFile));
    if (!ctx.ws.program.migrationAsked && v.status.empty()) offerMigration(ctx, v, false);
}
void ProgramApp::leave(Context& ctx) {
    if (v_ && v_->screen == Edit && v_->modified) {
        try { saveFile(ctx, *v_); } catch (...) {}       // EXIT/MENU dall'editor: il programma resta salvato
    }
    if (v_ && v_->screen == Edit) {
        ctx.ws.program.cursorLine = static_cast<std::uint32_t>(v_->editor.cursorLine());
        ctx.ws.program.cursorColumn = static_cast<std::uint32_t>(v_->editor.cursorColumn());
    }
    if (v_ && v_->installer) v_->installer->cancel();
    v_.reset();
}
std::string ProgramApp::screenName() const {
    if (!v_) return "closed";
    switch (v_->screen) {
    case Run:
        if (v_->finished) return "done";
        if (v_->machine && v_->machine->state() == prog::Machine::State::WaitingInput) return "input";
        if (v_->machine && v_->machine->state() == prog::Machine::State::Paused) return "debug";
        return "run";
    case Edit: return "edit";
    case Vars: return "vars";
    case Lib: return "lib";
    case Info: return "info";
    case Setup: return "setup";
    default: return "list";
    }
}
bool ProgramApp::tick(Context& ctx) {
    if (!v_) return false;
    View& v = *v_;
    bool changed = false;
    // download dell'indice
    if (v.fetchingIndex) {
        net::Fetcher* f = ctx.platform.fetcher();
        if (!f) { v.fetchingIndex = false; return true; }
        const net::Fetcher::State s = f->poll(v.indexText);
        if (v.indexText.size() > libs::MaxIndexBytes) { f->cancel(); v.fetchingIndex = false; v.libStatus = "indice troppo grande"; return true; }
        if (s == net::Fetcher::State::Failed) {
            v.fetchingIndex = false;
            v.libStatus = "Indice: " + f->error();
            // senza rete: l'ultimo indice scaricato (solo per vedere le librerie e le versioni)
            if (files::FileSystem* fs = sd(ctx)) {
                std::vector<std::uint8_t> data;
                std::string err;
                if (files::readAll(*fs, std::string(libs::CacheRoot) + "/index.json", data, libs::MaxIndexBytes) == files::Status::Ok &&
                    libs::parseIndex(std::string(data.begin(), data.end()), v.index, err)) {
                    v.indexLoaded = true;
                    v.indexSource = "copia";
                    v.libStatus += " (mostro l'ultimo indice scaricato)";
                }
            }
            return true;
        }
        if (s == net::Fetcher::State::Done) {
            v.fetchingIndex = false;
            std::string err;
            if (!libs::parseIndex(v.indexText, v.index, err)) { v.libStatus = err; return true; }
            v.indexLoaded = true;
            v.indexSource = "rete";
            // copia dell'indice in LIB/.cache: elenco e INFO anche senza rete
            if (files::FileSystem* fs = sd(ctx)) {
                if (fs->makeDirs(libs::CacheRoot) == files::Status::Ok)
                    (void)files::writeAtomic(*fs, std::string(libs::CacheRoot) + "/index.json", reinterpret_cast<const std::uint8_t*>(v.indexText.data()), v.indexText.size());
            }
            v.libStatus.clear();
            try {
                if (!v.pendingLib.empty()) { const std::string k = v.pendingLib; v.pendingLib.clear(); proposePlan(ctx, v, k); }
                else chooseLibrary(v, libLang(v));
            } catch (const MathError& e) {
                v.libStatus = e.what();
            }
        }
        return true;
    }
    // installazione a passi
    if (v.installer) {
        const bool more = v.installer->tick(8192);
        changed = true;
        if (!more) {
            const libs::Installer::State s = v.installer->state();
            v.libStatus = s == libs::Installer::State::Done ? "Installazione completata" : v.installer->error();
            v.installer.reset();
            v.folderSource.reset();
            v.httpsSource.reset();
            refreshLibs(ctx, v);
        } else v.libStatus = "Installazione: " + v.installer->progress();
        return changed;
    }
    if (v.screen != Run || !v.runner.running()) return false;
    try {
        v.runner.tick([&ctx] { return ctx.now(); });
    } catch (const prog::Error& e) {
        v.finished = true;
        v.errLine = e.line;
        v.errColumn = e.column;
        v.errFile = e.file.empty() ? v.file : v.runFolder + "/" + e.file;
        v.runStatus = "Errore" + (e.file.empty() ? std::string() : " in " + e.file) + " alla riga " + std::to_string(e.line);
        std::string trace;
        for (std::size_t i = 0; i < e.trace.size() && i < 4; ++i) trace += "\n  " + e.trace[i];
        if (v.host) v.host->output += std::string("\n") + e.what() + trace + "\n";
        throw MathError(std::string(e.what()) + " (" + (e.file.empty() ? std::string() : e.file + ", ") + "riga " + std::to_string(e.line) + ", colonna " + std::to_string(e.column) + ")");
    }
    if (!v.runner.running()) {
        v.finished = true;
        v.runStatus = v.runner.state() == step::State::Break ? "Break" : "Fine (codice " + std::to_string(v.machine ? v.machine->exitCode() : 0) + ")";
    }
    return true;
}

void ProgramApp::key(Context& ctx, const KeyEvent& ev) {
    if (!v_) enter(ctx);
    View& v = *v_;
    // ---- nomi, elenchi e domande
    if (v.dialog.active()) {
        const int r = v.dialog.key(ev);
        if (r == 0) return;
        const Ask ask = v.ask;
        v.ask = AskNone;
        if (r < 0) return;
        const std::string name = v.dialog.name();
        const int pick = v.dialog.selectedIndex();
        v.dialog.close();
        code::Store st = store(ctx);
        switch (ask) {
        case AskNewName: {
            std::string path;
            files::Status s;
            if (v.newKind == 0) s = st.newFile(v.newLang, v.dir, name, path);
            else if (v.newKind == 1) s = st.newProject(v.newLang, v.dir, name, path);
            else { path = v.dir + "/" + name; s = st.makeFolder(path); }
            if (s != files::Status::Ok) storeError(s, name);
            refresh(ctx, v);
            selectName(v, baseName(path));
            if (v.newKind == 0) openEditor(ctx, v, path);
            else if (v.newKind == 1) {
                v.dir = path;
                refresh(ctx, v);
            }
            return;
        }
        case AskRename: {
            const files::Status s = st.rename(v.target, name);
            if (s != files::Status::Ok) storeError(s, name);
            refresh(ctx, v);
            selectName(v, name);
            return;
        }
        case AskSearch: {
            v.picks = st.search(v.dir, name);
            if (v.picks.empty()) { ctx.toast("Nessun nome inizia con \"" + name + "\""); return; }
            std::vector<std::string> shown;
            for (const std::string& p : v.picks) shown.push_back(p.substr(v.dir.size() + 1));
            v.ask = AskPick;
            v.dialog.setExtension("");
            v.dialog.openList("Trovati", shown);
            return;
        }
        case AskPick: {
            if (pick < 0 || static_cast<std::size_t>(pick) >= v.picks.size()) return;
            const std::string p = v.picks[static_cast<std::size_t>(pick)];
            v.dir = parentOf(p);
            refresh(ctx, v);
            selectName(v, baseName(p));
            return;
        }
        case AskPassOld:
            if (!st.checkPassword(v.target, name)) throw MathError("password errata");
            v.passOld = name;
            v.ask = AskPassNew;
            v.dialog.openName("Nuova password (vuota: nessuna)", "");
            nameRule(v.dialog, 8);
            return;
        case AskPassNew: {
            const files::Status s = st.setPassword(v.target, name);
            if (s != files::Status::Ok) storeError(s, "password");
            ctx.toast(name.empty() ? "Password tolta" : "Password impostata (chiesta per EDIT)");
            refresh(ctx, v);
            return;
        }
        case AskEditPass:
            if (!st.checkPassword(v.target, name)) throw MathError("password errata");
            openEditor(ctx, v, v.target);
            return;
        case AskFind:
            v.findText = name;
            if (!v.editor.find(name)) ctx.toast("\"" + name + "\" non trovato");
            ensureVisible(v);
            return;
        case AskReplaceWhat:
            v.findText = name;
            v.ask = AskReplaceWith;
            v.dialog.openName("Sostituisci con", v.replaceText);
            v.dialog.setNameRule([](char c) { return c >= 32 && c < 127; }, [](char c) { return c; }, 64);
            return;
        case AskReplaceWith: {
            v.replaceText = name;
            const int n = replaceAll(v, v.findText, v.replaceText);
            ctx.toast(std::to_string(n) + " sostituzioni");
            return;
        }
        case AskGoto: {
            const long n = std::strtol(name.c_str(), nullptr, 10);
            const std::size_t local = n > 0 && static_cast<std::size_t>(n) > v.blockFirstLine ? static_cast<std::size_t>(n) - v.blockFirstLine : 0;
            if (local == 0 || !v.editor.gotoLine(local)) ctx.toast("riga " + name + " non nel testo aperto");
            ensureVisible(v);
            return;
        }
        case AskSaveAs: {
            std::string n = name;
            std::string e = code::extensionOf(v.lang);
            if (n.find('.') == std::string::npos) n += e;
            if (!code::validItemName(n)) throw MathError("nome non valido");
            const std::string path = parentOf(v.file) + "/" + n;
            if (st.exists(path)) throw MathError("esiste gia' " + n);
            if (v.block) throw MathError("SAVE-AS non disponibile per i file aperti a blocchi");
            const files::Status s = st.writeText(path, v.editor.text());
            if (s != files::Status::Ok) storeError(s, n);
            v.file = path;
            v.modified = false;
            progmode::addRecent(ctx.ws.program, path);
            ctx.toast("Salvato come " + n);
            return;
        }
        case AskCopyTo: case AskMoveTo: {
            if (pick < 0 || static_cast<std::size_t>(pick) >= v.picks.size()) return;
            const std::string dest = v.picks[static_cast<std::size_t>(pick)];
            std::string np;
            const files::Status s = ask == AskCopyTo ? st.copy(v.target, dest, &np) : st.move(v.target, dest);
            if (s != files::Status::Ok) storeError(s, baseName(v.target));
            ctx.toast(std::string(ask == AskCopyTo ? "Copiato in " : "Spostato in ") + shortPath(dest));
            refresh(ctx, v);
            return;
        }
        case AskLibChoose:
            if (pick < 0 || static_cast<std::size_t>(pick) >= v.picks.size()) return;
            proposePlan(ctx, v, v.picks[static_cast<std::size_t>(pick)]);
            return;
        default: return;
        }
    }
    const bool modal = v.shell.modalKey(ev,
        [&](int tag, bool yes) {
            code::Store st = store(ctx);
            switch (tag) {
            case ConfirmDel:
                if (!yes) return;
                {
                    const files::Status s = st.remove(v.target);
                    if (s != files::Status::Ok) storeError(s, baseName(v.target));
                    refresh(ctx, v);
                }
                return;
            case ConfirmDelAll:
                if (!yes) return;
                {
                    const files::Status s = st.removeAll(v.dir);
                    if (s != files::Status::Ok) storeError(s);
                    refresh(ctx, v);
                }
                return;
            case ConfirmMigrate:
                if (!yes) { ctx.ws.program.migrationAsked = true; ctx.markDirty(); return; }
                if (ctx.platform.internalFiles() && code::Store::hasLegacy(*ctx.platform.internalFiles()))
                    v.shell.confirm.open("Dopo la conversione, eliminare gli originali dalla flash interna?", ConfirmMigrateRemove);
                else migrate(ctx, v, false);
                return;
            case ConfirmMigrateRemove: migrate(ctx, v, yes); return;
            case ConfirmLibDel:
                if (!yes) return;
                if (const libs::Installed* l = selectedLib(v)) {
                    std::string err;
                    files::FileSystem* fs = sd(ctx);
                    if (!fs || libs::removeLibrary(*fs, l->manifest.language, l->manifest.name, err) != files::Status::Ok) throw MathError(err.empty() ? "rimozione non riuscita" : err);
                    ctx.toast("Libreria rimossa (i suoi dati restano in LIBDATA)");
                    refreshLibs(ctx, v);
                }
                return;
            case ConfirmPlan:
                if (yes) startInstall(ctx, v);
                return;
            default: return;
            }
        },
        [](int) {},
        [&](int tag, int choice) {
            if (choice < 0) return;
            if (tag == MenuNewKind) {
                v.newKind = choice;
                code::Lang l;
                if (choice < 2 && !langOfPath(v.dir, l)) {
                    std::vector<std::string> names;
                    for (std::size_t i = 0; i < code::LangCount; ++i) names.push_back(code::nameOf(static_cast<code::Lang>(i)));
                    v.shell.showMenu("Linguaggio", names, MenuNewLang);
                    return;
                }
                if (choice < 2) v.newLang = l;
                v.ask = AskNewName;
                v.dialog.openName(choice == 0 ? "Nome del file" : choice == 1 ? "Nome del progetto" : "Nome della cartella", "");
                nameRule(v.dialog, 32);
                return;
            }
            if (tag == MenuNewLang) {
                v.newLang = static_cast<code::Lang>(choice);
                v.dir = code::rootOf(v.newLang);
                refresh(ctx, v);
                v.ask = AskNewName;
                v.dialog.openName(v.newKind == 0 ? "Nome del file" : "Nome del progetto", "");
                nameRule(v.dialog, 32);
            }
        });
    if (modal) return;
    if (ev.is(Key::Setup)) { openSetup(ctx, v); return; }
    switch (v.screen) {
    // ------------------------------------------------------------ gestore dei file
    case Files: {
        const int a = v.shell.fkeys.key(ev, ctx.shift());
        if (a == FKeyBar::Handled) return;
        code::Store st = store(ctx);
        const code::Item* it = selectedItem(v);
        switch (a) {
        case ActRun: case ActDebug: {
            const std::string p = selectedPath(v);
            if (it && it->dir && isLangRoot(p)) { v.dir = p; refresh(ctx, v); return; }
            startRun(ctx, v, p, a == ActDebug);
            return;
        }
        case ActEdit: {
            const std::string p = selectedPath(v);
            if (it && it->dir) { v.dir = p; refresh(ctx, v); return; }
            editFile(ctx, v, p);
            return;
        }
        case ActNew:
            if (!v.status.empty()) throw MathError(v.status);
            v.shell.showMenu("NEW", {"File", "Progetto (cartella con i file)", "Cartella"}, MenuNewKind);
            return;
        case ActDel:
            v.target = selectedPath(v);
            v.shell.confirm.open(std::string(it && it->dir ? "Eliminare la cartella " : "Eliminare ") + baseName(v.target) + (it && it->dir ? " e il suo contenuto?" : "?"), ConfirmDel);
            return;
        case ActDelAll:
            if (v.items.empty()) return;
            v.shell.confirm.open("Eliminare TUTTO in " + shortPath(v.dir) + "?", ConfirmDelAll);
            return;
        case ActSearch:
            if (!v.status.empty()) throw MathError(v.status);
            v.ask = AskSearch;
            v.dialog.openSearch("Cerca il nome (inizio)");
            return;
        case ActRename:
            v.target = selectedPath(v);
            v.ask = AskRename;
            v.dialog.openName("Nuovo nome", baseName(v.target));
            nameRule(v.dialog, 40);
            return;
        case ActCopy: case ActMove: {
            v.target = selectedPath(v);
            code::Lang l;
            const std::string root = langOfPath(v.target, l) ? code::rootOf(l) : std::string(code::CodeRoot);
            v.picks = foldersUnder(st, root);
            std::vector<std::string> shown;
            for (const std::string& p : v.picks) shown.push_back(shortPath(p));
            v.ask = a == ActCopy ? AskCopyTo : AskMoveTo;
            v.dialog.setExtension("");
            v.dialog.openList(a == ActCopy ? "Copia in" : "Sposta in", shown);
            return;
        }
        case ActSort:
            ctx.ws.program.sort = static_cast<std::uint8_t>((ctx.ws.program.sort + 1) % 3);
            ctx.markDirty();
            refresh(ctx, v);
            ctx.toast(std::string("Ordine: ") + (ctx.ws.program.sort == 0 ? "nome" : ctx.ws.program.sort == 1 ? "tipo" : "dimensione"));
            return;
        case ActPass: {
            v.target = selectedPath(v);
            if (it && it->dir) throw MathError("la password vale per i file");
            if (st.locked(v.target)) { v.ask = AskPassOld; v.dialog.openName("Password attuale", ""); }
            else { v.ask = AskPassNew; v.dialog.openName("Password (al piu' 8 caratteri)", ""); }
            nameRule(v.dialog, 8);
            return;
        }
        case ActLib: toLib(ctx, v); return;
        case ActSetup: openSetup(ctx, v); return;
        case ActMigrate: offerMigration(ctx, v, true); return;
        default: break;
        }
        if (ev.is(Key::Exe)) {
            const std::string p = selectedPath(v);
            if (it && it->dir) { v.dir = p; refresh(ctx, v); v.list.selected = 0; return; }
            startRun(ctx, v, p, false);
            return;
        }
        if (ev.is(Key::Right) && it && it->dir) { v.dir = v.dir + "/" + it->name; refresh(ctx, v); v.list.selected = 0; return; }
        if (ev.is(Key::Exit) || ev.is(Key::Left)) {
            if (v.dir.size() > std::string(code::CodeRoot).size()) {
                const std::string was = baseName(v.dir);
                v.dir = parentOf(v.dir);
                refresh(ctx, v);
                selectName(v, was);
                return;
            }
            if (ev.is(Key::Exit)) ctx.home();
            return;
        }
        if (ev.key == Key::Char && !ev.ctrl && std::isalnum(static_cast<unsigned char>(ev.ch))) {
            // salto al primo nome con la lettera (come l'elenco della fx-CG50)
            for (std::size_t i = 0; i < v.items.size(); ++i)
                if (std::tolower(static_cast<unsigned char>(v.items[i].name[0])) == std::tolower(static_cast<unsigned char>(ev.ch))) { v.list.selected = static_cast<int>(i); break; }
            return;
        }
        (void)v.list.key(ev, 14);
        return;
    }
    // ------------------------------------------------------------ editor
    case Edit: {
        const int a = v.shell.fkeys.key(ev, false);
        if (a == FKeyBar::Handled) return;
        switch (a) {
        case ActRun: case ActDebug: {
            if (v.block) throw MathError("Memory ERROR: il file supera " + std::to_string(prog::MaxSourceBytes / 1024) + " KB e non si esegue");
            if (v.modified) saveFile(ctx, v);
            ctx.ws.program.cursorLine = static_cast<std::uint32_t>(v.editor.cursorLine());
            startRun(ctx, v, v.file, a == ActDebug);
            return;
        }
        case ActSave: saveFile(ctx, v); ctx.toast("Salvato " + baseName(v.file)); return;
        case ActFind:
            if (!v.findText.empty() && ctx.shift()) { if (!v.editor.find(v.findText)) ctx.toast("non trovato"); ensureVisible(v); return; }
            v.ask = AskFind;
            v.dialog.openName("Trova", v.findText);
            v.dialog.setNameRule([](char c) { return c >= 32 && c < 127; }, [](char c) { return c; }, 64);
            return;
        case ActReplace:
            v.ask = AskReplaceWhat;
            v.dialog.openName("Sostituisci", v.findText);
            v.dialog.setNameRule([](char c) { return c >= 32 && c < 127; }, [](char c) { return c; }, 64);
            return;
        case ActGoto:
            v.ask = AskGoto;
            v.dialog.openName("Vai alla riga", "");
            v.dialog.setNameRule([](char c) { return c >= '0' && c <= '9'; }, [](char c) { return c; }, 7);
            return;
        case ActUndo: if (!v.history.apply(v.editor, v.history.undo, v.history.redo)) ctx.toast("niente da annullare"); else v.modified = true; ensureVisible(v); return;
        case ActRedo: if (!v.history.apply(v.editor, v.history.redo, v.history.undo)) ctx.toast("niente da ripetere"); else v.modified = true; ensureVisible(v); return;
        case ActSaveAs:
            v.ask = AskSaveAs;
            v.dialog.openName("Salva come", baseName(v.file));
            nameRule(v.dialog, 40);
            return;
        case ActBreak: {
            auto& set = v.breakpoints[v.file];
            const int ln = static_cast<int>(v.blockFirstLine + v.editor.cursorLine() + 1);
            if (set.count(ln)) set.erase(ln); else set.insert(ln);
            ctx.toast(set.count(ln) ? "Punto d'interruzione alla riga " + std::to_string(ln) : "Punto d'interruzione tolto");
            return;
        }
        case ActTop: if (v.block && !v.blockStack.empty()) { while (!v.blockStack.empty()) blockMove(ctx, v, false); } v.editor.top(); ensureVisible(v); return;
        case ActBottom: v.editor.bottom(); ensureVisible(v); return;
        case ActErr:
            if (v.errLine <= 0) { ctx.toast("nessun errore"); return; }
            if (!v.errFile.empty() && v.errFile != v.file) { if (v.modified) saveFile(ctx, v); openEditor(ctx, v, v.errFile, v.errLine, v.errColumn); return; }
            v.editor.moveTo(static_cast<std::size_t>(v.errLine - 1), static_cast<std::size_t>(std::max(0, v.errColumn - 1)));
            ensureVisible(v);
            return;
        default: break;
        }
        if (ev.is(Key::Exit)) {
            if (v.modified) saveFile(ctx, v);
            ctx.ws.program.cursorLine = static_cast<std::uint32_t>(v.editor.cursorLine());
            ctx.ws.program.cursorColumn = static_cast<std::uint32_t>(v.editor.cursorColumn());
            ctx.markDirty();
            const std::string name = baseName(v.file);
            const std::string folder = parentOf(v.file);
            if (!folder.empty()) v.dir = folder;
            toFiles(ctx, v);
            selectName(v, name);
            return;
        }
        // scorciatoie: Ctrl+S salva, Ctrl+Z annulla, Ctrl+Y ripete, Ctrl+F trova
        if (ev.key == Key::Char && ev.ctrl) {
            const char c = static_cast<char>(std::tolower(static_cast<unsigned char>(ev.ch)));
            if (c == 's') { saveFile(ctx, v); ctx.toast("Salvato"); return; }
            if (c == 'z') { if (v.history.apply(v.editor, v.history.undo, v.history.redo)) v.modified = true; ensureVisible(v); return; }
            if (c == 'y') { if (v.history.apply(v.editor, v.history.redo, v.history.undo)) v.modified = true; ensureVisible(v); return; }
            if (c == 'f') { v.ask = AskFind; v.dialog.openName("Trova", v.findText); v.dialog.setNameRule([](char x) { return x >= 32 && x < 127; }, [](char x) { return x; }, 64); return; }
        }
        // file a blocchi: Pagina giu' alla fine / su all'inizio cambiano blocco
        if (v.block && ev.is(Key::PageDown) && v.editor.cursorLine() + 1 >= v.editor.lineCount()) { blockMove(ctx, v, true); return; }
        if (v.block && ev.is(Key::PageUp) && v.editor.cursorLine() == 0) { blockMove(ctx, v, false); return; }
        const bool typing = ev.key == Key::Char && !ev.ctrl;
        const bool editing = typing || ev.is(Key::Exe) || ev.is(Key::Del) || ev.is(Key::DelForward) || ev.is(Key::AC) || (ev.key == Key::Char && ev.ctrl && (ev.ch == 'x' || ev.ch == 'v' || ev.ch == 'X' || ev.ch == 'V'));
        if (editing) v.history.push(v.editor, typing && ev.ch != ' ');
        if (ev.is(Key::Exe)) {
            const std::string ind = indentFor(v);
            if (v.editor.newline() && !ind.empty()) (void)v.editor.insert(ind);
            v.modified = true;
            ensureVisible(v);
            return;
        }
        if (ev.key == Key::Char && ev.ch == '\t') { (void)v.editor.insert("    "); v.modified = true; return; }
        if (ev.key == Key::Char && ev.ch == '}' && !ev.ctrl) {
            // '}' a inizio riga: un livello di rientro in meno
            const std::string& line = v.editor.line(v.editor.cursorLine());
            const bool blank = line.find_first_not_of(' ') == std::string::npos && v.editor.cursorColumn() == line.size();
            if (blank && line.size() >= 4) { for (int k = 0; k < 4; ++k) v.editor.backspace(); }
        }
        if (v.editor.key(ev, &ctx.textClipboard)) { if (editing) v.modified = true; }
        if (v.editor.full()) ctx.toast("Memory ERROR: file al limite dell'editor");
        ensureVisible(v);
        return;
    }
    // ------------------------------------------------------------ esecuzione e debug
    case Run: {
        const bool running = v.runner.running();
        const bool paused = v.machine && v.machine->state() == prog::Machine::State::Paused && running;
        if (running && ev.is(Key::AC)) {
            v.runner.interrupt();
            v.finished = true;
            v.runStatus = "Break";
            throw MathError("Break: programma interrotto (file del programma chiusi)");
        }
        if (paused) {
            const int a = v.shell.fkeys.key(ev, false);
            prog::StepMode m = prog::StepMode::Into;
            switch (a) {
            case ActStep: m = prog::StepMode::Into; break;
            case ActOver: m = prog::StepMode::Over; break;
            case ActOut: m = prog::StepMode::Out; break;
            case ActCont: m = prog::StepMode::Run; break;
            case ActVars: v.varPath.clear(); refreshVars(v); v.screen = Vars; v.shell.fkeys.set(&kVarsMenu); return;
            case ActStop: v.runner.interrupt(); v.finished = true; v.runStatus = "Fermato"; v.shell.fkeys.set(&kRunMenu); return;
            default:
                if (ev.is(Key::Exe)) { m = prog::StepMode::Into; break; }
                return;
            }
            v.machine->resume(m);
            return;
        }
        const bool waiting = v.machine && v.machine->state() == prog::Machine::State::WaitingInput;
        if (waiting && !v.finished) {
            if (ev.is(Key::Exe)) {
                v.host->output += v.input.text() + "\n";
                v.host->input.push_back(v.input.text());
                v.input.clear();
                return;
            }
            if (ev.ctrl && ev.key == Key::Char && (ev.ch == 'd' || ev.ch == 'D')) {
                // Ctrl+D: fine dell'input (EOF), dopo l'eventuale riga gia' scritta
                if (!v.input.text().empty()) { v.host->output += v.input.text() + "\n"; v.host->input.push_back(v.input.text()); v.input.clear(); }
                v.host->output += "^D\n";
                v.host->endOfInput = true;
                return;
            }
            (void)v.input.key(ev);
            return;
        }
        if (running && v.host) {
            // tasti per Getkey / Console.ReadKey
            if (ev.key == Key::Char) v.host->keys.push_back(static_cast<unsigned char>(ev.ch));
            else if (ev.is(Key::Exe)) v.host->keys.push_back(13);
            else if (ev.is(Key::Up)) v.host->keys.push_back(28);
            else if (ev.is(Key::Down)) v.host->keys.push_back(37);
            else if (ev.is(Key::Left)) v.host->keys.push_back(38);
            else if (ev.is(Key::Right)) v.host->keys.push_back(27);
            return;
        }
        const int a = v.shell.fkeys.key(ev, false);
        if (a == ActEdit || (v.finished && ev.is(Key::Exe) && v.errLine > 0)) {
            leaveRun(v);
            if (!v.errFile.empty() && v.errFile != v.file) { openEditor(ctx, v, v.errFile, v.errLine, v.errColumn); return; }
            if (v.file.empty()) { toFiles(ctx, v); return; }
            if (v.errLine > 0) v.editor.moveTo(static_cast<std::size_t>(v.errLine - 1), static_cast<std::size_t>(std::max(0, v.errColumn - 1)));
            toEdit(ctx, v);
            return;
        }
        if (a == ActGraph && v.host && !v.host->commands.empty()) { ctx.open(AppId::Graph); return; }
        if (v.finished && (ev.is(Key::Exit) || ev.is(Key::Exe))) {
            leaveRun(v);
            if (v.back == Edit && !v.file.empty()) toEdit(ctx, v);
            else toFiles(ctx, v);
        }
        return;
    }
    case Vars: {
        if (ev.is(Key::Exit)) {
            if (!v.varPath.empty()) { v.varPath.pop_back(); refreshVars(v); return; }
            v.screen = Run;
            v.shell.fkeys.set(&kDebugMenu);
            return;
        }
        if (ev.is(Key::Exe) || ev.is(Key::Right)) {
            if (v.vars.empty()) return;
            const prog::VarInfo& vi = v.vars[static_cast<std::size_t>(std::clamp(v.varList.selected, 0, static_cast<int>(v.vars.size()) - 1))];
            if (vi.expandable && vi.handle) { v.varPath.emplace_back(vi.name, vi.handle); v.varList.selected = 0; refreshVars(v); }
            return;
        }
        (void)v.varList.key(ev, 14);
        return;
    }
    // ------------------------------------------------------------ librerie
    case Lib: {
        if (ev.is(Key::AC) && (v.installer || v.fetchingIndex)) {
            if (v.installer) v.installer->cancel();
            if (v.fetchingIndex && ctx.platform.fetcher()) ctx.platform.fetcher()->cancel();
            v.fetchingIndex = false;
            v.installer.reset();
            v.libStatus = "Annullato: nessuna libreria lasciata a meta'";
            refreshLibs(ctx, v);
            return;
        }
        if (v.installer || v.fetchingIndex) return;
        const int a = v.shell.fkeys.key(ev, ctx.shift());
        if (a == FKeyBar::Handled) return;
        files::FileSystem* fs = sd(ctx);
        switch (a) {
        case ActInfo: {
            const libs::Installed* l = selectedLib(v);
            if (!l) return;
            v.info.clear();
            v.info.title = "LIB " + l->manifest.name;
            if (!l->manifestOk) v.info.add("Errore", l->error);
            v.info.add("Versione", l->manifest.version.text());
            v.info.add("Linguaggio", l->manifest.language);
            if (!l->manifest.description.empty()) v.info.add("Descrizione", l->manifest.description);
            if (!l->manifest.author.empty()) v.info.add("Autore", l->manifest.author);
            if (!l->manifest.license.empty()) v.info.add("Licenza", l->manifest.license);
            if (!l->manifest.ns.empty()) v.info.add("Namespace", l->manifest.ns);
            v.info.add("Firmware minimo", l->manifest.minFirmware.text());
            std::uint64_t bytes = 0;
            for (const auto& f : l->manifest.files) bytes += f.size;
            v.info.add("File", std::to_string(l->manifest.files.size()) + " (" + std::to_string(bytes) + " byte)");
            for (const auto& d : l->manifest.dependencies) v.info.add("Dipende da", d.name + " " + d.range);
            v.info.add("Cartella", shortPath(l->root));
            v.screen = Info;
            v.shell.fkeys.set(&kInfoMenu);
            return;
        }
        case ActInstall:
            v.pendingLib.clear();
            if (v.indexLoaded) { chooseLibrary(v, libLang(v)); return; }
            startIndexFetch(ctx, v);
            return;
        case ActLocal:
            loadLocalIndex(ctx, v);
            chooseLibrary(v, libLang(v));
            return;
        case ActIndex:
            v.indexLoaded = false;
            startIndexFetch(ctx, v);
            return;
        case ActUpdate: {
            const libs::Installed* l = selectedLib(v);
            if (!l) return;
            v.pendingLib = l->manifest.language + "/" + l->manifest.name;
            if (v.indexLoaded) { const std::string k = v.pendingLib; v.pendingLib.clear(); proposePlan(ctx, v, k); return; }
            startIndexFetch(ctx, v);
            return;
        }
        case ActRemove: {
            const libs::Installed* l = selectedLib(v);
            if (!l || !fs) return;
            const auto deps = libs::dependents(*fs, l->manifest.language, l->manifest.name);
            std::string warn;
            for (const std::string& d : deps) warn += (warn.empty() ? "" : ", ") + d;
            v.shell.confirm.open("Rimuovere " + l->manifest.name + "?" + (warn.empty() ? std::string() : " Serve a: " + warn), ConfirmLibDel);
            return;
        }
        case ActVerify: {
            const libs::Installed* l = selectedLib(v);
            if (!l || !fs) return;
            const libs::VerifyReport rep = libs::verify(*fs, *l);
            v.info.clear();
            v.info.title = "VERIFY " + l->manifest.name;
            if (rep.ok) v.info.add("Esito", "OK: " + std::to_string(l->manifest.files.size()) + " file integri (SHA-256)");
            for (const std::string& p : rep.problems) v.info.add("!", p);
            v.screen = Info;
            v.shell.fkeys.set(&kInfoMenu);
            return;
        }
        default: break;
        }
        if (ev.is(Key::Exit)) { toFiles(ctx, v); return; }
        (void)v.libList.key(ev, 14);
        return;
    }
    case Info:
        if (ev.is(Key::Exit) || ev.is(Key::Exe)) { v.screen = Lib; v.shell.fkeys.set(&kLibMenu); return; }
        (void)v.info.key(ev);
        return;
    case Setup: {
        const int a = v.shell.fkeys.key(ev, false);
        if (a == ActOk || (ev.is(Key::Exe) && !v.setup.editing() && v.setup.fields[static_cast<std::size_t>(v.setup.selected)].type == FormField::Type::Choice)) {
            applySetupForm(ctx, v);
            ctx.toast("SET UP salvato");
            if (a == ActOk) { if (v.back == Lib) toLib(ctx, v); else if (v.back == Edit && !v.file.empty()) toEdit(ctx, v); else toFiles(ctx, v); }
            return;
        }
        if (a == ActMigrate) { offerMigration(ctx, v, true); return; }
        if (ev.is(Key::Exit) && !v.setup.editing()) { if (v.back == Lib) toLib(ctx, v); else if (v.back == Edit && !v.file.empty()) toEdit(ctx, v); else toFiles(ctx, v); return; }
        (void)v.setup.key(ctx, ev);
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
    case Files: {
        const char* sortName = ctx.ws.program.sort == 0 ? "nome" : ctx.ws.program.sort == 1 ? "tipo" : "dim.";
        drawModeHeader(c, shortPath(v.dir) + "/   (" + std::to_string(v.items.size()) + ", per " + sortName + ")");
        if (!v.status.empty()) ui::textCenter(c, ui::ScreenW / 2, 120, v.status.c_str(), Warn);
        else if (v.items.empty()) ui::textCenter(c, ui::ScreenW / 2, 120, "Cartella vuota: F3 NEW", Muted);
        else v.list.draw(c, gfx::Rect{0, top + 18, ui::ScreenW, ui::ContentBottom - top - 18}, ListRowH);
        break;
    }
    case Edit: {
        const std::size_t lineNo = v.blockFirstLine + v.editor.cursorLine() + 1;
        std::string head = baseName(v.file) + (v.modified ? " *" : "") + "  [" + code::nameOf(v.lang) + "]  " + std::to_string(lineNo) + ":" +
                           std::to_string(v.editor.cursorColumn() + 1);
        if (v.block) head += "  blocco " + std::to_string(v.blockStack.size() + 1);
        drawModeHeader(c, head);
        ensureVisible(v);
        // numeri di riga
        const std::size_t lastNo = v.blockFirstLine + std::min(v.editor.lineCount(), v.first + EditorRows);
        int digits = 1;
        for (std::size_t n = lastNo; n >= 10; n /= 10) ++digits;
        const int gutter = 6 * std::max(digits, 2) + 6;
        const int charW = 8, cols = (ui::ScreenW - gutter - 4) / charW;
        const int cc = static_cast<int>(v.editor.cursorColumn());
        if (cc < v.scroll) v.scroll = cc;
        if (cc >= v.scroll + cols) v.scroll = cc - cols + 1;
        std::size_t ml0 = 0, mc0 = 0, ml1 = 0, mc1 = 0;
        const bool match = matchBracket(v.editor, ml0, mc0, ml1, mc1);
        int state = 0;
        for (std::size_t l = 0; l < v.first && l < v.editor.lineCount(); ++l) (void)progmode::highlightCode(v.editor.line(l), static_cast<int>(v.lang), state);
        const auto& bps = v.breakpoints[v.file];
        for (int r = 0; r < EditorRows && v.first + static_cast<std::size_t>(r) < v.editor.lineCount(); ++r) {
            const std::size_t li = v.first + static_cast<std::size_t>(r);
            const std::string& line = v.editor.line(li);
            const auto cls = progmode::highlightCode(line, static_cast<int>(v.lang), state);
            const int y = top + 18 + r * 16;
            const int no = static_cast<int>(v.blockFirstLine + li + 1);
            const bool err = v.errLine > 0 && no == v.errLine && (v.errFile.empty() || v.errFile == v.file);
            if (err) c.rect(0, y, ui::ScreenW, 16, ErrorHighlight, true);
            if (bps.count(no)) c.rect(0, y, gutter - 3, 16, rgb888To565(255, 190, 190), true);
            char num[12];
            std::snprintf(num, sizeof num, "%d", no);
            ui::textRight(c, gutter - 4, y + 3, num, Muted, font::Face::Small);
            c.vLine(gutter - 2, y, 16, Faint);
            for (std::size_t k = static_cast<std::size_t>(v.scroll); k < line.size() && static_cast<int>(k) < v.scroll + cols; ++k) {
                const int x = gutter + static_cast<int>(k - static_cast<std::size_t>(v.scroll)) * charW;
                if (match && ((li == ml0 && k == mc0) || (li == ml1 && k == mc1))) c.rect(x, y, charW, 16, Highlight, true);
                const char s[2] = {line[k], 0};
                font::draw(c, font::Face::Medium, x, y, s, tokColor(cls[k]));
            }
            if (li == v.editor.cursorLine()) c.vLine(gutter + (cc - v.scroll) * charW, y + 1, 14, Cursor);
        }
        break;
    }
    case Run: {
        const bool paused = v.machine && v.machine->state() == prog::Machine::State::Paused && v.runner.running();
        drawModeHeader(c, (v.file.empty() ? std::string("programma") : baseName(v.file)) + "  " + (paused ? std::string("pausa") : v.runStatus));
        if (paused) {
            // debug: righe attorno alla riga corrente e variabili
            const auto stack = v.machine->callStack();
            const std::string file = stack.empty() ? std::string() : stack.front().file;
            const int line = stack.empty() ? 0 : stack.front().line;
            const auto it = v.runSources.find(file);
            const int y0 = top + 18;
            ui::textFit(c, 4, y0, (stack.empty() ? std::string("?") : stack.front().function) + "  " + file + ":" + std::to_string(line), ui::ScreenW - 8, Accent, font::Face::Small);
            for (int r = 0; r < 7; ++r) {
                const int ln = line - 3 + r;
                const int y = y0 + 14 + r * 16;
                if (it == v.runSources.end() || ln < 1 || static_cast<std::size_t>(ln) > it->second.size()) continue;
                if (ln == line) c.rect(0, y, ui::ScreenW, 16, Highlight, true);
                char num[12];
                std::snprintf(num, sizeof num, "%d", ln);
                ui::textRight(c, 26, y + 3, num, Muted, font::Face::Small);
                if (ln == line) ui::text(c, 28, y, ">", Error);
                ui::textFit(c, 38, y, it->second[static_cast<std::size_t>(ln - 1)], ui::ScreenW - 42, Ink);
            }
            const auto vars = v.machine->variables(0);
            int y = y0 + 14 + 7 * 16 + 4;
            c.hLine(0, y - 2, ui::ScreenW, Faint);
            for (std::size_t i = 0; i < vars.size() && y < ui::ContentBottom - 14; ++i, y += 14)
                ui::textFit(c, 4, y, vars[i].name + " = " + vars[i].value + "  (" + vars[i].type + ")", ui::ScreenW - 8, Ink, font::Face::Small);
            break;
        }
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
    case Vars: {
        std::string path = "Variabili";
        for (const auto& p : v.varPath) path += " > " + p.first;
        drawModeHeader(c, path);
        if (v.vars.empty()) ui::textCenter(c, ui::ScreenW / 2, 120, "nessuna variabile", Muted);
        else v.varList.draw(c, gfx::Rect{0, top + 18, ui::ScreenW, ui::ContentBottom - top - 18}, ListRowH);
        break;
    }
    case Lib: {
        drawModeHeader(c, "LIB  " + (libLang(v).empty() ? std::string("tutte") : libLang(v)) + "  (" + std::to_string(v.libsInstalled.size()) + " installate)");
        if (v.libsInstalled.empty()) ui::textCenter(c, ui::ScreenW / 2, 110, "Nessuna libreria: F2 INST", Muted);
        else v.libList.draw(c, gfx::Rect{0, top + 18, ui::ScreenW, ui::ContentBottom - top - 40}, ListRowH);
        if (!v.libStatus.empty()) ui::textFit(c, 4, ui::ContentBottom - 20, v.libStatus, ui::ScreenW - 8, v.installer || v.fetchingIndex ? Accent : Warn, font::Face::Small);
        if (v.installer || v.fetchingIndex) ui::textRight(c, ui::ScreenW - 4, top + 2, "AC: annulla", Warn, font::Face::Small);
        break;
    }
    case Info:
        drawModeHeader(c, v.info.title);
        v.info.draw(c, top + 18, ui::ContentBottom);
        break;
    case Setup: {
        static const char* const keys[6] = {"OK", "", "", "", "MIGR", ""};
        drawFormScreen(c, "SET UP Program", v.setup, ui::ContentBottom, keys);
        v.shell.drawModals(c);
        v.dialog.draw(c);
        return;
    }
    }
    v.shell.fkeys.draw(c, ctx.shift());
    v.shell.drawModals(c);
    v.dialog.draw(c);
}
} // namespace fx::app
