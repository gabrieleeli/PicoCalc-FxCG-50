// RUN-MATRIX: natural (2D) input, history with recall, OPTN catalogue, VARS,
// store arrow, S<->D, Base-N and DMS result views.
#include "apps.hpp"
#include "math_renderer.hpp"
#include "run_matrix.hpp"
#include <algorithm>
#include <cstdio>

namespace fx::app {
using namespace ui::theme;

namespace {
struct CatalogCategory { const char* name; std::vector<const char*> items; };
const std::vector<CatalogCategory>& catalog() {
    static const std::vector<CatalogCategory> c = {
        {"CALC  d/dx \xE2\x88\xAB \xCE\xA3", {"\xE2\x88\xAB dx  (template)", "d/dx  (template)", "\xCE\xA3  (template)", "\xCE\xA0  (template)", "derivative2(", "solve(", "integral(", "sum("}},
        {"PROB  n! nCr", {"!", "nCr(", "nPr(", "rand()", "randint("}},
        {"NUM  Abs Int", {"abs(", "int(", "intg(", "frac(", "round(", "gcd(", "lcm(", "mod("}},
        {"ANGLE  \xC2\xB0 r g", {"\xC2\xB0", "deg(", "rad(", "grad(", "dms(", "pol(", "rec("}},
        {"COMPLEX  i Arg", {"i", "abs(", "arg(", "conj(", "re(", "im("}},
        {"HYP  sinh cosh", {"sinh(", "cosh(", "tanh(", "asinh(", "acosh(", "atanh("}},
        {"LOG  log ln e^", {"log(", "ln(", "exp(", "log2(", "logab (template)"}},
        {"CONST  \xCF\x80 e c G", {"\xCF\x80", "e", "physc", "physg", "physh", "NA", "kB", "qe", "g0", "goldenRatio"}},
        {"BASE-N  Hex Bin", {"▶Dec", "▶Hex", "▶Bin", "▶Oct", "▶DMS", "0x", "0b", "0o"}},
    };
    return c;
}
const std::vector<std::string>& mathTemplates() {
    static const std::vector<std::string> t = {"\xE2\x81\xBF\xE2\x88\x9A\xE2\x96\xA1  radice n-esima", "\xE2\x96\xA1\xC2\xB2  quadrato", "\xE2\x96\xA1\xE2\x81\xBB\xC2\xB9  reciproco",
        "log\xE2\x96\xA1(\xE2\x96\xA1)  log base a", "e^\xE2\x96\xA1", "10^\xE2\x96\xA1", "|\xE2\x96\xA1|  valore assoluto", "d/dx", "\xE2\x88\xAB dx", "\xCE\xA3", "\xCE\xA0", "( )  gruppo"};
    return t;
}
} // namespace

void RunMatrixApp::enter(Context& ctx) { ctx.ws.apply(); browse_ = -1; menu_.close(); }

void RunMatrixApp::insertCatalogItem(Context& ctx, const std::string& item) {
    if (item.find("(template)") != std::string::npos) {
        if (item.rfind("\xE2\x88\xAB", 0) == 0) editor_.insertIntegral();
        else if (item.rfind("d/dx", 0) == 0) editor_.insertDerivative();
        else if (item.rfind("\xCE\xA3", 0) == 0) editor_.insertSummation();
        else if (item.rfind("\xCE\xA0", 0) == 0) editor_.insertProduct();
        else if (item.rfind("logab", 0) == 0) editor_.insertLogBase();
        return;
    }
    if (item.rfind("\xE2\x96\xB6", 0) == 0) {          // ▶Dec / ▶Hex / ... : change the view of the last result
        if (ctx.ws.history.empty()) return;
        auto& e = ctx.ws.history.back();
        const std::string v = item.substr(3);
        e.view = v == "Dec" ? ViewDecimal : v == "Hex" ? ViewHex : v == "Bin" ? ViewBin : v == "Oct" ? ViewOct : ViewDms;
        ctx.markDirty();
        return;
    }
    editor_.insertText(item);
}

void RunMatrixApp::evaluate(Context& ctx) {
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

void RunMatrixApp::key(Context& ctx, const KeyEvent& ev) {
    // --- popup menus
    if (menu_.open) {
        const int choice = menu_.key(ev);
        if (choice < 0) return;
        if (menuKind_ == 1) {
            switch (choice) {
            case 0: editor_.insertNthRoot(3); break;      // index edited afterwards on the CG50; cube root default
            case 1: editor_.insertSquare(); break;
            case 2: editor_.insertPower(); editor_.insertText("-1"); (void)editor_.moveRight(); break;
            case 3: editor_.insertLogBase(); break;
            case 4: editor_.insertExp(); break;
            case 5: editor_.insertTenPower(); break;
            case 6: editor_.insertAbs(); break;
            case 7: editor_.insertDerivative(); break;
            case 8: editor_.insertIntegral(); break;
            case 9: editor_.insertSummation(); break;
            case 10: editor_.insertProduct(); break;
            case 11: editor_.insertGroup(); break;
            default: break;
            }
        } else if (menuKind_ == 2) {
            optnCategory_ = choice;
            std::vector<std::string> items;
            for (const char* s : catalog()[static_cast<std::size_t>(choice)].items) items.emplace_back(s);
            menu_.show(catalog()[static_cast<std::size_t>(choice)].name, std::move(items));
            menuKind_ = 3;
        } else if (menuKind_ == 3) {
            insertCatalogItem(ctx, menu_.items[static_cast<std::size_t>(choice)]);
        } else if (menuKind_ == 4) {
            editor_.insert(static_cast<char>('A' + choice));
        }
        return;
    }
    // --- history browsing (UP from the input line)
    if (browse_ >= 0) {
        const int n = static_cast<int>(ctx.ws.history.size());
        switch (ev.key) {
        case Key::Up: if (browse_ > 0) --browse_; return;
        case Key::Down: if (browse_ + 1 < n) ++browse_; else browse_ = -1; return;
        case Key::Exe: case Key::Right: case Key::Left:
            if (browse_ < n) editor_.setExpression(ctx.ws.history[static_cast<std::size_t>(browse_)].expression);
            browse_ = -1; return;
        case Key::Exit: browse_ = -1; return;
        case Key::Del: case Key::DelForward:
            if (browse_ < n) { ctx.ws.history.erase(ctx.ws.history.begin() + browse_); ctx.markDirty(); }
            browse_ = std::min(browse_, static_cast<int>(ctx.ws.history.size()) - 1);
            return;
        default: browse_ = -1; break;   // any other key returns to editing and is processed below
        }
    }
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
    case Key::Exit: if (!editor_.empty()) editor_.clear(); else ctx.home(); return;
    case Key::AC:
        if (!editor_.empty()) editor_.clear();
        else { ctx.ws.history.clear(); ctx.markDirty(); }
        return;
    case Key::Del: editor_.backspace(); return;
    case Key::DelForward: editor_.eraseForward(); return;
    case Key::Left: editor_.moveLeft(); return;
    case Key::Right: editor_.moveRight(); return;
    case Key::Up:
        if (editor_.cursorPath().empty() || !editor_.moveUp()) {
            if (editor_.cursorPath().empty() && !ctx.ws.history.empty()) browse_ = static_cast<int>(ctx.ws.history.size()) - 1;
        }
        return;
    case Key::Down: editor_.moveDown(); return;
    case Key::Home: editor_.moveHome(); return;
    case Key::End: editor_.moveEnd(); return;
    case Key::Store: editor_.insertSymbol("\xE2\x86\x92"); return;
    case Key::F1: if (shift) editor_.insertNthRoot(3); else editor_.insertFraction(); return;
    case Key::F2: if (shift) editor_.insertSquare(); else editor_.insertRadical(); return;
    case Key::F3: if (shift) editor_.insertText("pi"); else editor_.insertPower(); return;
    case Key::F4: if (shift) editor_.insertSymbol("\xE2\x86\x92"); else { menu_.show("MATH", mathTemplates()); menuKind_ = 1; } return;
    case Key::F5:
        if (ctx.ws.history.empty()) return;
        {
            auto& e = ctx.ws.history.back();
            if (shift) e.view = static_cast<std::uint8_t>(e.view >= ViewOct || e.view < ViewHex ? ViewHex : e.view + 1);
            else e.view = e.view == ViewAuto ? ViewDecimal : ViewAuto;   // S<->D
            ctx.markDirty();
        }
        return;
    case Key::F6: if (shift) editor_.insertText("theta"); else editor_.insertText("Ans"); return;
    case Key::Optn: {
        std::vector<std::string> cats;
        for (const auto& c : catalog()) cats.emplace_back(c.name);
        menu_.show("OPTN", std::move(cats)); menuKind_ = 2; return;
    }
    case Key::Vars: {
        std::vector<std::string> vars;
        for (char v = 'A'; v <= 'Z'; ++v) vars.push_back(std::string(1, v) + " = " + formatReal(ctx.ws.memory.recall(v), ctx.ws.settings.display));
        menu_.show("VARS  A-Z", std::move(vars)); menuKind_ = 4; return;
    }
    default: return;
    }
}

void RunMatrixApp::draw(Context& ctx, gfx::Canvas& c) {
    ui::clear(c);
    ctx.drawStatus(c, "RUN-MATRIX");
    const int top = ui::ContentTop + 2, bottom = ui::ContentBottom - 2, left = 4, width = ui::ScreenW - 8;
    const Settings& s = ctx.ws.settings;

    // Input line layout (with the cursor) and horizontal scrolling.
    const math::CursorTarget cursor{editor_.activeRowPtr(), editor_.cursorIndex()};
    const math::Size inSize = math::measure(editor_.root());
    const int inputH = std::max(18, inSize.height + 6);

    // Blocks from the top: each history entry is expression (left) + result (right).
    struct Block { int h; int entry; bool result; };
    std::vector<Block> blocks;
    const auto& hist = ctx.ws.history;
    std::vector<ast::NodePtr> exprTrees(hist.size()), resultTrees(hist.size());
    for (std::size_t i = 0; i < hist.size(); ++i) {
        NaturalEditor tmp;
        try { tmp.setExpression(hist[i].expression); } catch (...) { tmp.clear(); tmp.insertText("?"); }
        exprTrees[i] = tmp.copyTree();
        resultTrees[i] = resultTree(hist[i], s);
        blocks.push_back({math::measure(*exprTrees[i]).height + 4, static_cast<int>(i), false});
        blocks.push_back({math::measure(*resultTrees[i]).height + 6, static_cast<int>(i), true});
    }
    int total = inputH;
    for (const auto& b : blocks) total += b.h;
    // Scroll so that the input line (or the browsed entry) stays visible.
    int y = std::min(top, bottom - total);
    if (browse_ >= 0) {
        int yy = 0;
        for (const auto& b : blocks) { if (b.entry == browse_ && !b.result) break; yy += b.h; }
        if (top + yy + (y - top) < top) y = top - yy;
    }
    c.setClip({0, top, ui::ScreenW, bottom - top});
    for (const auto& b : blocks) {
        if (y + b.h >= top && y < bottom) {
            const bool sel = b.entry == browse_;
            const auto& tree = b.result ? *resultTrees[static_cast<std::size_t>(b.entry)] : *exprTrees[static_cast<std::size_t>(b.entry)];
            const math::Size sz = math::measure(tree);
            if (sel && !b.result) c.rect(0, y, ui::ScreenW, b.h, Select, true);
            auto l = math::layout(tree, left, y + 2 + sz.baseline, width, b.h, b.result ? math::Align::Right : math::Align::Left);
            math::draw(c, l, sel && !b.result ? SelectInk : Ink, Cursor, Placeholder);
            if (b.result) c.hLine(left, y + b.h - 2, width, Grid);
        }
        y += b.h;
    }
    // Input line.
    auto l = math::layout(editor_.root(), left - scrollX_, y + 3 + inSize.baseline, width + scrollX_ + 2000, inputH, math::Align::Left, &cursor);
    if (l.hasCursor) {
        if (l.cursorX - left > width - 4) scrollX_ += (l.cursorX - left) - (width - 4);
        if (l.cursorX < left) scrollX_ = std::max(0, scrollX_ - (left - l.cursorX) - 8);
        l = math::layout(editor_.root(), left - scrollX_, y + 3 + inSize.baseline, width + scrollX_ + 2000, inputH, math::Align::Left, &cursor);
    }
    c.setClip({left, top, width, bottom - top});
    if (browse_ < 0) math::draw(c, l, Ink, Cursor, Placeholder);
    else math::draw(c, l, Muted, Bg, Placeholder);
    c.resetClip();
    if (scrollX_ > 0) c.rect(0, y + 2, 2, inputH - 4, Accent, true);

    const bool shift = ctx.shift();
    const char* normal[6] = {"a/b", "\xE2\x88\x9A\xE2\x96\xA1", "\xE2\x96\xA1^", "MATH", "S\xE2\x87\x94" "D", "Ans"};
    const char* shifted[6] = {"\xE2\x88\x9B\xE2\x96\xA1", "\xE2\x96\xA1\xC2\xB2", "\xCF\x80", "\xE2\x86\x92", "\xE2\x96\xB6" "BASE", "\xCE\xB8"};
    ui::softKeys(c, shift ? shifted : normal);
    menu_.draw(c);
}
} // namespace fx::app
