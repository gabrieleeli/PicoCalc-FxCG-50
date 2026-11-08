// End-to-end flows through every implemented mode, driven by raw FIFO words.
#include "apps.hpp"
#include "board_host.hpp"
#include "board_platform.hpp"
#include "board_port.hpp"
#include "firmware.hpp"
#include "test_util.hpp"
#include <cmath>
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
    void enter(const std::string& s) { type(s); key(pckey::Enter); }
    template <typename T> T* app() { return static_cast<T*>(fw.apps().active()); }
    bool error() { return fw.apps().errorShown(); }
    void home() { key(pckey::Tab); }
};
}

int main() {
    board::init();
    Rig r;
    Workspace& ws = r.fw.workspace();
    // ---------------- EQUATION
    r.type("a"); CHECK(r.fw.apps().activeId() == AppId::Equation);
    r.key(pckey::F2); r.key(pckey::F2);                   // POLY, degree 3
    CHECK(ws.equation.degree == 3);
    r.enter("1"); r.enter("-6"); r.enter("11"); r.enter("-6");
    CHECK(ws.equation.polynomial[0] == 1 && ws.equation.polynomial[3] == -6);
    r.key(pckey::F1);
    auto* eq = r.app<app::EquationApp>();
    CHECK(eq->screen() == app::EquationApp::Screen::Result);
    CHECK(eq->results().rows.size() == 3);
    CHECK_EQ_STR(eq->results().rows[0].second, "1");
    CHECK_EQ_STR(eq->results().rows[2].second, "3");
    r.key(pckey::Esc); r.key(pckey::Esc); r.key(pckey::Esc);   // result -> grid -> size -> menu
    r.key(pckey::F1); r.key(pckey::F1);                   // SIML, 2 unknowns: x+y=5, x-y=1
    r.enter("1"); r.enter("1"); r.enter("5"); r.enter("1"); r.enter("-1"); r.enter("1");
    r.key(pckey::F1);
    CHECK_EQ_STR(eq->results().rows[0].second, "3");
    CHECK_EQ_STR(eq->results().rows[1].second, "2");
    r.key(pckey::Esc); r.key(pckey::Esc); r.key(pckey::Esc);
    r.key(pckey::F2); r.key(pckey::F1);                   // POLY degree 2: x^2+1 -> complex roots
    r.enter("1"); r.enter("0"); r.enter("1"); r.key(pckey::F1);
    CHECK_EQ_STR(eq->results().rows[0].second, "i");
    CHECK_EQ_STR(eq->results().rows[1].second, "-i");
    r.key(pckey::Esc); r.key(pckey::Esc); r.key(pckey::Esc);
    r.key(pckey::F4); r.key(pckey::F2);                   // INEQ degree 2: x^2-3x+2 > 0
    r.enter("1"); r.enter("-3"); r.enter("2"); r.key(pckey::F1);
    CHECK(eq->results().rows.size() >= 1);
    CHECK_EQ_STR(eq->results().rows[0].second, "x<1 , 2<x");
    r.key(pckey::Esc); r.key(pckey::F4); r.key(pckey::F4); r.key(pckey::F1);   // relation cycles: < ... then >= then <=
    r.key(pckey::Esc); r.key(pckey::Esc); r.key(pckey::Esc);
    r.key(pckey::F3);                                     // SOLVER
    r.key(pckey::Enter); r.key(pckey::Break); r.enter("x^3=8"); r.enter("1");
    r.key(pckey::F1);
    CHECK(!r.error());
    CHECK_EQ_STR(eq->results().rows[0].second, "2");
    CHECK_NEAR(ws.memory.recall('X'), 2, 1e-12);
    r.home();
    // ---------------- STATISTICS
    r.type("2"); CHECK(r.fw.apps().activeId() == AppId::Statistics);
    for (const char* v : {"1", "2", "3", "4", "5"}) r.enter(v);
    r.key(pckey::Right); r.key(pckey::Home);
    for (const char* v : {"2.1", "3.9", "6.2", "7.8", "10.1"}) r.enter(v);
    CHECK(ws.lists[0].size() == 5 && ws.lists[1].size() == 5 && ws.lists[1][4] == 10.1);
    r.key(pckey::Down); r.key(pckey::Down);               // cannot move past the first empty cell
    r.enter("7");
    CHECK(ws.lists[1].size() == 6);
    r.key(pckey::Up); r.key(pckey::Del);           // delete it again
    CHECK(ws.lists[1].size() == 5);
    r.key(pckey::F2); r.type("1");                        // CALC -> 1-Var
    auto* st = r.app<app::StatisticsApp>();
    CHECK(st->screen() == app::StatisticsApp::Screen::Results);
    CHECK_EQ_STR(st->results().rows[0].second, "3");      // mean of List1
    r.key(pckey::Esc);
    r.key(pckey::F2); r.type("3");                        // CALC -> Reg ax+b
    CHECK(!r.error());
    CHECK_EQ_STR(st->results().rows[0].first, "a");
    CHECK_NEAR(std::stod(st->results().rows[0].second), 1.99, 1e-9);
    CHECK_NEAR(std::stod(st->results().rows[1].second), 0.05, 1e-9);
    r.key(pckey::F2);                                     // COPY -> first empty GRAPH slot
    CHECK(!ws.functions[1].expr.empty());
    r.key(pckey::F1);                                     // DRAW
    CHECK(st->screen() == app::StatisticsApp::Screen::Graph);
    r.key(pckey::Esc); r.key(pckey::Esc);
    r.key(pckey::F4); r.type("2");                        // DIST -> Normal CD (defaults -1..1)
    r.key(pckey::F1);
    CHECK(!r.error());
    r.home();
    // ---------------- GRAPH + G-SOLVE + TABLE
    r.type("5"); CHECK(r.fw.apps().activeId() == AppId::Graph);
    ws.functions[1].expr.clear();
    r.key(pckey::Down); r.type("x^2"); r.key(pckey::Right); r.enter("-2");   // Y2 = x^2-2
    CHECK_EQ_STR(ws.functions[1].expr, "(x)^(2)-2");
    ws.functions[0].selected = false;
    r.key(pckey::F6);
    auto* gr = r.app<app::GraphApp>();
    CHECK(gr->screen() == app::GraphApp::Screen::Plot);
    r.key(pckey::F5); r.type("1");                        // G-SOLVE ROOT
    CHECK(!r.error());
    CHECK(gr->solutions().size() == 2);
    CHECK_NEAR(gr->solutions()[0].first, -std::sqrt(2.0), 1e-9);
    CHECK_NEAR(gr->solutions()[1].first, std::sqrt(2.0), 1e-9);
    r.key(pckey::Esc);
    r.key(pckey::F5); r.type("3");                        // MIN
    CHECK(gr->solutions().size() == 1);
    CHECK_NEAR(gr->solutions()[0].first, 0, 1e-10); CHECK_NEAR(gr->solutions()[0].second, -2, 1e-12);
    r.key(pckey::Esc);
    r.key(pckey::F5); r.type("8"); r.type("1");          // G-SOLVE ∫dx > ∫dx: limits typed as numbers
    r.enter("0"); r.enter("3");
    CHECK(!r.error());
    CHECK_NEAR(ws.memory.answer(), 3.0, 1e-9);            // ∫(x^2-2)dx from 0 to 3 = 9-6
    CHECK_NEAR(gr->integralValue(), 3.0, 1e-9);
    r.key(pckey::Esc);
    r.key(pckey::F2); r.type("3"); r.key(pckey::Enter);    // ZOOM IN at the pointer (factor 2)
    CHECK_NEAR(ws.view.xmax - ws.view.xmin, 6.3, 1e-9);
    r.key(pckey::F6);                                     // G<->T: back to the relation list
    CHECK(gr->screen() == app::GraphApp::Screen::List);
    r.key(pckey::F8); r.type("4");                        // OPTN > TABLE
    CHECK(r.fw.apps().activeId() == AppId::Table);
    r.home();
    // ---------------- SYSTEM, CONVERSION, MEMORY
    r.type("g"); CHECK(r.fw.apps().activeId() == AppId::System);
    r.key(pckey::Right);                                  // Angle: Rad -> Gra
    CHECK(ws.settings.angle == AngleMode::Gradians);
    r.key(pckey::Left); r.key(pckey::Left);               // -> Deg
    CHECK(ws.settings.angle == AngleMode::Degrees);
    r.key(pckey::Esc);
    CHECK(r.fw.apps().activeId() == AppId::MainMenu);
    r.type("k"); CHECK(r.fw.apps().activeId() == AppId::Conversion);
    r.key(pckey::Break); r.enter("2.5");                  // 2.5 m -> km (default From m, To km)
    r.key(pckey::F1);
    CHECK_NEAR(ws.memory.answer(), 0.0025, 1e-15);
    r.home();
    r.type("f"); CHECK(r.fw.apps().activeId() == AppId::Memory);
    r.key(pckey::F1); r.key(pckey::Enter);                 // delete Run-Matrix history
    CHECK(ws.history.empty());
    r.home();
    // Everything above survives a reboot.
    r.frame();
    Rig again;
    CHECK(again.fw.workspace().lists[1].size() == 5);
    CHECK(again.fw.workspace().settings.angle == AngleMode::Degrees);
    CHECK_EQ_STR(again.fw.workspace().functions[1].expr, "(x)^(2)-2");
    return testutil::finish("apps_flow_test");
}
