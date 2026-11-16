// MathField (v1.12.0): campo di input Math/Line condiviso, con lo stesso
// editor naturale (NaturalEditor) e lo stesso disegno 2D (math_renderer) della
// riga di input di Run-Matrix. Lo usano le celle dei coefficienti di EQUATION e
// l'equazione del Solver (manuale fx-CG50 1-29: input naturale nel Solver).
#include "apps.hpp"
#include "math_renderer.hpp"
#include <algorithm>

namespace fx::app {
using namespace ui::theme;

namespace {
// Menu MATH della PicoCalc (i modelli che la fx-CG50 ha su tasti dedicati).
const std::vector<std::string>& mathItems() {
    static const std::vector<std::string> items = {
        "\xE2\x96\xA1/\xE2\x96\xA1  frazione", "\xE2\x96\xA1 \xE2\x96\xA1/\xE2\x96\xA1  frazione mista", "\xE2\x88\x9A\xE2\x96\xA1  radice quadrata",
        "\xE2\x81\xBF\xE2\x88\x9A\xE2\x96\xA1  radice n-esima", "\xE2\x96\xA1\xC2\xB2  quadrato", "\xE2\x96\xA1^\xE2\x96\xA1  potenza", "\xE2\x96\xA1\xE2\x81\xBB\xC2\xB9  reciproco",
        "|\xE2\x96\xA1|  valore assoluto", "log\xE2\x96\xA1(\xE2\x96\xA1)  log base a", "e^\xE2\x96\xA1", "10^\xE2\x96\xA1", "\xCF\x80", "i  unita' immaginaria",
        "Ans", "( )  gruppo"};
    return items;
}
math::Palette fieldPalette() {
    math::Palette p;
    p.ink = Ink; p.cursor = Cursor; p.placeholder = Placeholder;
    for (int i = 0; i < 5; ++i) p.paren[i] = Paren[i];
    p.selection = Highlight; p.error = ErrorHighlight;
    return p;
}
} // namespace

void MathField::start(Context& ctx, const std::string& expression) {
    const bool line = ctx.ws.settings.inputMode == Settings::InputMode::Line;
    if (editor_.linear() != line) editor_.setLinear(line);
    editor_.clear();
    if (!expression.empty()) editor_.setExpression(expression);
    editor_.clearErrorMark();
    menu_.close();
    scroll_ = 0;
}

void MathField::markError(const ErrorLocation& where) {
    if (where.known()) (void)editor_.markError(static_cast<std::size_t>(where.position), static_cast<std::size_t>(where.length));
}

bool MathField::key(Context& ctx, const KeyEvent& ev) {
    if (menu_.open) {
        const int choice = menu_.key(ev);
        if (choice < 0) return true;
        switch (choice) {
        case 0: editor_.insertFraction(); break;
        case 1: editor_.insertMixedFraction(); break;
        case 2: editor_.insertRadical(); break;
        case 3: editor_.insertRootTemplate(); break;
        case 4: editor_.insertSquare(); break;
        case 5: editor_.insertPower(); break;
        case 6: editor_.insertPower(); editor_.insertText("-1"); (void)editor_.moveRight(); break;
        case 7: editor_.insertAbs(); break;
        case 8: editor_.insertLogBase(); break;
        case 9: editor_.insertExp(); break;
        case 10: editor_.insertTenPower(); break;
        case 11: editor_.insertText("pi"); break;
        case 12: editor_.insert('i'); break;
        case 13: editor_.insertText("Ans"); break;
        default: editor_.insertGroup(); break;
        }
        return true;
    }
    editor_.clearErrorMark();
    const bool shift = ctx.shift();
    if (ev.is(Key::Char) && ev.ctrl) {
        switch (ev.ch) {
        case 'c': case 'C': ctx.clipboard = editor_.copyTree(); ctx.toast("Copiato"); return true;
        case 'x': case 'X': ctx.clipboard = editor_.copyTree(); editor_.clear(); ctx.toast("Tagliato"); return true;
        case 'v': case 'V': if (ctx.clipboard) editor_.paste(*ctx.clipboard); return true;
        case 'z': case 'Z': if (!editor_.undo()) ctx.toast("Niente da annullare"); return true;
        case 'y': case 'Y': if (!editor_.redo()) ctx.toast("Niente da ripetere"); return true;
        default: return false;
        }
    }
    switch (ev.key) {
    case Key::Char: editor_.insert(ev.ch); return true;
    case Key::Del:
        if (shift) {   // INS (SHIFT DEL), come in Run-Matrix
            if (editor_.linear()) editor_.setOverwrite(!editor_.overwrite());
            else editor_.beginArgument();
            return true;
        }
        editor_.backspace(); return true;
    case Key::DelForward: editor_.eraseForward(); return true;
    case Key::Left: editor_.moveLeft(); return true;
    case Key::Right: editor_.moveRight(); return true;
    case Key::Up: return editor_.moveUp();
    case Key::Down: return editor_.moveDown();
    case Key::Home: editor_.moveHome(); return true;
    case Key::End: editor_.moveEnd(); return true;
    case Key::Store: editor_.insertSymbol("\xE2\x86\x92"); return true;
    case Key::AC: editor_.clear(); return true;
    case Key::F1: if (shift) editor_.insertMixedFraction(); else editor_.insertFraction(); return true;
    case Key::F2: if (shift) editor_.insertSquare(); else editor_.insertRadical(); return true;
    case Key::F3: if (shift) editor_.insertText("pi"); else editor_.insertPower(); return true;
    case Key::F4: menu_.show("MATH", mathItems()); return true;
    case Key::F5: editor_.insert('i'); return true;
    case Key::F6: if (shift) editor_.insertText("theta"); else editor_.insertText("Ans"); return true;
    default: return false;
    }
}

const char* const* MathField::softKeys(bool shift) {
    static const char* const normal[6] = {"\xE2\x96\xA1/\xE2\x96\xA1", "\xE2\x88\x9A\xE2\x96\xA1", "\xE2\x96\xA1^\xE2\x96\xA1", "MATH", "i", "Ans"};
    static const char* const shifted[6] = {"\xE2\x96\xA1 \xE2\x96\xA1/\xE2\x96\xA1", "\xE2\x96\xA1\xC2\xB2", "\xCF\x80", "MATH", "i", "\xCE\xB8"};
    return shift ? shifted : normal;
}

void MathField::draw(gfx::Canvas& c, int x, int baseline, int width, int height) const {
    math::CursorTarget cursor{editor_.activeRowPtr(), editor_.cursorIndex()};
    cursor.colorParens = true;
    cursor.cursorStyle = editor_.overwrite() ? 1 : editor_.argumentPending() ? 2 : 0;
    const NaturalEditor::Range mark = editor_.errorMark();
    if (!mark.empty()) { cursor.markRow = mark.row; cursor.markBegin = mark.begin; cursor.markEnd = mark.end; cursor.markIsError = true; }
    // Scorrimento orizzontale: il cursore resta visibile.
    auto l = math::layout(editor_.root(), x - scroll_, baseline, width + scroll_ + 4000, height, math::Align::Left, &cursor);
    if (l.hasCursor) {
        if (l.cursorX > x + width - 4) scroll_ += l.cursorX - (x + width - 4);
        else if (l.cursorX < x) scroll_ = std::max(0, scroll_ - (x - l.cursorX) - 16);
        else if (scroll_ > 0 && l.size.width - scroll_ < width - 8) scroll_ = std::max(0, l.size.width - width + 8);
        l = math::layout(editor_.root(), x - scroll_, baseline, width + scroll_ + 4000, height, math::Align::Left, &cursor);
    }
    c.setClip({x, baseline - height, width, height * 2});
    math::draw(c, l, fieldPalette());
    c.resetClip();
    if (scroll_ > 0) ui::text(c, x - 8, baseline - 8, "\xE2\x97\x80", Accent, font::Face::Small);
}
} // namespace fx::app
