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
    r.key(pckey::Down);                                   // Input/Output (first Setup item since v1.10.0) -> Angolo
    r.key(pckey::Right);                                  // Angle: Rad -> Gra
    CHECK(ws.settings.angle == AngleMode::Gradians);
    r.key(pckey::Left); r.key(pckey::Left);               // -> Deg
    CHECK(ws.settings.angle == AngleMode::Degrees);
    r.key(pckey::Esc);
    CHECK(r.fw.apps().activeId() == AppId::MainMenu);
    r.type("k"); CHECK(r.fw.apps().activeId() == AppId::Conversion);
    // v1.14.0: elenco delle grandezze con la ricerca; Lunghezza parte da m
    r.type("Lunghezza"); r.key(pckey::Enter);
    {
        auto* cv = r.app<app::ConversionApp>();
        CHECK(cv->screen() == app::ConversionApp::Screen::Convert && cv->fromName() == "m");
        r.key(pckey::Down); r.key(pckey::Down);           // campo A
        r.type("km"); r.key(pckey::Enter);                // elenco filtrato: km
        CHECK(cv->toName() == "km");
        r.key(pckey::Up); r.key(pckey::Up);               // campo Valore
        r.enter("2.5");                                    // EXE converte
        CHECK_NEAR(ws.memory.answer(), 0.0025, 1e-15);
        r.key(pckey::Esc); r.key(pckey::Esc);
    }
    r.home();
    // ---------------- CONVERSION v1.14.0: categorie, SWAP, ALL, PREFIX, Ans, esatti, valute, errori
    {
        using CA = app::ConversionApp;
        r.type("k");
        auto* cv = r.app<CA>();
        CHECK(cv->screen() == CA::Screen::Menu && cv->menuRows() > 100);   // gruppi + 97 voci (+ recenti)
        r.type("Temperatura"); r.key(pckey::Enter);                        // ricerca: prima voce trovata
        CHECK(cv->screen() == CA::Screen::Convert && cv->fromName() == "K");
        r.key(pckey::Down); r.type("C"); r.key(pckey::Enter);   // Da: °C (simbolo identico in cima)
        r.key(pckey::Down); r.type("F"); r.key(pckey::Enter);   // A: °F
        CHECK(cv->fromName() == "\xC2\xB0" "C" && cv->toName() == "\xC2\xB0" "F");
        r.key(pckey::Up); r.key(pckey::Up); r.enter("100");
        CHECK(!r.error() && cv->result() == "212");
        CHECK_NEAR(ws.memory.answer(), 212, 1e-12);                       // stessa formula della v1.13.1 (211.99999999999997)
        r.key(pckey::F2);                                                  // SWAP: 100 °F -> °C
        CHECK(cv->fromName() == "\xC2\xB0" "F" && cv->toName() == "\xC2\xB0" "C");
        CHECK_NEAR(ws.memory.answer(), 37.77777777777778, 1e-12);
        CHECK(cv->exactResult() == "340/9");                               // forma esatta
        r.key(pckey::F3);                                                  // ALL: 100 °F in tutte le 8 unita'
        CHECK(cv->screen() == CA::Screen::All && cv->allList().rows.size() == 8);
        CHECK(cv->allList().rows[1].first == "K");
        r.key(pckey::Esc);
        CHECK(cv->screen() == CA::Screen::Convert);
        r.key(pckey::Esc);                                                 // F6/EXIT: elenco
        CHECK(cv->screen() == CA::Screen::Menu);
        // Cambio di categoria: Dati digitali, prefisso IEC con F4, valore come espressione con Ans e A-Z
        r.type("dati d"); r.key(pckey::Enter);
        CHECK(cv->fromName() == "bit");
        r.key(pckey::Down); r.type("B"); r.key(pckey::Enter);            // Da: byte
        r.key(pckey::F4);                                                  // PREFIX su Da
        CHECK(cv->screen() == CA::Screen::Prefix);
        r.type("gibi"); r.key(pckey::Enter);
        CHECK(cv->fromName() == "GiB");
        r.key(pckey::Down); r.type("MiB"); r.key(pckey::Enter);
        ws.memory.store('A', 2);
        r.key(pckey::Up); r.key(pckey::Up); r.enter("A+1");
        CHECK(!r.error() && cv->result() == "3072" && ws.memory.answer() == 3072);
        r.key(pckey::F5);                                                  // COPY negli appunti
        CHECK(r.fw.apps().context().clipboard != nullptr);
        // Errori senza crash: testo nel campo numerico, unita' incompatibili non selezionabili
        r.key(pckey::Enter); r.key(pckey::Break); r.type("abc+"); r.key(pckey::Enter);
        CHECK(r.error() && r.fw.apps().errorTitle() == "Syntax ERROR");
        r.key(pckey::Esc); r.key(pckey::Esc);
        CHECK(!r.error());
        r.key(pckey::Esc);
        // Formati: numeri romani fuori intervallo -> Argument ERROR
        r.type("romani"); r.key(pckey::Enter);
        r.type("4000"); r.key(pckey::Enter);
        CHECK(r.error() && r.fw.apps().errorTitle() == "Argument ERROR");
        r.key(pckey::Esc);
        r.key(pckey::Down); r.key(pckey::Down); r.key(pckey::Right);        // A: romano
        r.key(pckey::Up); r.key(pckey::Up); r.key(pckey::Enter); r.key(pckey::Break); r.type("2026"); r.key(pckey::Enter);
        CHECK(!r.error() && cv->result() == "MMXXVI");
        r.key(pckey::Esc);
        // Valute: tasso non impostato -> Argument ERROR; con F4 TASSI il tasso viene salvato
        r.type("valute"); r.key(pckey::Enter);
        CHECK(cv->fromName() == "EUR" && cv->toName() == "USD");
        r.key(pckey::F1);
        CHECK(r.error() && r.fw.apps().errorTitle() == "Argument ERROR");
        r.key(pckey::Esc);
        r.key(pckey::F4);                                                  // TASSI: USD per 1 EUR
        CHECK(cv->screen() == CA::Screen::Rates);
        r.enter("1.08");
        CHECK(ws.settings.conversion.rates[1] == 1.08);
        r.key(pckey::Esc);
        r.key(pckey::Up); r.key(pckey::Up); r.key(pckey::Up); r.enter("100");
        CHECK(!r.error() && cv->result() == "108");
        r.key(pckey::Esc);
        // Preferiti e recenti nel workspace (record v10)
        CHECK(!ws.settings.conversion.recent.empty() && ws.settings.conversion.recent[0].entry == 0x0200);
        r.key(pckey::F2);                                                  // la prima voce (recente) tra i preferiti
        CHECK(ws.settings.conversion.favorites.size() == 1);
        const auto blob = serializeWorkspace(ws);
        Workspace back;
        CHECK(deserializeWorkspace(blob.data(), blob.size(), back) && back.settings.conversion.favorites == ws.settings.conversion.favorites &&
              back.settings.conversion.rates[1] == 1.08 && back.settings.conversion.recent.size() == ws.settings.conversion.recent.size());
        r.home();
    }
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
