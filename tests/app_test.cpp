// Integration tests: the real Firmware class driven by raw STM32 FIFO words
// through the host board port (keyboard decoder -> apps -> workspace -> flash).
#include "apps.hpp"
#include "board_host.hpp"
#include "board_platform.hpp"
#include "board_port.hpp"
#include "firmware.hpp"
#include "run_matrix.hpp"
#include "test_util.hpp"
#include <string>

using namespace fx;
using app::AppId;

namespace {
std::uint16_t press(std::uint8_t code, std::uint8_t state = 1) { return static_cast<std::uint16_t>((state << 8) | code); }
struct Rig {
    BoardPlatform platform;
    Firmware fw{platform, board::storageFlash()};
    Rig() { fw.boot(); frame(); }
    void frame() { if (fw.step()) { fw.draw(board::graphics()); board::present(); } }
    void raw(std::uint16_t r) { fw.rawKey(r); board::host::advance(20); frame(); }
    void key(std::uint8_t code) { raw(press(code)); raw(press(code, 3)); }
    void type(const std::string& s) { for (char c : s) raw(press(static_cast<std::uint8_t>(c))); }
    AppId active() { return fw.apps().activeId(); }
};
}

int main() {
    board::init();
    // --- keyboard decoder (protocol from clockworkpi/PicoCalc keyboard firmware)
    {
        KeyDecoder d;
        CHECK(d.decode(0).none());
        CHECK(d.decode(press('1', 3)).none());                     // release ignored
        auto e = d.decode(press('a')); CHECK(e.isChar('a'));
        CHECK(d.decode(press(pckey::Enter)).is(Key::Exe));
        CHECK(d.decode(press(pckey::Esc)).is(Key::Exit));
        CHECK(d.decode(press(pckey::Tab)).is(Key::Menu));
        CHECK(d.decode(press(pckey::Backspace)).is(Key::Del));
        CHECK(d.decode(press(pckey::Del)).is(Key::DelForward));
        CHECK(d.decode(press(pckey::Break)).is(Key::AC));
        CHECK(d.decode(press(pckey::Insert)).is(Key::Store));
        CHECK(d.decode(press(pckey::F1)).is(Key::F1));
        CHECK(d.decode(press(pckey::F6)).is(Key::F6));
        CHECK(d.decode(press(pckey::F7)).is(Key::Shift));
        CHECK(d.decode(press(pckey::F10)).is(Key::Setup));
        CHECK(d.decode(press(pckey::Up)).is(Key::Up));
        CHECK(d.decode(press(pckey::Up)).is(Key::Up));            // auto-repeat arrives as PRESSED
        // Function keys: only the first HOLD becomes a long press, no repeats.
        auto h1 = d.decode(press(pckey::Esc, 2)); CHECK(h1.is(Key::Exit) && h1.longPress);
        CHECK(d.decode(press(pckey::Esc, 2)).none());
        CHECK(d.decode(press(pckey::Esc, 3)).none());
        CHECK(d.decode(press(pckey::Esc, 2)).longPress);         // new press after release
        // Modifiers are tracked from press/release reports.
        CHECK(d.decode(press(pckey::ModCtrl)).none()); CHECK(d.ctrl());
        auto cc = d.decode(press('c')); CHECK(cc.isChar('c') && cc.ctrl);
        CHECK(d.decode(press(pckey::ModCtrl, 3)).none()); CHECK(!d.ctrl());
        CHECK(d.decode(press(pckey::CapsLock)).none());
        CHECK(d.decode(press(pckey::Power)).none());
    }
    // --- launcher and Run-Matrix end-to-end
    {
        Rig r;
        CHECK(r.active() == AppId::MainMenu);
        r.key(pckey::Enter);                                         // first tile = Run-Matrix
        CHECK(r.active() == AppId::RunMatrix);
        r.type("1+2"); r.key(pckey::Enter);
        auto& ws = r.fw.workspace();
        CHECK(ws.history.size() == 1 && ws.history[0].value.real() == 3);
        CHECK(ws.memory.answer() == 3);
        // Fraction template: F1, numerator, DOWN, denominator, EXE -> exact 1/3.
        r.key(pckey::F1); r.type("1"); r.key(pckey::Down); r.type("3"); r.key(pckey::Enter);
        CHECK(ws.history.size() == 2);
        CHECK_EQ_STR(resultText(ws.history[1], ws.settings), "1/3");
        r.key(pckey::F5);                                            // S<->D
        CHECK_EQ_STR(resultText(ws.history[1], ws.settings), "0.3333333333");
        // Store arrow and Ans.
        r.type("Ans*3"); r.key(pckey::Insert); r.type("B"); r.key(pckey::Enter);
        CHECK_NEAR(ws.memory.recall('B'), 1.0, 1e-12);
        // Radians by default (fx-CG50); in Deg mode sin(30) = 1/2 is shown as a fraction.
        CHECK(ws.settings.angle == AngleMode::Radians);
        ws.settings.angle = AngleMode::Degrees;
        r.type("sin(30)"); r.key(pckey::Enter);
        CHECK_EQ_STR(resultText(ws.history.back(), ws.settings), "1/2");
        // Errors open the fx-CG50 error box and keep the input for correction.
        r.type("1/0"); r.key(pckey::Enter);
        CHECK(r.fw.apps().errorShown());
        CHECK_EQ_STR(r.fw.apps().errorTitle(), "Math ERROR");
        r.key(pckey::Esc);
        CHECK(!r.fw.apps().errorShown());
        auto* rm = static_cast<app::RunMatrixApp*>(r.fw.apps().active());
        CHECK_EQ_STR(rm->editor().toExpression(), "1/0");
        r.key(pckey::Break);                                         // AC clears the input
        CHECK(rm->editor().empty());
        // History recall: UP selects the last entry, EXE copies it to the input.
        r.key(pckey::Up); CHECK(rm->browsing() >= 0);
        r.key(pckey::Enter); CHECK_EQ_STR(rm->editor().toExpression(), "sin(30)");
        r.key(pckey::Break);
        // Undo / redo / copy / paste with Ctrl.
        r.type("12");
        r.raw(press(pckey::ModCtrl)); r.raw(press('z')); r.raw(press(pckey::ModCtrl, 3));
        CHECK_EQ_STR(rm->editor().toExpression(), "1");
        r.raw(press(pckey::ModCtrl)); r.raw(press('c')); r.raw(press('v')); r.raw(press(pckey::ModCtrl, 3));
        CHECK_EQ_STR(rm->editor().toExpression(), "11");
        r.key(pckey::Break);
        // SHIFT is one-shot: SHIFT F3 inserts pi, the next F3 is the power template.
        r.key(pckey::F7); CHECK(r.fw.apps().context().shift());
        r.key(pckey::F3); CHECK(!r.fw.apps().context().shift());
        CHECK_EQ_STR(rm->editor().toExpression(), "pi");
        r.key(pckey::Break);
        // MENU returns to the main menu; direct access key '1' re-opens Run-Matrix.
        r.key(pckey::Tab); CHECK(r.active() == AppId::MainMenu);
        r.type("1"); CHECK(r.active() == AppId::RunMatrix);
        r.raw(press(pckey::Esc, 2));                                 // long EXIT -> main menu
        CHECK(r.active() == AppId::MainMenu);
        // Unimplemented modes open an explicit "not available" screen.
        r.type("b"); CHECK(r.active() == AppId::Program);
        CHECK(r.fw.apps().active() != nullptr);
        r.key(pckey::Esc); CHECK(r.active() == AppId::MainMenu);
        // Autosave after leaving an app, then a reboot restores the workspace.
        r.frame();
        CHECK(r.fw.saves() >= 1);
    }
    {
        Rig again;                                                   // same RAM flash = reboot
        CHECK(again.fw.loadedFromFlash());
        CHECK(again.fw.workspace().history.size() >= 4);
        CHECK_NEAR(again.fw.workspace().memory.recall('B'), 1.0, 1e-12);
    }
    // --- partial LCD update: an unchanged frame transmits nothing.
    {
        Rig r;
        board::invalidate(); r.fw.draw(board::graphics()); board::present();
        CHECK(board::lastFlushBytes() == 320u * 320u * 3u);
        r.fw.draw(board::graphics()); board::present();
        CHECK(board::lastFlushBytes() == 0);
        r.key(pckey::Right);                                          // move the selection
        CHECK(board::lastFlushBytes() > 0 && board::lastFlushBytes() < 320u * 320u * 3u / 2);
    }
    return testutil::finish("app_test");
}
