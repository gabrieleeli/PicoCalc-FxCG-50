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
    void settle() { for (int i = 0; i < 400; ++i) { board::host::advance(5); if (!fw.step()) break; fw.draw(board::graphics()); board::present(); } }
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
    // ---------------- EQUATION (v1.12.0: schermate del manuale fx-CG50 4-1...4-5)
    app::Context& ctx = r.fw.apps().context();
    r.type("a"); CHECK(r.fw.apps().activeId() == AppId::Equation);
    r.key(pckey::F2); r.key(pckey::F2);                   // POLY, degree 3
    CHECK(ws.equation.degree == 3);
    r.enter("1"); r.enter("-6"); r.enter("11"); r.enter("-6");
    CHECK(ws.equation.polynomial[0].value == Complex(1, 0) && ws.equation.polynomial[3].value == Complex(-6, 0));
    r.key(pckey::F1);
    auto* eq = r.app<app::EquationApp>();
    CHECK(eq->screen() == app::EquationApp::Screen::Solutions);
    CHECK(eq->solutionRows() == 3);
    CHECK_EQ_STR(eq->solutionText(0, ctx), "X1=3");       // reali decrescenti (manuale 4-3)
    CHECK_EQ_STR(eq->solutionText(2, ctx), "X3=1");
    r.key(pckey::Esc); r.key(pckey::Esc); r.key(pckey::Esc);   // soluzioni -> coefficienti -> grado -> menu
    r.key(pckey::F1); r.key(pckey::F1);                   // SIML, 2 unknowns: x+y=5, x-y=1
    r.enter("1"); r.enter("1"); r.enter("5"); r.enter("1"); r.enter("-1"); r.enter("1");
    r.key(pckey::F1);
    CHECK_EQ_STR(eq->solutionText(0, ctx), "X=3");
    CHECK_EQ_STR(eq->solutionText(1, ctx), "Y=2");
    r.key(pckey::Esc); r.key(pckey::Esc); r.key(pckey::Esc);
    r.key(pckey::F2); r.key(pckey::F1);                   // POLY degree 2: x^2+1
    r.enter("1"); r.enter("0"); r.enter("1"); r.key(pckey::F1);
    CHECK_EQ_STR(eq->message(), "No Solution");          // Complex Mode Real: solo radici reali (manuale 4-4)
    ws.settings.complex = ComplexMode::Rectangular;
    r.key(pckey::Esc); r.key(pckey::F1);
    CHECK_EQ_STR(eq->solutionText(0, ctx), "X1=i");
    CHECK_EQ_STR(eq->solutionText(1, ctx), "X2=-i");
    ws.settings.complex = ComplexMode::Real;
    r.key(pckey::Esc); r.key(pckey::Esc); r.key(pckey::Esc);
    r.key(pckey::F4); r.key(pckey::F2);                   // INEQ (estensione) degree 2: x^2-3x+2 > 0
    r.enter("1"); r.enter("-3"); r.enter("2"); r.key(pckey::F1);
    CHECK(eq->inequalityResults().rows.size() >= 1);
    CHECK_EQ_STR(eq->inequalityResults().rows[0].second, "x<1 , 2<x");
    r.key(pckey::Esc); r.key(pckey::F5); r.key(pckey::F5); r.key(pckey::F1);   // relation cycles: < ... then >= then <=
    CHECK(!r.error());
    r.key(pckey::Esc); r.key(pckey::Esc); r.key(pckey::Esc);
    r.key(pckey::F3);                                     // SOLVER: l'equazione vuota si scrive subito
    CHECK(eq->screen() == app::EquationApp::Screen::SolverTable && eq->editing());
    r.type("x^3"); r.key(pckey::Right); r.enter("=8");    // Math: → esce dall'esponente
    CHECK(!eq->editing() && eq->solverVars().size() == 1 && eq->row() == 0);
    r.enter("1");                                         // X = 1 (valore iniziale)
    CHECK_NEAR(ws.memory.recall('X'), 1, 0);
    r.key(pckey::F6);                                     // SOLVE
    CHECK(!r.error());
    CHECK(eq->screen() == app::EquationApp::Screen::SolverResult);
    CHECK_NEAR(eq->solverOutcome().value, 2, 1e-12);
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
#if FXAI_BOARD_PICO_PLUS_2W
    // v1.18.0: barre F1-F6 della fx-CG50 (CALC › 1-VAR, CALC › REG › X › ax+b, COPY, DRAW, DIST › NORM › Ncd)
    r.key(pckey::F2); r.key(pckey::F1);                   // CALC -> 1-VAR
    auto* st = r.app<app::StatisticsApp>();
    CHECK(st->screen() == app::StatisticsApp::Screen::Results);
    CHECK_EQ_STR(st->results().rows[0].second, "3");      // mean of List1
    r.key(pckey::Esc);
    r.key(pckey::F2); r.key(pckey::F3); r.key(pckey::F1); r.key(pckey::F1);   // CALC -> REG -> X -> ax+b
    r.settle();
    CHECK(!r.error());
    CHECK(st->screen() == app::StatisticsApp::Screen::Results);
    CHECK_EQ_STR(st->results().rows[0].first, "a");
    CHECK_NEAR(std::stod(st->results().rows[0].second), 1.99, 1e-9);
    CHECK_NEAR(std::stod(st->results().rows[1].second), 0.05, 1e-9);
    r.key(pckey::F5); r.key(pckey::Enter);                // COPY -> first empty GRAPH slot (selected)
    CHECK(!ws.functions[1].expr.empty());
    r.key(pckey::F6);                                     // DRAW
    CHECK(st->screen() == app::StatisticsApp::Screen::Graph);
    r.key(pckey::Esc); r.key(pckey::Esc);
    r.key(pckey::F5); r.key(pckey::F1); r.key(pckey::F2); // DIST -> NORM -> Ncd (defaults -1..1)
    r.key(pckey::F1);
    r.settle();
    CHECK(!r.error());
    r.home();
#else
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
#endif
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
    r.key(pckey::Down);                                   // Input/Output (first Setup item since v1.10.0) -> Angolo
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
