// SPREADSHEET (v1.17.0, modalita' Base; solo Pimoroni Pico Plus 2 W).
// Funzioni minime del capitolo 9 della fx-CG50 descritte dal manuale (nessun
// codice o testo copiato): griglia A-Z x 999 con cursore e scorrimento, celle
// con numeri, testo e formule (=A1+B2*2, $A$1, A1:A10 con Sum, Mean, Min,
// Max, Median del motore unico), ricalcolo automatico a passi (AC = Break),
// "Circular ERROR", CUT/COPY/PASTE di celle e blocchi, FILL, colonna <-> List
// e file .pcsheet. Il nucleo e' in spreadsheet.cpp; qui tasti e disegno.
#include "apps.hpp"
#include "line_editor.hpp"
#include "spreadsheet.hpp"
#include <algorithm>
#include <cctype>
#include <cstdio>

namespace fx::app {
using namespace ui::theme;
namespace {
enum Act : int { ActFile = 0, ActNew, ActOpen, ActSaveAs, ActCut, ActCopy, ActPaste, ActFill, ActSelect, ActDelete, ActColToList, ActListToCol,
                 ActRecalc, ActJump, ActEdit };
const FKeyPage kFilePage{{{"NEW", ActNew}, {"OPEN", ActOpen}, {"SAVE\xC2\xB7" "AS", ActSaveAs}, {}, {}, {}}};
const FKeyMenu kFileMenu{&kFilePage, 1, nullptr};
const FKeyPage kEditPage{{{"CUT", ActCut}, {"COPY", ActCopy}, {"PASTE", ActPaste}, {"FILL", ActFill}, {"SELECT", ActSelect}, {}}};
const FKeyMenu kEditMenu{&kEditPage, 1, nullptr};
const FKeyPage kListPage{{{"COL\xE2\x86\x92L", ActColToList}, {"L\xE2\x86\x92" "COL", ActListToCol}, {}, {}, {}, {}}};
const FKeyMenu kListMenu{&kListPage, 1, nullptr};
const FKeyPage kMainPage{{{"FILE", -1, &kFileMenu}, {"EDIT", -1, &kEditMenu}, {"DELETE", ActDelete}, {"LIST", -1, &kListMenu}, {"CALC", ActRecalc}, {"JUMP", ActJump}}};
const FKeyMenu kMainMenu{&kMainPage, 1, nullptr};
enum Ask : int { AskNone = 0, AskJump, AskColToList, AskListToCol, AskFill };
enum Confirm : int { ConfirmNew = 1, ConfirmDelete };
constexpr int HeadW = 30, CellW = 58, RowH = 18, VisibleCols = 5;
} // namespace

struct SpreadsheetApp::View {
    ModeShell shell;
    TypedFileMenu files{files::FileType::Spreadsheet};
    LineEditor edit{sheet::MaxInput};
    bool editing{false};
    bool selecting{false};
    sheet::Ref anchor{};
    sheet::Clipboard clip;
    step::Runner runner;
    std::unique_ptr<sheet::Recalc> recalc;
    LineEditor askLine{8};
};

SpreadsheetApp::SpreadsheetApp() = default;
SpreadsheetApp::~SpreadsheetApp() = default;

namespace {
sheet::State& st(Context& ctx) { return ctx.ws.sheet; }
sheet::Block block(const SpreadsheetApp::View& v, const sheet::State& s) {
    return v.selecting ? sheet::Block{v.anchor, s.cursor} : sheet::Block{s.cursor, s.cursor};
}
void startRecalc(Context& ctx, SpreadsheetApp::View& v) {
    v.runner.reset();
    v.recalc = std::make_unique<sheet::Recalc>(st(ctx).sheet, ctx.ws.evalContext());
    v.runner.start(*v.recalc, step::Runner::Config{200, 25});
}
void changed(Context& ctx, SpreadsheetApp::View& v) {
    ctx.markDirty();
    if (st(ctx).autoCalc) startRecalc(ctx, v);
}
std::string cellText(Context& ctx, const sheet::Cell* c) {
    if (!c) return {};
    if (c->kind == sheet::Kind::Text) return c->input[0] == '"' ? c->input.substr(1) : c->input;
    if (c->kind == sheet::Kind::Formula) {
        if (!c->error.empty()) return c->error.find("Circular") != std::string::npos ? "Circ" : "ERR";
        if (c->stale) return "...";
    }
    return fmt(ctx, c->value);
}
void scrollToCursor(sheet::State& s, int visibleRows) {
    if (s.cursor.row < s.top) s.top = s.cursor.row;
    if (s.cursor.row >= s.top + visibleRows) s.top = s.cursor.row - visibleRows + 1;
    if (s.cursor.col < s.left) s.left = s.cursor.col;
    if (s.cursor.col >= s.left + VisibleCols) s.left = s.cursor.col - VisibleCols + 1;
    s.top = std::clamp(s.top, 1, sheet::Rows);
    s.left = std::clamp(s.left, 0, sheet::Columns - VisibleCols);
}
int visibleRows() { return (ui::ContentBottom - ui::ContentTop - 18 - 20) / RowH; }
void commit(Context& ctx, SpreadsheetApp::View& v) {
    sheet::State& s = st(ctx);
    s.sheet.set(s.cursor, v.edit.text());             // Syntax ERROR / Memory ERROR: la cella resta com'era
    v.editing = false;
    changed(ctx, v);
}
} // namespace

void SpreadsheetApp::enter(Context& ctx) {
    v_ = std::make_unique<View>();
    v_->shell.fkeys.set(&kMainMenu);
    startRecalc(ctx, *v_);                            // valori aggiornati (A-Z e liste possono essere cambiate)
}
void SpreadsheetApp::leave(Context&) { v_.reset(); }
std::string SpreadsheetApp::screenName() const {
    if (!v_) return "closed";
    if (v_->files.active()) return "file";
    if (v_->editing) return "edit";
    return v_->selecting ? "select" : "grid";
}
bool SpreadsheetApp::tick(Context& ctx) {
    if (!v_ || !v_->runner.running()) return false;
    v_->runner.tick([&ctx] { return ctx.now(); });
    if (!v_->runner.running()) v_->recalc.reset();
    return true;
}

void SpreadsheetApp::key(Context& ctx, const KeyEvent& ev) {
    if (!v_) enter(ctx);
    View& v = *v_;
    sheet::State& s = st(ctx);
    if (v.runner.running() && ev.is(Key::AC)) { v.runner.interrupt(); throw MathError("Break: ricalcolo interrotto"); }
    if (v.files.active()) {
        const int r = v.files.key(ev);
        if (r != 1) return;
        if (v.files.purpose() == TypedFileMenu::Purpose::Open) {
            const auto data = v.files.load(ctx, v.files.file());
            sheet::Sheet loaded;
            if (!sheet::decodeFile(data.data(), data.size(), loaded)) throw MathError("Data ERROR: foglio danneggiato");
            s.sheet = std::move(loaded);
            s.file = v.files.file();
            s.cursor = {0, 1}; s.top = 1; s.left = 0;
            changed(ctx, v);
        } else {
            v.files.save(ctx, v.files.file(), sheet::encodeFile(s.sheet));
            s.file = v.files.file();
            ctx.markDirty();
            ctx.toast("Salvato " + s.file);
        }
        return;
    }
    const bool modal = v.shell.modalKey(ev,
        [&](int tag, bool yes) {
            if (!yes) return;
            if (tag == ConfirmNew) { s.sheet.clearAll(); s.file.clear(); s.cursor = {0, 1}; s.top = 1; s.left = 0; }
            else { sheet::clearBlock(s.sheet, block(v, s)); v.selecting = false; }
            changed(ctx, v);
        },
        [&](int tag) {
            const std::string text = v.shell.ask.text();
            if (tag == AskJump) {
                sheet::Ref r;
                std::string t = text;
                for (char& c : t) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                if (!sheet::parseRef(t, r)) throw MathError("Range ERROR: cella A1...Z999");
                s.cursor = r;
            } else if (tag == AskColToList || tag == AskListToCol) {
                const double n = evalNumber(ctx, text);
                if (n < 1 || n > 26 || n != static_cast<int>(n)) throw MathError("Argument ERROR: List 1...26");
                const auto li = static_cast<std::size_t>(n) - 1;
                if (tag == AskColToList) {
                    ctx.ws.setList(li, sheet::columnValues(s.sheet, s.cursor.col, 1, sheet::Rows));
                    ctx.toast("Colonna " + std::string(1, static_cast<char>('A' + s.cursor.col)) + " \xE2\x86\x92 List " + std::to_string(li + 1));
                } else {
                    sheet::setColumn(s.sheet, s.cursor.col, s.cursor.row, ctx.ws.lists[li]);
                    changed(ctx, v);
                }
                ctx.markDirty();
            }
        },
        [&](int, int) {});
    if (modal) return;
    if (v.editing) {
        if (ev.is(Key::Exe)) { commit(ctx, v); if (s.cursor.row < sheet::Rows) ++s.cursor.row; return; }
        if (ev.is(Key::Exit)) { v.editing = false; return; }
        if (ev.is(Key::Up) || ev.is(Key::Down)) {
            commit(ctx, v);
            s.cursor.row = std::clamp(s.cursor.row + (ev.is(Key::Up) ? -1 : 1), 1, sheet::Rows);
            return;
        }
        (void)v.edit.key(ev);
        return;
    }
    if (ev.is(Key::Setup)) {
        // SET UP della modalita': Auto Calc On/Off.
        s.autoCalc = !s.autoCalc;
        ctx.markDirty();
        ctx.toast(std::string("Auto Calc: ") + (s.autoCalc ? "On" : "Off"));
        if (s.autoCalc) startRecalc(ctx, v);
        return;
    }
    const int action = v.shell.fkeys.key(ev, ctx.shift());
    switch (action) {
    case FKeyBar::Handled: return;
    case ActNew: v.shell.fkeys.set(&kMainMenu); v.shell.confirm.open("Nuovo foglio? (le celle non salvate si perdono)", ConfirmNew); return;
    case ActOpen: v.shell.fkeys.set(&kMainMenu); v.files.openList(ctx); return;
    case ActSaveAs: v.shell.fkeys.set(&kMainMenu); v.files.openSaveAs(ctx, TypedFileMenu::stem(s.file)); return;
    case ActCut: case ActCopy:
        sheet::copyBlock(s.sheet, block(v, s), action == ActCut, v.clip);
        v.selecting = false;
        v.shell.fkeys.set(&kMainMenu);
        ctx.toast(action == ActCut ? "CUT: scegli dove incollare" : "COPY: scegli dove incollare");
        return;
    case ActPaste:
        sheet::paste(s.sheet, v.clip, s.cursor);
        v.shell.fkeys.set(&kMainMenu);
        changed(ctx, v);
        return;
    case ActFill: {
        // FILL: la cella attuale (il suo testo) in tutto il blocco selezionato.
        const sheet::Cell* c = s.sheet.cell(v.anchor);
        if (!v.selecting) throw MathError("Argument ERROR: FILL vuole un blocco (EDIT > SELECT)");
        sheet::fill(s.sheet, block(v, s), c ? c->input : std::string());
        v.selecting = false;
        v.shell.fkeys.set(&kMainMenu);
        changed(ctx, v);
        return;
    }
    case ActSelect: v.selecting = !v.selecting; v.anchor = s.cursor; v.shell.fkeys.set(&kMainMenu); return;
    case ActDelete:
        if (v.selecting) v.shell.confirm.open("Cancellare il blocco?", ConfirmDelete);
        else { s.sheet.clear(s.cursor); changed(ctx, v); }
        return;
    case ActColToList: v.shell.fkeys.set(&kMainMenu); v.shell.ask.open("Colonna \xE2\x86\x92 List n", AskColToList); return;
    case ActListToCol: v.shell.fkeys.set(&kMainMenu); v.shell.ask.open("List n \xE2\x86\x92 colonna (dalla cella)", AskListToCol); return;
    case ActRecalc: startRecalc(ctx, v); return;
    case ActJump: v.shell.ask.open("JUMP: cella (A1...Z999)", AskJump); return;
    default: break;
    }
    switch (ev.key) {
    case Key::Up: s.cursor.row = std::max(1, s.cursor.row - 1); return;
    case Key::Down: s.cursor.row = std::min(sheet::Rows, s.cursor.row + 1); return;
    case Key::Left: s.cursor.col = std::max(0, s.cursor.col - 1); return;
    case Key::Right: s.cursor.col = std::min(sheet::Columns - 1, s.cursor.col + 1); return;
    case Key::PageUp: s.cursor.row = std::max(1, s.cursor.row - visibleRows()); return;
    case Key::PageDown: s.cursor.row = std::min(sheet::Rows, s.cursor.row + visibleRows()); return;
    case Key::Home: s.cursor = {0, 1}; return;
    case Key::Exit:
        if (v.selecting) { v.selecting = false; return; }
        if (v.shell.fkeys.inSubmenu()) { v.shell.fkeys.back(); return; }
        ctx.home();
        return;
    case Key::Del: case Key::DelForward: s.sheet.clear(s.cursor); changed(ctx, v); return;
    case Key::Exe: {
        const sheet::Cell* c = s.sheet.cell(s.cursor);
        v.edit.set(c ? c->input : std::string());
        v.editing = true;
        return;
    }
    case Key::Char:
        if (ev.ctrl) {
            if (ev.ch == 'c' || ev.ch == 'x') { sheet::copyBlock(s.sheet, block(v, s), ev.ch == 'x', v.clip); v.selecting = false; }
            else if (ev.ch == 'v') { sheet::paste(s.sheet, v.clip, s.cursor); changed(ctx, v); }
            return;
        }
        v.edit.clear();
        v.editing = true;
        (void)v.edit.key(ev);
        return;
    default: return;
    }
}

void SpreadsheetApp::draw(Context& ctx, gfx::Canvas& c) {
    if (!v_) enter(ctx);
    View& v = *v_;
    sheet::State& s = st(ctx);
    ui::clear(c);
    ctx.drawStatus(c, "SPREADSHEET");
    const int rows = visibleRows();
    scrollToCursor(s, rows);
    const int top = ui::ContentTop;
    // intestazioni di colonna
    c.rect(0, top, ui::ScreenW, 18, StatusBg, true);
    const sheet::Block sel = block(v, s);
    for (int k = 0; k < VisibleCols; ++k) {
        const int col = s.left + k, x = HeadW + k * CellW;
        const char name[2] = {static_cast<char>('A' + col), 0};
        if (col == s.cursor.col) c.rect(x, top, CellW, 18, SelectSoft, true);
        ui::textCenter(c, x + CellW / 2, top + 1, name, Ink);
        c.vLine(x, top, 18 + rows * RowH, Faint);
    }
    for (int r = 0; r < rows; ++r) {
        const int row = s.top + r, y = top + 18 + r * RowH;
        char num[6];
        std::snprintf(num, sizeof num, "%d", row);
        c.rect(0, y, HeadW, RowH, row == s.cursor.row ? SelectSoft : StatusBg, true);
        ui::textRight(c, HeadW - 2, y + 2, num, Muted, font::Face::Small);
        c.hLine(0, y + RowH - 1, ui::ScreenW, Grid);
        for (int k = 0; k < VisibleCols; ++k) {
            const sheet::Ref ref{s.left + k, row};
            const int x = HeadW + k * CellW;
            const bool cur = ref == s.cursor, inSel = v.selecting && sel.contains(ref);
            if (inSel) c.rect(x + 1, y, CellW - 1, RowH - 1, Highlight, true);
            if (cur) { c.rect(x + 1, y, CellW - 1, RowH - 1, Select, false); c.rect(x + 2, y + 1, CellW - 3, RowH - 3, Select, false); }
            const sheet::Cell* cell = s.sheet.cell(ref);
            if (!cell) continue;
            const std::string t = cellText(ctx, cell);
            const bool num = cell->kind != sheet::Kind::Text;
            const std::uint16_t ink = cell->kind == sheet::Kind::Formula && !cell->error.empty() ? Error : Ink;
            if (num) {
                std::string shown = t;
                while (shown.size() > 1 && font::measure(font::Face::Small, shown.c_str()).width > CellW - 4) shown.pop_back();
                ui::textRight(c, x + CellW - 3, y + 3, shown.c_str(), ink, font::Face::Small);
            } else {
                ui::textFit(c, x + 3, y + 3, t, CellW - 5, ink, font::Face::Small);
            }
        }
    }
    // riga della cella: riferimento e testo (o l'editor)
    const int ly = ui::ContentBottom - 20;
    c.rect(0, ly, ui::ScreenW, 20, Bg, true);
    c.hLine(0, ly, ui::ScreenW, StatusLine);
    const std::string ref = sheet::refName(s.cursor) + (v.selecting ? ":" + sheet::refName(v.anchor) : "") + (s.autoCalc ? "" : " M");
    ui::text(c, 3, ly + 3, ref.c_str(), Accent, font::Face::Small);
    if (v.editing) {
        v.edit.draw(c, 56, ly + 2, ui::ScreenW - 60, Ink);
    } else if (const sheet::Cell* cell = s.sheet.cell(s.cursor)) {
        std::string line = cell->input;
        if (cell->kind == sheet::Kind::Formula && !cell->error.empty()) line += "   " + cell->error;
        ui::textFit(c, 56, ly + 3, line, ui::ScreenW - 60, Ink, font::Face::Small);
    }
    if (v.runner.running()) ui::textRight(c, ui::ScreenW - 3, ly + 3, (std::to_string(std::max(0, v.runner.percent())) + "%").c_str(), Warn, font::Face::Small);
    v.shell.fkeys.draw(c, ctx.shift());
    v.shell.drawModals(c);
    v.files.draw(c);
}
} // namespace fx::app
