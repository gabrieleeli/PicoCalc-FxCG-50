// v1.15.0 (F5): editor di testo multiriga (righe e striscia Notes di eActivity,
// memo, sorgenti di Program, script Python): editing, righe lunghe e a capo,
// ricerca, salto alla riga, clipboard del Context, limiti per scheda e
// memoria esaurita senza perdita del testo.
#include "graphics_engine.hpp"
#include "test_util.hpp"
#include "text_editor.hpp"
#include <cstdlib>
#include <new>
#include <vector>

using namespace fx;

// operator new che fallisce dopo un numero dato di allocazioni (memoria esaurita simulata).
namespace { int gFailCountdown = -1; }
void* operator new(std::size_t n) {
    if (gFailCountdown == 0) throw std::bad_alloc();
    if (gFailCountdown > 0) --gFailCountdown;
    void* p = std::malloc(n ? n : 1);
    if (!p) throw std::bad_alloc();
    return p;
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

namespace {
KeyEvent k(Key key, bool ctrl = false) { KeyEvent e; e.key = key; e.ctrl = ctrl; return e; }
KeyEvent ch(char c, bool ctrl = false) { KeyEvent e; e.key = Key::Char; e.ch = c; e.ctrl = ctrl; return e; }
void type(TextEditor& t, const std::string& s) { for (char c : s) t.key(ch(c)); }
} // namespace

int main() {
    // ---------------------------------------------------------------- editing di base
    {
        TextEditor t(1000, 100);
        CHECK(t.empty() && t.lineCount() == 1 && t.bytes() == 0);
        type(t, "ciao");
        CHECK(t.text() == "ciao" && t.cursorColumn() == 4);
        t.key(k(Key::Exe));                                     // EXE = a capo (manuale 10-7)
        type(t, "mondo");
        CHECK(t.text() == "ciao\nmondo" && t.lineCount() == 2 && t.bytes() == 10);
        t.key(k(Key::Up));
        CHECK(t.cursorLine() == 0 && t.cursorColumn() == 4);
        t.key(k(Key::Home)); t.key(k(Key::DelForward));
        CHECK(t.text() == "iao\nmondo");
        t.key(k(Key::End)); t.key(k(Key::DelForward));          // unisce le righe
        CHECK(t.text() == "iaomondo" && t.lineCount() == 1);
        CHECK(t.cursorColumn() == 3);
        t.key(k(Key::Del));                                     // DEL cancella a sinistra del cursore
        CHECK(t.text() == "iamondo" && t.cursorColumn() == 2);
        t.key(k(Key::AC));                                      // AC svuota la riga
        CHECK(t.text().empty() && t.bytes() == 0);
        // UTF-8: il cursore salta i caratteri interi
        CHECK(t.insert("\xCE\xB8=\xCF\x80"));                   // θ=π
        CHECK(t.cursorColumn() == 5);
        t.left();
        CHECK(t.cursorColumn() == 3);
        t.backspace();
        CHECK(t.text() == "\xCE\xB8\xCF\x80" && t.cursorColumn() == 2);
        // testo su piu' righe incollato
        t.clear();
        CHECK(t.insert("a\nbb\nccc"));
        CHECK(t.lineCount() == 3 && t.cursorLine() == 2 && t.cursorColumn() == 3 && t.bytes() == 8);
        // \r\n dei file di Windows
        CHECK(t.setText("x\r\ny\r\n") && t.lineCount() == 3 && t.line(0) == "x" && t.line(1) == "y");
        t.deleteLine();
        CHECK(t.lineCount() == 2 && t.line(0) == "y");
    }
    // ---------------------------------------------------------------- spostamenti e colonna voluta
    {
        TextEditor t;
        CHECK(t.setText("lunghissima\nab\nterza riga"));
        t.moveTo(0, 8);
        t.down();
        CHECK(t.cursorLine() == 1 && t.cursorColumn() == 2);    // fine della riga corta
        t.down();
        CHECK(t.cursorLine() == 2 && t.cursorColumn() == 8);    // ritrova la colonna voluta
        t.top();
        CHECK(t.cursorLine() == 0 && t.cursorColumn() == 0);
        t.bottom();
        CHECK(t.cursorLine() == 2 && t.cursorColumn() == 10);
        t.key(k(Key::Home, true));
        CHECK(t.cursorLine() == 0);
        // PageUp/PageDown di righe visibili
        std::string many;
        for (int i = 0; i < 40; ++i) many += "r" + std::to_string(i) + "\n";
        CHECK(t.setText(many));
        t.setVisibleRows(10);
        t.pageDown();
        CHECK(t.cursorLine() == 10);
        t.pageDown(); t.pageDown(); t.pageDown(); t.pageDown();
        CHECK(t.cursorLine() == 40);
        t.pageUp();
        CHECK(t.cursorLine() == 30);
    }
    // ---------------------------------------------------------------- F5.2 ricerca e salto alla riga
    {
        TextEditor t;
        CHECK(t.setText("alfa beta\ngamma alfa\ndelta"));
        CHECK(t.find("alfa") && t.cursorLine() == 1 && t.cursorColumn() == 6);
        CHECK(t.find("alfa") && t.cursorLine() == 0 && t.cursorColumn() == 0);   // ricomincia dall'inizio
        CHECK(t.find("delta") && t.cursorLine() == 2);
        CHECK(!t.find("zeta") && t.cursorLine() == 2);
        CHECK(!t.find(""));
        CHECK(t.gotoLine(2) && t.cursorLine() == 1 && t.cursorColumn() == 0);
        CHECK(!t.gotoLine(0) && !t.gotoLine(4) && t.cursorLine() == 1);
        // selezione e clipboard di testo (Ctrl+C/X/V; Ctrl+frecce selezionano)
        std::string clip;
        t.moveTo(0, 0);
        for (int i = 0; i < 4; ++i) t.key(k(Key::Right, true));
        CHECK(t.hasSelection() && t.selectedText() == "alfa");
        t.key(ch('c', true), &clip);
        CHECK(clip == "alfa");
        t.key(k(Key::End));
        t.key(ch('v', true), &clip);
        CHECK(t.line(0) == "alfa betaalfa");
        // selezione su due righe, taglio
        t.moveTo(1, 6);
        t.beginSelection();
        t.down();
        CHECK(t.selectedText() == "alfa\ndelta");
        t.key(ch('x', true), &clip);
        CHECK(clip == "alfa\ndelta" && t.text() == "alfa betaalfa\ngamma ");
        // Ctrl+C senza selezione copia la riga
        t.moveTo(0, 2);
        t.key(ch('c', true), &clip);
        CHECK(clip == "alfa betaalfa");
        // digitare con una selezione la sostituisce
        t.moveTo(1, 0);
        t.beginSelection(); t.end();
        t.key(ch('Z'));
        CHECK(t.line(1) == "Z");
    }
    // ---------------------------------------------------------------- righe lunghe: a capo e scorrimento
    {
        TextEditor t;
        const std::string longLine = "La funzione f(x) = x^2 - 4 ha due radici reali distinte che si trovano facilmente con la formula";
        CHECK(t.setText(longLine));
        const auto pts = t.wrapPoints(0, 160);
        CHECK(pts.size() > 2 && pts[0] == 0);
        for (std::size_t i = 1; i < pts.size(); ++i) CHECK(pts[i] > pts[i - 1] && longLine[pts[i] - 1] == ' ');   // a capo dopo uno spazio
        std::vector<std::uint16_t> px(320 * 320, 0xFFFF);
        gfx::Canvas c(px.data(), 320, 320);
        t.moveTo(0, longLine.size());
        t.draw(c, gfx::Rect{0, 20, 160, 64}, 0);
        CHECK(t.firstLine() == 0);
        // senza a capo: scorrimento orizzontale fino al cursore
        t.setWrap(false);
        CHECK(t.wrapPoints(0, 160).size() == 1);
        t.draw(c, gfx::Rect{0, 20, 160, 64}, 0);
        // molte righe: la prima riga visibile segue il cursore
        std::string many;
        for (int i = 0; i < 30; ++i) many += "riga " + std::to_string(i) + "\n";
        CHECK(t.setText(many));
        t.bottom();
        t.draw(c, gfx::Rect{0, 20, 200, 80}, 0);
        CHECK(t.firstLine() > 20 && t.firstLine() <= 30);
        t.top();
        t.draw(c, gfx::Rect{0, 20, 200, 80}, 0);
        CHECK(t.firstLine() == 0);
    }
    // ---------------------------------------------------------------- F5.3 limiti per scheda
    {
        CHECK(limits::kTextEditorBytes == (board::config::kIsPicoPlus2W ? 256u * 1024u : 24u * 1024u));
        TextEditor t(20, 8);
        type(t, "12345678");
        CHECK(!t.full());
        t.key(ch('9'));                                          // riga oltre 8 byte: rifiutato
        CHECK(t.full() && t.text() == "12345678");
        t.key(k(Key::Exe));
        type(t, "abcdefgh");
        t.key(k(Key::Exe));
        CHECK(t.bytes() == 18 && !t.full());
        type(t, "xyz");
        CHECK(t.full() && t.bytes() == 20 && t.text() == "12345678\nabcdefgh\nxy");  // totale 20 byte al massimo
        CHECK(!t.insert("\nz") && t.text() == "12345678\nabcdefgh\nxy");
        CHECK(!t.setText(std::string(21, 'q')) && t.text() == "12345678\nabcdefgh\nxy");
        CHECK(!t.setText("ok\n123456789") && t.full() && t.text() == "12345678\nabcdefgh\nxy");
        TextEditor board;                                        // limite della scheda
        CHECK(board.maxBytes() == limits::kTextEditorBytes);
        std::string full;
        while (full.size() + 100 <= limits::kTextEditorBytes) full += std::string(99, 'a') + "\n";
        full.resize(limits::kTextEditorBytes);
        CHECK(board.setText(full) && board.bytes() == limits::kTextEditorBytes);
        board.bottom();
        CHECK(!board.insert("x") && board.full() && board.bytes() == limits::kTextEditorBytes);
    }
    // ---------------------------------------------------------------- memoria esaurita: testo intatto
    {
        TextEditor t(1u << 20, 1u << 16);
        CHECK(t.setText("base\nseconda"));
        // operator new che fallisce dopo n allocazioni: ogni modifica o riesce o lascia il testo com'era
        for (int failAt = 0; failAt < 6; ++failAt) {
            const std::string before = t.text();
            bool refused = false;
            const std::string added = "\nnuova riga lunga lunga lunga lunga lunga";
            gFailCountdown = failAt;
            refused = !t.insert(added);
            gFailCountdown = -1;
            if (refused) CHECK(t.text() == before && t.full());
            else CHECK(t.text() != before);
            t.setText("base\nseconda");
            t.moveTo(0, 2);
        }
    }
    return testutil::finish("text_editor_test");
}
