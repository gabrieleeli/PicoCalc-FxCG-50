// RUN-MATRIX: natural (2D) input, history with in-place editing and
// recalculation (fx-CG50 history function), exact results with S<->D, mixed
// fractions and repeating decimals, OPTN catalogue, VARS (A-Z and the
// variables shared by Graph/Table), store arrow, Base-N and DMS views.
// Only calculations and results live here: graphs, statistics, tables and
// conversions belong to their own modes (see docs/APPLICATIONS_ARCHITECTURE.md).
#include "apps.hpp"
#include "math_renderer.hpp"
#include "run_matrix.hpp"
#include <algorithm>
#include <cstdio>

namespace fx::app {
using namespace ui::theme;

namespace {
// OPTN categories as on the fx-CG50 (Run-Matrix): items are inserted into the
// input, except the RESULT entries that change the view of the selected result.
struct CatalogCategory { const char* name; std::vector<const char*> items; };
const std::vector<CatalogCategory>& catalog() {
    static const std::vector<CatalogCategory> c = {
        {"CALC  d/dx \xE2\x88\xAB \xCE\xA3", {"\xE2\x88\xAB dx  (template)", "d/dx  (template)", "\xCE\xA3  (template)", "\xCE\xA0  (template)", "derivative2(", "solve(", "integral(", "sum(", "Int\xC3\xB7", "Rmdr"}},
        {"PROB  n! nCr Ran#", {"!", "nCr(", "nPr(", "rand()", "randint(", "rannorm(", "ranbin("}},
        {"NUMERIC  Abs Int", {"abs(", "int(", "intg(", "frac(", "round(", "rndfix(", "gcd(", "lcm(", "mod(", "modexp("}},
        {"ANGLE  \xC2\xB0 r g Pol", {"\xC2\xB0", "deg(", "rad(", "grad(", "dms(", "pol(", "rec("}},
        {"COMPLEX  i Arg", {"i", "abs(", "arg(", "conj(", "re(", "im("}},
        {"HYPERBL  sinh cosh", {"sinh(", "cosh(", "tanh(", "asinh(", "acosh(", "atanh("}},
        {"LOG  log ln e^", {"log(", "ln(", "exp(", "log2(", "logab (template)"}},
        {"LOGIC  And Or Not", {"and", "or", "xor", "not"}},
        {"RELAZIONI  = \xE2\x89\xA0 < >", {"=", "\xE2\x89\xA0", ">", "<", "\xE2\x89\xA5", "\xE2\x89\xA4"}},
        {"CONST  \xCF\x80 e c G", {"\xCF\x80", "e", "physc", "physg", "physh", "NA", "kB", "qe", "g0", "goldenRatio"}},
        {"RISULTATO  S\xE2\x87\x94" "D ab/c", {"\xE2\x96\xB6S\xE2\x87\x94" "D", "\xE2\x96\xB6" "ab/c\xE2\x87\x94" "d/c", "\xE2\x96\xB6Periodo", "\xE2\x96\xB6" "Dec", "\xE2\x96\xB6Hex", "\xE2\x96\xB6" "Bin", "\xE2\x96\xB6Oct", "\xE2\x96\xB6" "DMS"}},
        {"BASE-N  0x 0b 0o", {"0x", "0b", "0o"}},
    };
    return c;
}
const std::vector<std::string>& mathTemplates() {
    static const std::vector<std::string> t = {
        "\xE2\x96\xA1 \xE2\x96\xA1/\xE2\x96\xA1  frazione mista", "\xE2\x81\xBF\xE2\x88\x9A\xE2\x96\xA1  radice n-esima", "\xE2\x88\x9B\xE2\x96\xA1  radice cubica",
        "\xE2\x96\xA1\xC2\xB2  quadrato", "\xE2\x96\xA1\xE2\x81\xBB\xC2\xB9  reciproco", "log\xE2\x96\xA1(\xE2\x96\xA1)  log base a", "e^\xE2\x96\xA1", "10^\xE2\x96\xA1",
        "|\xE2\x96\xA1|  valore assoluto", "d/dx", "\xE2\x88\xAB dx", "\xCE\xA3", "\xCE\xA0", "( )  gruppo", "0.1\xE2\x80\xBE  periodo"};
    return t;
}
// VARS categories: alpha memory, answer memories and the variables owned by
// Graph (V-Window) and Table, read through the workspace.
const char* const kVarsCategories[] = {"A~Z  variabili", "Ans  PreAns", "V-WIN  finestra Graph", "TABLE  intervallo Table"};
struct SharedVar { const char* token; const char* label; };
const SharedVar kVWin[] = {{"xmin", "Xmin"}, {"xmax", "Xmax"}, {"xscl", "Xscl"}, {"ymin", "Ymin"}, {"ymax", "Ymax"},
                           {"yscl", "Yscl"}, {"tmin", "Tmin"}, {"tmax", "Tmax"}, {"tptch", "Tptch"}};
const SharedVar kTable[] = {{"tblstart", "TblStart"}, {"tblend", "TblEnd"}, {"tblpitch", "TblPitch"}};
const char* const kNoExact = "Nessuna altra forma: valore approssimato o intero";
} // namespace

void RunMatrixApp::enter(Context& ctx) { ctx.ws.apply(); browse_ = -1; menu_.close(); menuKind_ = Menu::None; }
void RunMatrixApp::leave(Context&) { if (editIndex_ >= 0) endEdit(true); }

HistoryEntry* RunMatrixApp::target(Context& ctx) {
    auto& h = ctx.ws.history;
    if (h.empty()) return nullptr;
    if (browse_ >= 0 && browse_ < static_cast<int>(h.size())) return &h[static_cast<std::size_t>(browse_)];
    return &h.back();
}

void RunMatrixApp::resultKey(Context& ctx, int kind) {
    HistoryEntry* e = target(ctx);
    if (!e) return;
    const Settings& s = ctx.ws.settings;
    bool changed = false;
    switch (kind) {
    case 0: changed = toggleExactDecimal(*e, s); if (!changed) ctx.toast(kNoExact); break;
    case 1: changed = toggleMixedFraction(*e, s); if (!changed) ctx.toast("Nessuna frazione impropria da convertire"); break;
    case 2: changed = togglePeriodic(*e); if (!changed) ctx.toast("Il periodo richiede un risultato razionale esatto"); break;
    default: {
        static const std::uint8_t views[] = {ViewDecimal, ViewHex, ViewBin, ViewOct, ViewDms};
        e->view = views[std::clamp(kind - 3, 0, 4)];
        changed = true;
        break;
    }
    }
    if (changed) ctx.markDirty();
}

void RunMatrixApp::insertCatalogItem(Context& ctx, const std::string& item) {
    if (item.find("(template)") != std::string::npos) {
        if (item.rfind("\xE2\x88\xAB", 0) == 0) editor_.insertIntegral();
        else if (item.rfind("d/dx", 0) == 0) editor_.insertDerivative();
        else if (item.rfind("\xCE\xA3", 0) == 0) editor_.insertSummation();
        else if (item.rfind("\xCE\xA0", 0) == 0) editor_.insertProduct();
        else if (item.rfind("logab", 0) == 0) editor_.insertLogBase();
        return;
    }
    if (item.rfind("\xE2\x96\xB6", 0) == 0) {          // ▶ entries change the view of the selected result
        const std::string v = item.substr(3);
        const int kind = v.rfind("S", 0) == 0 ? 0 : v.rfind("ab/c", 0) == 0 ? 1 : v == "Periodo" ? 2 :
                         v == "Dec" ? 3 : v == "Hex" ? 4 : v == "Bin" ? 5 : v == "Oct" ? 6 : 7;
        resultKey(ctx, kind);
        return;
    }
    // Operator words and Int÷ are single tokens; relations are symbols.
    if (item == "Int\xC3\xB7" || item == "Rmdr" || item == "and" || item == "or" || item == "xor" || item == "not" ||
        item == "\xE2\x89\xA0" || item == "\xE2\x89\xA5" || item == "\xE2\x89\xA4") { editor_.insertSymbol(item); return; }
    editor_.insertText(item);
}

void RunMatrixApp::evaluate(Context& ctx) {
    if (editIndex_ >= 0) {
        // fx-CG50 history editing: the edited line and every line after it are recalculated.
        const std::size_t failed = editHistoryEntry(ctx.ws, static_cast<std::size_t>(editIndex_), editor_.toExpression());
        const int edited = editIndex_;
        endEdit(true);
        browse_ = -1;
        ctx.markDirty();
        const std::size_t later = ctx.ws.history.size() - static_cast<std::size_t>(edited) - 1;
        if (failed) ctx.toast(std::to_string(failed) + " righe successive con errore");
        else if (later) ctx.toast("Ricalcolate " + std::to_string(later + 1) + " righe");
        return;
    }
    if (editor_.empty()) {
        // EXE on an empty line repeats the previous calculation, like the CG50.
        if (ctx.ws.history.empty()) return;
        editor_.setExpression(ctx.ws.history.back().expression);
    }
    const std::string text = editor_.toExpression();
    (void)runCalculation(ctx.ws, text);
    editor_.clear();
    scrollX_ = 0;
    ctx.markDirty();
}

void RunMatrixApp::beginEdit(Context& ctx, int index) {
    if (index < 0 || index >= static_cast<int>(ctx.ws.history.size())) return;
    pendingInput_ = editor_.copyTree();
    editor_.setExpression(ctx.ws.history[static_cast<std::size_t>(index)].expression);
    editIndex_ = index;
    browse_ = -1;
    scrollX_ = 0;
}
void RunMatrixApp::endEdit(bool restoreInput) {
    editIndex_ = -1;
    if (restoreInput && pendingInput_) editor_.setTree(*pendingInput_);
    else editor_.clear();
    pendingInput_.reset();
    scrollX_ = 0;
}

void RunMatrixApp::menuChoice(Context& ctx, int choice) {
    const auto idx = static_cast<std::size_t>(choice);
    switch (menuKind_) {
    case Menu::Math:
        switch (choice) {
        case 0: editor_.insertMixedFraction(); break;
        case 1: editor_.insertRootTemplate(); break;      // index and radicand are both editable
        case 2: editor_.insertNthRoot(3); break;
        case 3: editor_.insertSquare(); break;
        case 4: editor_.insertPower(); editor_.insertText("-1"); (void)editor_.moveRight(); break;
        case 5: editor_.insertLogBase(); break;
        case 6: editor_.insertExp(); break;
        case 7: editor_.insertTenPower(); break;
        case 8: editor_.insertAbs(); break;
        case 9: editor_.insertDerivative(); break;
        case 10: editor_.insertIntegral(); break;
        case 11: editor_.insertSummation(); break;
        case 12: editor_.insertProduct(); break;
        case 13: editor_.insertGroup(); break;
        case 14: editor_.insertPeriod(); break;
        default: break;
        }
        menuKind_ = Menu::None;
        break;
    case Menu::OptnCategories: {
        optnCategory_ = choice;
        std::vector<std::string> items;
        for (const char* s : catalog()[idx].items) items.emplace_back(s);
        menu_.show(catalog()[idx].name, std::move(items));
        menuKind_ = Menu::OptnItems;
        break;
    }
    case Menu::OptnItems: {
        const std::string item = menu_.items[idx];
        menuKind_ = Menu::None;
        insertCatalogItem(ctx, item);
        break;
    }
    case Menu::VarsCategories: {
        varsCategory_ = choice;
        const CalculatorMemory& m = ctx.ws.memory;
        const DisplayFormat f = ctx.ws.settings.display;
        std::vector<std::string> items;
        if (choice == 0) {
            for (char v = 'A'; v <= 'Z'; ++v) {
                const ExactSlot& e = m.exactSlot(v);
                const bool exactShown = e.valid && !e.decimal && !e.value.isZero() && exact::fitsRelaxed(e.value);
                items.push_back(std::string(1, v) + " = " + (exactShown ? exact::toText(e.value, false) : formatReal(m.recall(v), f)));
            }
        } else if (choice == 1) {
            items = {"Ans = " + formatValue({m.answer(), m.answerImag()}, ctx.ws.settings.complex, f),
                     "PreAns = " + formatValue({m.previousAnswer(), m.previousAnswerImag()}, ctx.ws.settings.complex, f)};
        } else if (choice == 2) {
            for (const auto& v : kVWin) { double d = 0; (void)ctx.ws.resolve(v.token, d); items.push_back(std::string(v.label) + " = " + formatReal(d, f)); }
        } else {
            for (const auto& v : kTable) { double d = 0; (void)ctx.ws.resolve(v.token, d); items.push_back(std::string(v.label) + " = " + formatReal(d, f)); }
        }
        menu_.show(kVarsCategories[idx], std::move(items));
        menuKind_ = Menu::VarsItems;
        break;
    }
    case Menu::VarsItems:
        menuKind_ = Menu::None;
        if (varsCategory_ == 0) editor_.insert(static_cast<char>('A' + choice));
        else if (varsCategory_ == 1) editor_.insertText(choice == 0 ? "Ans" : "PreAns");
        else if (varsCategory_ == 2) editor_.insertSymbol(kVWin[idx].token);
        else editor_.insertSymbol(kTable[idx].token);
        break;
    case Menu::None: break;
    }
}

bool RunMatrixApp::browseKey(Context& ctx, const KeyEvent& ev) {
    auto& hist = ctx.ws.history;
    const int n = static_cast<int>(hist.size());
    if (browse_ >= n) browse_ = n - 1;
    if (browse_ < 0) return false;
    const bool shift = ctx.shift();
    if (ev.is(Key::Char) && ev.ctrl && (ev.ch == 'c' || ev.ch == 'C')) {
        NaturalEditor tmp;
        tmp.setExpression(hist[static_cast<std::size_t>(browse_)].expression);
        ctx.clipboard = tmp.copyTree();
        ctx.toast("Espressione copiata");
        return true;
    }
    switch (ev.key) {
    case Key::Up: if (browse_ > 0) --browse_; return true;
    case Key::Down: if (browse_ + 1 < n) ++browse_; else browse_ = -1; return true;
    case Key::Exe: case Key::F1: beginEdit(ctx, browse_); return true;          // edit in place and recalculate
    case Key::F2: case Key::Left: case Key::Right:                             // reuse the expression in the input line
        editor_.setExpression(hist[static_cast<std::size_t>(browse_)].expression);
        browse_ = -1; return true;
    case Key::F3: {                                                            // reuse the result (exact when possible)
        const std::string v = resultExpression(hist[static_cast<std::size_t>(browse_)]);
        browse_ = -1;
        NaturalEditor tmp;
        tmp.setExpression(v);
        editor_.paste(tmp.root());
        return true;
    }
    case Key::F4: resultKey(ctx, 2); return true;
    case Key::F5: resultKey(ctx, shift ? 1 : 0); return true;
    case Key::F6: case Key::Del: case Key::DelForward:
        hist.erase(hist.begin() + browse_); ctx.markDirty();
        browse_ = std::min(browse_, static_cast<int>(hist.size()) - 1);
        return true;
    case Key::Exit: case Key::AC: browse_ = -1; return true;
    default: browse_ = -1; return false;   // other keys go back to the input line and are handled there
    }
}

void RunMatrixApp::key(Context& ctx, const KeyEvent& ev) {
    // --- popup menus
    if (menu_.open) {
        const int choice = menu_.key(ev);
        if (choice == -2) menuKind_ = Menu::None;
        if (choice < 0) return;
        menuChoice(ctx, choice);
        return;
    }
    // --- history browsing (UP from the input line)
    if (browse_ >= 0 && browseKey(ctx, ev)) return;
    const bool shift = ctx.shift();
    if (ev.is(Key::Char) && ev.ctrl) {
        switch (ev.ch) {
        case 'c': case 'C': ctx.clipboard = editor_.copyTree(); ctx.toast("Copiato"); return;
        case 'v': case 'V': if (ctx.clipboard) editor_.paste(*ctx.clipboard); return;
        case 'z': case 'Z': if (!editor_.undo()) ctx.toast("Niente da annullare"); return;
        case 'y': case 'Y': if (!editor_.redo()) ctx.toast("Niente da ripetere"); return;
        default: return;
        }
    }
    switch (ev.key) {
    case Key::Char:
        try { editor_.insert(ev.ch); }
        catch (const std::invalid_argument&) { ctx.toast("Carattere non ammesso"); }
        return;
    case Key::Exe: evaluate(ctx); return;
    case Key::Exit:
        if (editIndex_ >= 0) { const int i = editIndex_; endEdit(true); browse_ = i; return; }   // cancel the edit
        if (!editor_.empty()) editor_.clear(); else ctx.home();
        return;
    case Key::AC:
        if (editIndex_ >= 0) { endEdit(true); return; }
        if (!editor_.empty()) editor_.clear();
        else { ctx.ws.history.clear(); ctx.markDirty(); }
        return;
    case Key::Del: editor_.backspace(); return;
    case Key::DelForward: editor_.eraseForward(); return;
    case Key::Left: editor_.moveLeft(); return;
    case Key::Right: editor_.moveRight(); return;
    case Key::Up:
        if (editor_.cursorPath().empty() || !editor_.moveUp()) {
            if (editIndex_ < 0 && editor_.cursorPath().empty() && !ctx.ws.history.empty()) browse_ = static_cast<int>(ctx.ws.history.size()) - 1;
        }
        return;
    case Key::Down: editor_.moveDown(); return;
    case Key::Home: editor_.moveHome(); return;
    case Key::End: editor_.moveEnd(); return;
    case Key::Store: editor_.insertSymbol("\xE2\x86\x92"); return;
    case Key::F1: if (shift) editor_.insertMixedFraction(); else editor_.insertFraction(); return;
    case Key::F2: if (shift) editor_.insertSquare(); else editor_.insertRadical(); return;
    case Key::F3: if (shift) editor_.insertText("pi"); else editor_.insertPower(); return;
    case Key::F4: if (shift) editor_.insertSymbol("\xE2\x86\x92"); else { menu_.show("MATH", mathTemplates()); menuKind_ = Menu::Math; } return;
    case Key::F5: resultKey(ctx, shift ? 1 : 0); return;          // S<->D, SHIFT: ab/c <-> d/c
    case Key::F6: if (shift) editor_.insertText("theta"); else editor_.insertText("Ans"); return;
    case Key::Optn: {
        std::vector<std::string> cats;
        for (const auto& c : catalog()) cats.emplace_back(c.name);
        menu_.show("OPTN", std::move(cats)); menuKind_ = Menu::OptnCategories; return;
    }
    case Key::Vars: {
        std::vector<std::string> cats(std::begin(kVarsCategories), std::end(kVarsCategories));
        menu_.show("VARS", std::move(cats)); menuKind_ = Menu::VarsCategories; return;
    }
    default: return;
    }
}

void RunMatrixApp::draw(Context& ctx, gfx::Canvas& c) {
    ui::clear(c);
    ctx.drawStatus(c, editIndex_ >= 0 ? "RUN-MATRIX  modifica" : "RUN-MATRIX");
    const int top = ui::ContentTop + 2, bottom = ui::ContentBottom - 2, left = 4, width = ui::ScreenW - 8;
    const Settings& s = ctx.ws.settings;
    const auto& hist = ctx.ws.history;
    const math::CursorTarget cursor{editor_.activeRowPtr(), editor_.cursorIndex()};
    const math::Size edSize = math::measure(editor_.root());
    const int editorH = std::max(18, edSize.height + 6);

    // Blocks from the top: each history entry is expression (left) + result (right),
    // then the input line (absent while an entry is edited in place).
    struct Block { int h; int entry; bool result; bool live; };
    std::vector<Block> blocks;
    std::vector<ast::NodePtr> exprTrees(hist.size()), resultTrees(hist.size());
    for (std::size_t i = 0; i < hist.size(); ++i) {
        const bool live = editIndex_ == static_cast<int>(i);
        if (!live) {
            NaturalEditor tmp;
            try { tmp.setExpression(hist[i].expression); } catch (...) { tmp.clear(); tmp.insertText("?"); }
            exprTrees[i] = tmp.copyTree();
        }
        resultTrees[i] = resultTree(hist[i], s);
        blocks.push_back({live ? editorH : math::measure(*exprTrees[i]).height + 4, static_cast<int>(i), false, live});
        blocks.push_back({math::measure(*resultTrees[i]).height + 6, static_cast<int>(i), true, false});
    }
    if (editIndex_ < 0) blocks.push_back({editorH, -1, false, true});
    // Scroll so that the active block (input, edited or browsed entry) is visible.
    int total = 0, focusY = 0, focusH = 0;
    const int focusEntry = editIndex_ >= 0 ? editIndex_ : browse_;
    for (const auto& b : blocks) {
        if ((focusEntry < 0 && b.entry == -1) || (focusEntry >= 0 && b.entry == focusEntry)) {
            if (focusH == 0) focusY = total;
            focusH = total + b.h - focusY;
        }
        total += b.h;
    }
    int offset = std::min(0, (bottom - top) - total);
    if (top + offset + focusY < top) offset = -focusY;
    if (top + offset + focusY + focusH > bottom) offset = (bottom - top) - (focusY + focusH);
    int y = top + offset;
    for (const auto& b : blocks) {
        const bool sel = b.entry >= 0 && b.entry == browse_;
        if (b.live) {
            // Input line or entry edited in place, with cursor and horizontal scrolling.
            const int baseY = y + 3 + edSize.baseline;
            auto l = math::layout(editor_.root(), left - scrollX_, baseY, width + scrollX_ + 2000, editorH, math::Align::Left, &cursor);
            if (l.hasCursor) {
                if (l.cursorX - left > width - 4) scrollX_ += (l.cursorX - left) - (width - 4);
                if (l.cursorX < left) scrollX_ = std::max(0, scrollX_ - (left - l.cursorX) - 8);
                l = math::layout(editor_.root(), left - scrollX_, baseY, width + scrollX_ + 2000, editorH, math::Align::Left, &cursor);
            }
            if (y + b.h >= top && y < bottom) {
                c.setClip({0, top, ui::ScreenW, bottom - top});
                if (b.entry >= 0) c.rect(0, y, ui::ScreenW, b.h, SelectSoft, true);
                c.setClip({left, std::max(top, y), width, std::min(bottom, y + b.h) - std::max(top, y)});
                if (browse_ < 0) math::draw(c, l, Ink, Cursor, Placeholder);
                else math::draw(c, l, Muted, Bg, Placeholder);
                c.resetClip();
                if (scrollX_ > 0) c.rect(0, y + 2, 2, b.h - 4, Accent, true);
            }
        } else if (y + b.h >= top && y < bottom) {
            c.setClip({0, top, ui::ScreenW, bottom - top});
            const auto& tree = b.result ? *resultTrees[static_cast<std::size_t>(b.entry)] : *exprTrees[static_cast<std::size_t>(b.entry)];
            const math::Size sz = math::measure(tree);
            if (sel && !b.result) c.rect(0, y, ui::ScreenW, b.h, Select, true);
            const HistoryEntry& h = hist[static_cast<std::size_t>(b.entry)];
            const bool failed = b.result && !h.error.empty() && h.error[0] != '=';
            auto l = math::layout(tree, left, y + 2 + sz.baseline, width, b.h, b.result ? math::Align::Right : math::Align::Left);
            math::draw(c, l, sel && !b.result ? SelectInk : failed ? Error : Ink, Cursor, Placeholder);
            if (b.result) c.hLine(left, y + b.h - 2, width, Grid);
            c.resetClip();
        }
        y += b.h;
    }
    // History length indicator (fx-CG50 scroll bar).
    if (total > bottom - top) ui::scrollBar(c, ui::ScreenW - 3, top, bottom - top, total, bottom - top, -offset);

    const bool shift = ctx.shift();
    if (browse_ >= 0) {
        const char* keys[6] = {"MODIF", "COPIA", "RIS", "PERIOD", shift ? "ab/c" : "S\xE2\x87\x94" "D", "ELIM"};
        ui::softKeys(c, keys);
    } else {
        const char* normal[6] = {"a/b", "\xE2\x88\x9A\xE2\x96\xA1", "\xE2\x96\xA1^", "MATH", "S\xE2\x87\x94" "D", "Ans"};
        const char* shifted[6] = {"a b/c", "\xE2\x96\xA1\xC2\xB2", "\xCF\x80", "\xE2\x86\x92", "ab/c\xE2\x87\x94" "d/c", "\xCE\xB8"};
        ui::softKeys(c, shift ? shifted : normal);
    }
    menu_.draw(c);
}
} // namespace fx::app
