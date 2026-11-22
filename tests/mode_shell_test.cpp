// v1.15.0 (F4): guscio comune delle modalita': pila di schermate, barra F1-F6
// con sottomenu, pagine e SHIFT, lista selezionabile, conferme, richieste di
// valori, SET UP da tabella, VARS dai resolver, e TABLE portata sul guscio.
#include "app_rig.hpp"
#include "mode_shell.hpp"
#include "test_util.hpp"

using namespace fx;
using namespace fx::app;

namespace {
KeyEvent k(Key key) { KeyEvent e; e.key = key; return e; }
KeyEvent ch(char c) { KeyEvent e; e.key = Key::Char; e.ch = c; return e; }
enum : int { AOpen = 1, ANew, ADelete, ASearch, ASave, ASaveAs, ATop, ABottom, AShiftA, AP2 };
const FKeyPage kFileSub{{{"SAVE", ASave}, {"SAVE\xC2\xB7" "AS", ASaveAs}, {}, {}, {}, {}}};
const FKeyMenu kFileMenu{&kFileSub, 1, nullptr};
const FKeyPage kJumpSub{{{"TOP", ATop}, {"BOTTOM", ABottom}, {}, {}, {}, {}}};
const FKeyMenu kJumpMenu{&kJumpSub, 1, nullptr};
const FKeyPage kPages[2] = {{{{"OPEN", AOpen}, {"NEW", ANew}, {"FILE", -1, &kFileMenu}, {"JUMP", -1, &kJumpMenu}, {"DEL", ADelete}, {}}},
                            {{{"SEARCH", ASearch}, {"P2", AP2}, {}, {}, {}, {}}}};
const FKeyPage kShift{{{"A\xE2\x87\x94" "a", AShiftA}, {}, {}, {}, {}, {}}};
const FKeyMenu kRoot{kPages, 2, &kShift};
} // namespace

int main() {
    // ---------------------------------------------------------------- pila di schermate
    {
        ScreenStack s;
        s.reset(1);
        CHECK(s.top() == 1 && s.depth() == 1 && !s.pop());
        s.push(2); s.push(3);
        CHECK(s.top() == 3 && s.below() == 2 && s.root() == 1);
        s.replace(4);
        CHECK(s.top() == 4 && s.pop() && s.top() == 2 && s.pop() && s.top() == 1);
        for (int i = 0; i < 20; ++i) s.push(10 + i);           // oltre il limite: la cima si sostituisce
        CHECK(s.depth() == ScreenStack::MaxDepth && s.top() == 29);
    }
    // ---------------------------------------------------------------- barra F1-F6: sottomenu, pagine, SHIFT
    {
        FKeyBar b;
        b.set(&kRoot);
        const char* l[6];
        b.labels(false, l);
        CHECK(std::string(l[0]) == "OPEN" && std::string(l[5]) == "\xE2\x96\xB7");   // ▷ con piu' pagine
        CHECK(b.key(k(Key::F1), false) == AOpen && b.key(k(Key::F5), false) == ADelete);
        CHECK(b.key(k(Key::F3), false) == FKeyBar::Handled && b.inSubmenu());          // FILE
        b.labels(false, l);
        CHECK(std::string(l[0]) == "SAVE" && l[5] == nullptr);
        CHECK(b.key(k(Key::F2), false) == ASaveAs);
        CHECK(b.key(k(Key::F3), false) == FKeyBar::None);                            // vuoto
        CHECK(b.key(k(Key::Exit), false) == FKeyBar::Handled && !b.inSubmenu());     // EXIT risale
        CHECK(b.key(k(Key::F6), false) == FKeyBar::Handled && b.page() == 1);        // ▷
        CHECK(b.key(k(Key::F1), false) == ASearch && b.key(k(Key::F2), false) == AP2);
        CHECK(b.key(k(Key::F6), false) == FKeyBar::Handled && b.page() == 0);        // torna alla prima pagina
        CHECK(b.key(k(Key::F1), true) == AShiftA);                                   // pagina di SHIFT
        CHECK(b.key(k(Key::Exe), false) == FKeyBar::None && b.key(k(Key::Exit), false) == FKeyBar::None);
        CHECK(b.key(k(Key::F4), false) == FKeyBar::Handled && b.key(k(Key::F2), false) == ABottom);
        b.set(nullptr);
        CHECK(b.key(k(Key::F1), false) == FKeyBar::None);
    }
    // ---------------------------------------------------------------- lista selezionabile
    {
        SelectList list;
        list.items = {"A", "B", "C", "D", "E", "F", "G", "H"};
        CHECK(list.key(k(Key::Up)) && list.selected == 7);                            // circolare
        CHECK(list.key(k(Key::Down)) && list.selected == 0);
        CHECK(list.key(k(Key::PageDown), 3) && list.selected == 3);
        CHECK(list.key(k(Key::End)) && list.selected == 7 && list.key(k(Key::Home)) && list.selected == 0);
        CHECK(!list.key(k(Key::Exe)));
        std::vector<std::uint16_t> px(320 * 320);
        gfx::Canvas c(px.data(), 320, 320);
        list.selected = 7;
        list.draw(c, gfx::Rect{0, 20, 300, 60}, 20);
        CHECK(list.first() == 5);
        list.items.clear();
        CHECK(!list.key(k(Key::Down)));
    }
    // ---------------------------------------------------------------- modali: conferma, richiesta, menu
    {
        ModeShell shell;
        int confirmed = -1, answered = -1, chosen = -1;
        std::string answer;
        auto onConfirm = [&](int tag, bool yes) { confirmed = yes ? tag : -tag; };
        auto onAnswer = [&](int tag) { answered = tag; answer = shell.ask.text(); };
        auto onMenu = [&](int tag, int c) { chosen = tag * 100 + c; };
        CHECK(!shell.modalKey(k(Key::F1), onConfirm, onAnswer, onMenu));             // nessun modale
        shell.confirm.open("Delete eActivity?", 7);
        CHECK(shell.modalOpen() && shell.modalKey(ch('x'), onConfirm, onAnswer, onMenu) && shell.confirm.active());
        CHECK(shell.modalKey(k(Key::F6), onConfirm, onAnswer, onMenu) && !shell.confirm.active() && confirmed == -1);
        shell.confirm.open("Delete eActivity?", 7);
        CHECK(shell.modalKey(k(Key::Exe), onConfirm, onAnswer, onMenu) && !shell.confirm.active() && confirmed == 7);
        // un errore nell'azione lascia la conferma aperta (come TABLE)
        shell.confirm.open("Eliminare?", 3);
        bool thrown = false;
        try { shell.modalKey(k(Key::F1), [](int, bool) { throw std::runtime_error("x"); }, onAnswer, onMenu); } catch (const std::runtime_error&) { thrown = true; }
        CHECK(thrown && shell.confirm.active());
        shell.confirm.close();
        shell.ask.open("Valore:", 5, '1');
        for (char c : std::string("+2")) shell.modalKey(ch(c), onConfirm, onAnswer, onMenu);
        shell.modalKey(k(Key::Exe), onConfirm, onAnswer, onMenu);
        CHECK(answered == 5 && answer == "1+2" && !shell.ask.active());
        shell.ask.open("Valore:", 6);
        shell.modalKey(k(Key::Exit), onConfirm, onAnswer, onMenu);
        CHECK(answered == 5 && !shell.ask.active());
        shell.showMenu("ROW", {"DELETE", "INSERT", "ADD"}, 4);
        shell.modalKey(k(Key::Down), onConfirm, onAnswer, onMenu);
        shell.modalKey(k(Key::Exe), onConfirm, onAnswer, onMenu);
        CHECK(chosen == 401 && !shell.menu.open && shell.menuTag == 0);
        shell.showMenu("ROW", {"DELETE"}, 4);
        shell.modalKey(k(Key::Exit), onConfirm, onAnswer, onMenu);
        CHECK(chosen == 401 && shell.menuTag == 0);
        std::vector<std::uint16_t> px(320 * 320);
        gfx::Canvas c(px.data(), 320, 320);
        shell.confirm.open("Prova", 1);
        shell.drawModals(c);
        std::size_t painted = 0;
        for (int x = 16; x < 304; ++x) painted += c.getPixel(x, 150) != 0;
        CHECK(painted > 200);                                                         // il riquadro della conferma e' disegnato
    }
    // ---------------------------------------------------------------- SET UP da tabella e VARS dai resolver
    {
        board::init();
        testrig::Rig r;
        Context& ctx = r.fw.apps().context();
        std::vector<SetupItem> setup = {
            {"Angle", {"Deg", "Rad", "Gra"},
             [](const Context& c) { return c.ws.settings.angle == AngleMode::Degrees ? 0 : c.ws.settings.angle == AngleMode::Radians ? 1 : 2; },
             [](Context& c, int v) { c.ws.settings.angle = v == 0 ? AngleMode::Degrees : v == 1 ? AngleMode::Radians : AngleMode::Gradians; }},
            {"Sub Name", {"On", "Off"}, [](const Context& c) { return c.ws.settings.subName ? 0 : 1; }, [](Context& c, int v) { c.ws.settings.subName = v == 0; }}};
        Form form;
        r.ws().settings.angle = AngleMode::Radians;
        fillSetup(ctx, setup, form);
        CHECK(form.fields.size() == 2 && form.fields[0].choice == 1 && form.fields[0].text == "Rad" && form.fields[1].type == FormField::Type::Choice);
        form.fields[0].choice = 2; form.fields[1].choice = 1;
        applySetup(ctx, setup, form);
        CHECK(r.ws().settings.angle == AngleMode::Gradians && !r.ws().settings.subName);
        // VARS: variabili di sistema, liste e matrici definite, dagli stessi resolver di Run-Matrix
        r.ws().setList(2, {1, 2, 3});
        CalcValue m = CalcValue::matrix(1, 1);
        m.re[0] = 5;
        r.ws().storeMatrix(1, m);
        const auto vars = varsItems(ctx);
        bool xmin = false, list3 = false, matB = false, list1 = false;
        for (const auto& v : vars) {
            xmin = xmin || v.insert == "xmin";
            list3 = list3 || v.insert == "List 3";
            list1 = list1 || v.insert == "List 1";
            matB = matB || v.insert == "Mat B";
        }
        CHECK(xmin && list3 && matB && !list1);
    }
    // ---------------------------------------------------------------- F4.5 TABLE sul guscio
    {
        board::init();
        testrig::Rig r;
        r.ws().functions[0].expr = "x^2"; r.ws().functions[0].selected = true;
        r.fw.apps().open(AppId::Table);
        auto* t = r.active<TableApp>();
        CHECK(t != nullptr && t->screen() == TableApp::Screen::List);
        r.key(pckey::F5);                                                             // SET (pila: List -> Set)
        CHECK(t->screen() == TableApp::Screen::Set);
        r.key(pckey::F10);                                                            // SET UP sostituisce SET
        CHECK(t->screen() == TableApp::Screen::Setup);
        r.key(pckey::Esc);                                                            // torna dove SET era stato aperto
        CHECK(t->screen() == TableApp::Screen::List);
        r.key(pckey::F6);                                                             // TABLE
        CHECK(t->screen() == TableApp::Screen::Table && r.ws().table.generated);
        r.key(pckey::F10);
        CHECK(t->screen() == TableApp::Screen::Setup);
        r.key(pckey::Esc);
        CHECK(t->screen() == TableApp::Screen::Table);                               // la tabella esiste ancora
        r.key(pckey::F2);                                                             // DELETE: conferma del guscio
        r.key(pckey::Esc);
        CHECK(t->screen() == TableApp::Screen::Table && r.ws().table.generated);
        r.key(pckey::F2);
        r.key(pckey::Enter);
        CHECK(t->screen() == TableApp::Screen::List && !r.ws().table.generated);
        r.key(pckey::F3);                                                             // TYPE: menu del guscio
        r.key(pckey::Down);
        r.key(pckey::Enter);
        CHECK(r.ws().table.funcType == GraphType::Polar);
    }
    return testutil::finish("mode_shell_test");
}
