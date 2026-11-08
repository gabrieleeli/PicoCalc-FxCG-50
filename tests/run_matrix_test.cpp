// Run-Matrix: exact results and S<->D, mixed fractions, repeating decimals,
// Ans/variables, history editing with recalculation, shared variables, result
// views, persistence (including v1.6.0 records) and complete keyboard flows
// through the real Firmware class.
#include "apps.hpp"
#include "board_host.hpp"
#include "board_platform.hpp"
#include "board_port.hpp"
#include "firmware.hpp"
#include "run_matrix.hpp"
#include "test_util.hpp"
#include <algorithm>
#include <cmath>
#include <functional>
#include <string>

using namespace fx;
using app::AppId;

namespace {
const std::uint8_t kV1Record[] = {
#include "fixtures/workspace_v1_6_0.inc"
};
std::string calc(Workspace& ws, const char* e) { runCalculation(ws, e); return resultText(ws.history.back(), ws.settings); }
std::string toggled(Workspace& ws) { (void)toggleExactDecimal(ws.history.back(), ws.settings); return resultText(ws.history.back(), ws.settings); }
bool treeHas(const ast::Node& n, const std::function<bool(const ast::Node&)>& pred) {
    if (pred(n)) return true;
    for (const auto& c : n.children) if (c && treeHas(*c, pred)) return true;
    return false;
}
std::uint16_t press(std::uint8_t code, std::uint8_t state = 1) { return static_cast<std::uint16_t>((state << 8) | code); }
struct Rig {
    BoardPlatform platform;
    Firmware fw{platform, board::storageFlash()};
    Rig() { fw.boot(); frame(); }
    void frame() { if (fw.step()) { fw.draw(board::graphics()); board::present(); } }
    void raw(std::uint16_t r) { fw.rawKey(r); board::host::advance(20); frame(); }
    void key(std::uint8_t code) { raw(press(code)); raw(press(code, 3)); }
    void shift(std::uint8_t code) { key(pckey::F7); key(code); }
    void type(const std::string& s) { for (char c : s) raw(press(static_cast<std::uint8_t>(c))); }
    void enter(const std::string& s) { type(s); key(pckey::Enter); }
    Workspace& ws() { return fw.workspace(); }
    app::RunMatrixApp* rm() { return static_cast<app::RunMatrixApp*>(fw.apps().active()); }
    std::string last() { return resultText(ws().history.back(), ws().settings); }
};
} // namespace

int main() {
    // ---------------------------------------------------------------- fraction <-> decimal (task examples)
    {
        Workspace ws;
        CHECK_EQ_STR(calc(ws, "(1)/(2)"), "1/2");       CHECK_EQ_STR(toggled(ws), "0.5");   CHECK_EQ_STR(toggled(ws), "1/2");
        CHECK_EQ_STR(calc(ws, "(3)/(4)"), "3/4");       CHECK_EQ_STR(toggled(ws), "0.75");
        CHECK_EQ_STR(calc(ws, "(1)/(3)"), "1/3");       CHECK_EQ_STR(toggled(ws), "0.3333333333");
        CHECK(togglePeriodic(ws.history.back()));       CHECK_EQ_STR(resultText(ws.history.back(), ws.settings), "0.(3)");
        CHECK_EQ_STR(calc(ws, "0.25"), "0.25");         CHECK_EQ_STR(toggled(ws), "1/4");    CHECK_EQ_STR(toggled(ws), "0.25");
        CHECK_EQ_STR(calc(ws, "0.125"), "0.125");       CHECK_EQ_STR(toggled(ws), "1/8");
        CHECK_EQ_STR(calc(ws, "-(1)/(3)"), "-1/3");     CHECK_EQ_STR(toggled(ws), "-0.3333333333");
        CHECK_EQ_STR(calc(ws, "1/6"), "1/6");
        CHECK(togglePeriodic(ws.history.back()));       CHECK_EQ_STR(resultText(ws.history.back(), ws.settings), "0.1(6)");
        CHECK_EQ_STR(calc(ws, "22/7"), "22/7");
        CHECK(togglePeriodic(ws.history.back()));       CHECK_EQ_STR(resultText(ws.history.back(), ws.settings), "3.(142857)");
        CHECK(togglePeriodic(ws.history[0]));                               // 1/2: rational, period view available
        // The toggles change only the view: value, Ans and the exact form stay the same.
        const double before = ws.memory.answer();
        CHECK(toggleExactDecimal(ws.history.back(), ws.settings));
        CHECK(ws.memory.answer() == before && ws.history.back().exact.valid);
        CHECK_NEAR(ws.history.back().value.real(), 22.0 / 7.0, 1e-15);
        // Mixed fractions: Frac Result ab/c and the per-result toggle.
        ws.settings.mixedFractions = true;
        CHECK_EQ_STR(calc(ws, "22/7"), "3 1/7");
        CHECK(toggleMixedFraction(ws.history.back(), ws.settings));
        CHECK_EQ_STR(resultText(ws.history.back(), ws.settings), "22/7");
        CHECK_EQ_STR(calc(ws, "-7/3"), "-2 1/3");
        CHECK_EQ_STR(calc(ws, "2/3"), "2/3");
        CHECK(!toggleMixedFraction(ws.history.back(), ws.settings));        // proper fraction: nothing to convert
        ws.settings.mixedFractions = false;
        // fx-CG50 manual 2-23: 2/5 + 3 1/4 = 73/20 = 3.65
        CHECK_EQ_STR(calc(ws, "(2)/(5)+mixed(3,1,4)"), "73/20");
        CHECK(toggleMixedFraction(ws.history.back(), ws.settings));
        CHECK_EQ_STR(resultText(ws.history.back(), ws.settings), "3 13/20");
        CHECK(toggleExactDecimal(ws.history.back(), ws.settings));
        CHECK_EQ_STR(resultText(ws.history.back(), ws.settings), "3.65");
        // More than 10 characters: decimal display; the exact value is kept and S<->D shows it.
        CHECK_EQ_STR(calc(ws, "1/2578+1/4572"), "6.066202547E-04");
        CHECK_EQ_STR(toggled(ws), "3575/5893308");
        // Fractions mixed with decimals are shown as decimals (fx-CG50), S<->D converts them.
        CHECK_EQ_STR(calc(ws, "(1)/(2)*0.5"), "0.25");  CHECK_EQ_STR(toggled(ws), "1/4");
        CHECK_EQ_STR(calc(ws, "0.1+0.2"), "0.3");       CHECK_EQ_STR(toggled(ws), "3/10");
        // Irrational and numerical results never become fractions.
        CHECK_EQ_STR(calc(ws, "sin(1)"), "0.8414709848");
        CHECK(!toggleExactDecimal(ws.history.back(), ws.settings));
        CHECK_EQ_STR(calc(ws, "pi^2"), "9.869604401");
        CHECK(!toggleExactDecimal(ws.history.back(), ws.settings));
        CHECK_EQ_STR(calc(ws, "0.3333333333"), "0.3333333333");             // typed decimal: exactly 3333333333/10^10
        CHECK(toggleExactDecimal(ws.history.back(), ws.settings));
        CHECK_EQ_STR(resultText(ws.history.back(), ws.settings), "3333333333/10000000000");
        CHECK(!(ws.history.back().exact.value == exact::Value::fromRational({1, 3})));   // not silently 1/3
        CHECK_EQ_STR(calc(ws, "integral(x^2,0,3)"), "9");
        CHECK(ws.history.back().numerical && isApproximation(ws.history.back(), ws.settings));
        CHECK(treeHas(*resultTree(ws.history.back(), ws.settings), [](const ast::Node& n) { return n.text == "\xE2\x89\x88"; }));
        CHECK(!toggleExactDecimal(ws.history.back(), ws.settings));
        // Repeating decimal input: 0.1666... (written 0.1 + period 6) is exactly 1/6.
        CHECK_EQ_STR(calc(ws, "0.1_6"), "1/6");
        CHECK_EQ_STR(calc(ws, "0._9"), "1");
        CHECK_THROWS_AS(calc(ws, "3_3"), SyntaxError);
        // Surds and pi in natural form (fx-CG50 manual 2-4, 2-5).
        CHECK_EQ_STR(calc(ws, "sqrt(2)+sqrt(8)"), "3\xE2\x88\x9A" "2");
        CHECK_EQ_STR(calc(ws, "2(3-2sqrt(5))"), "6-4\xE2\x88\x9A" "5");
        CHECK_EQ_STR(calc(ws, "35sqrt(2)*3"), "148.492424");               // 105√2 is outside the display range
        CHECK_EQ_STR(toggled(ws), "105\xE2\x88\x9A" "2");
        CHECK_EQ_STR(calc(ws, "sqrt(2)+sqrt(3)+sqrt(6)"), "5.595754113");
        CHECK_EQ_STR(calc(ws, "78pi*2"), "156\xCF\x80");
        CHECK_EQ_STR(calc(ws, "123456pi*9"), "3490636.164");
        ws.settings.angle = AngleMode::Degrees;
        CHECK_EQ_STR(calc(ws, "sin(60)"), "\xE2\x88\x9A" "3/2");
        CHECK_EQ_STR(calc(ws, "2sin(45)cos(65)"), "0.5976724775");        // manual 2-16
        ws.settings.angle = AngleMode::Radians;
        CHECK_EQ_STR(calc(ws, "cos(pi/3)"), "1/2");                       // manual 2-16
        // Display setting: exact forms off -> decimals, S<->D still gives the exact form.
        ws.settings.fractionResults = false;
        CHECK_EQ_STR(calc(ws, "1/3"), "0.3333333333");
        CHECK_EQ_STR(toggled(ws), "1/3");
        ws.settings.fractionResults = true;
    }
    // ---------------------------------------------------------------- Ans, variables, store
    {
        Workspace ws;
        calc(ws, "1/3");
        CHECK_EQ_STR(calc(ws, "Ans*3"), "1");
        CHECK(ws.history.back().exact.valid);
        calc(ws, "1/7\xE2\x86\x92" "A");
        CHECK_EQ_STR(calc(ws, "7A"), "1");
        CHECK_EQ_STR(calc(ws, "A+1"), "8/7");
        CHECK_EQ_STR(calc(ws, "PreAns"), "1");
        calc(ws, "0.5");
        CHECK_EQ_STR(calc(ws, "Ans+1/3"), "0.8333333333");                 // decimal input propagates through Ans
        CHECK_EQ_STR(toggled(ws), "5/6");
        calc(ws, "10->A~C");
        CHECK(ws.memory.recall('A') == 10 && ws.memory.recall('B') == 10 && ws.memory.recall('C') == 10 && ws.memory.recall('D') == 0);
        CHECK_THROWS_AS(calc(ws, "1->C~A"), SyntaxError);
        CHECK_THROWS_AS(calc(ws, "1->12"), SyntaxError);
        calc(ws, "sqrt(2)->B");
        CHECK_EQ_STR(calc(ws, "B^2"), "2");
        calc(ws, "sin(1)->B");                                              // approximate value: no exact form
        CHECK(!ws.memory.exactSlot('B').valid);
        // Reusing a result keeps it exact.
        calc(ws, "2/3");
        const std::string again = resultExpression(ws.history.back());
        CHECK_EQ_STR(again, "(2)/(3)");
        CHECK_EQ_STR(calc(ws, again.c_str()), "2/3");
        // Shared variables owned by Graph and Table (read through the workspace).
        ws.view.xmin = -5; ws.table.step = 0.5;
        CHECK_EQ_STR(calc(ws, "Xmin*2"), "-10");
        CHECK_EQ_STR(calc(ws, "tblpitch"), "0.5");
        CHECK_THROWS_AS(evaluate("xmin+1"), MathError);                     // no workspace bound: not available
        // Engine operators reachable from OPTN.
        CHECK_EQ_STR(calc(ws, "107Int\xC3\xB7" "7"), "15");                 // manual 2-25
        CHECK_EQ_STR(calc(ws, "107Rmdr7"), "2");
        CHECK_EQ_STR(calc(ws, "modexp(5,3,3)"), "2");                       // manual 2-22
        CHECK_EQ_STR(calc(ws, "gcd(28,35)"), "7");
        CHECK_EQ_STR(calc(ws, "lcm(9,15)"), "45");
        calc(ws, "3->A"); calc(ws, "2->B");
        CHECK_EQ_STR(calc(ws, "A and B"), "1");                             // manual 2-24
        CHECK_EQ_STR(calc(ws, "(A>B) xor (B>A)"), "1");
        CHECK_EQ_STR(calc(ws, "not(A=3)"), "0");
        CHECK_EQ_STR(calc(ws, "2\xE2\x89\xA0" "3"), "1");
        CHECK_EQ_STR(calc(ws, "rndfix(200/7,3)*14"), "399.994");           // manual 2-2
        CHECK_EQ_STR(calc(ws, "pol(3,4)"), "r=5  \xCE\xB8=0.927295218");
        ws.settings.complex = ComplexMode::Rectangular;
        CHECK_EQ_STR(calc(ws, "sqrt(-4)"), "2i");
        CHECK(!ws.history.back().exact.valid);
        ws.settings.complex = ComplexMode::Real;
        CHECK_THROWS_AS(calc(ws, "1/0"), MathError);
        CHECK_THROWS_AS(calc(ws, "1+"), SyntaxError);
    }
    // ---------------------------------------------------------------- history editing (manual 1-24)
    {
        Workspace ws;
        calc(ws, "1+2"); calc(ws, "Ans*2"); calc(ws, "Ans+1/2");
        CHECK(editHistoryEntry(ws, 0, "1+3") == 0);
        CHECK(ws.history[0].value.real() == 4 && ws.history[1].value.real() == 8);
        CHECK_EQ_STR(resultText(ws.history[2], ws.settings), "17/2");
        CHECK(ws.memory.answer() == 8.5 && ws.memory.previousAnswer() == 8);
        // The first entry sees Ans = 0, as on the fx-CG50.
        CHECK(editHistoryEntry(ws, 0, "Ans+5") == 0);
        CHECK(ws.history[0].value.real() == 5 && ws.history[1].value.real() == 10);
        // An error in the edited line restores everything and is reported.
        const auto snapshot = ws.history;
        CHECK_THROWS_AS(editHistoryEntry(ws, 1, "Ans/0"), MathError);
        CHECK(ws.history.size() == snapshot.size() && ws.history[1].expression == snapshot[1].expression && ws.memory.answer() == 10.5);
        CHECK_THROWS_AS(editHistoryEntry(ws, 1, "2+"), SyntaxError);
        // Later lines that fail keep an error message.
        Workspace w2;
        calc(w2, "3"); calc(w2, "1/(Ans-2)");
        CHECK(editHistoryEntry(w2, 0, "2") == 1);
        CHECK(!w2.history[1].error.empty() && w2.history[1].error.find("Math ERROR") == 0);
        // Stores are replayed.
        Workspace w3;
        calc(w3, "5->A"); calc(w3, "A+1");
        CHECK(editHistoryEntry(w3, 0, "7->A") == 0);
        CHECK(w3.memory.recall('A') == 7 && w3.history[1].value.real() == 8);
    }
    // ---------------------------------------------------------------- 2D result trees
    {
        Workspace ws;
        calc(ws, "1/3");
        CHECK(treeHas(*resultTree(ws.history.back(), ws.settings), [](const ast::Node& n) { return n.kind == ast::Kind::Fraction; }));
        calc(ws, "1/sqrt(2)");
        const auto t = resultTree(ws.history.back(), ws.settings);
        CHECK(treeHas(*t, [](const ast::Node& n) { return n.kind == ast::Kind::Radical; }) && treeHas(*t, [](const ast::Node& n) { return n.kind == ast::Kind::Fraction; }));
        calc(ws, "1/6");
        (void)togglePeriodic(ws.history.back());
        CHECK(treeHas(*resultTree(ws.history.back(), ws.settings), [](const ast::Node& n) { return n.kind == ast::Kind::Call && n.text == "overline"; }));
        calc(ws, "10^12");
        CHECK_EQ_STR(resultText(ws.history.back(), ws.settings), "1E+12");
    }
    // ---------------------------------------------------------------- persistence
    {
        Workspace ws;
        ws.settings.mixedFractions = true;
        calc(ws, "1/3->A"); calc(ws, "sqrt(8)"); calc(ws, "0.75"); calc(ws, "integral(x,0,1)");
        (void)toggleExactDecimal(ws.history[2], ws.settings);
        const auto bytes = serializeWorkspace(ws);
        Workspace back;
        CHECK(deserializeWorkspace(bytes.data(), bytes.size(), back));
        CHECK(back.settings.mixedFractions);
        exact::Value a; bool dec = true;
        CHECK(back.memory.exactRecall('A', a, dec) && a == exact::Value::fromRational({1, 3}) && !dec);
        CHECK_EQ_STR(resultText(back.history[1], back.settings), "2\xE2\x88\x9A" "2");
        CHECK_EQ_STR(resultText(back.history[2], back.settings), "3/4");
        CHECK(back.history[3].numerical);
        CHECK_EQ_STR(calc(back, "Ans*2"), "1");                             // Ans = numerical integral 0.5: approximate
        CHECK(!back.history.back().exact.valid);
        CHECK_EQ_STR(calc(back, "3A"), "1");
        // v1.6.0 records are still accepted.
        Workspace old;
        CHECK(deserializeWorkspace(kV1Record, sizeof(kV1Record), old));
        CHECK(old.settings.angle == AngleMode::Degrees && !old.settings.mixedFractions);
        CHECK(old.memory.recall('A') == 2.5 && old.memory.recall('C') == -7);
        CHECK_NEAR(old.memory.answer(), 1.0 / 3.0, 1e-15);
        CHECK(old.memory.previousAnswer() == 9);
        CHECK(old.history.size() == 2 && old.history[1].view == ViewDecimal && old.lists[0].size() == 3);
        exact::Value e; bool d = false;
        CHECK(!old.memory.exactAnswer(e, d));                               // old Ans has no exact form: never invented
        CHECK(!old.history[0].exact.valid);
        CHECK_EQ_STR(resultText(old.history[0], old.settings), "0.3333333333");
        CHECK(recalculateHistory(old, 0) == 0);                             // recalculation restores the exact form
        CHECK_EQ_STR(resultText(old.history[0], old.settings), "1/3");
        // Corrupted exact data is rejected: locate the slot of A = 1/3 (flags, count, num, den, radicand).
        Workspace one;
        one.memory.storeExact('A', 1.0 / 3.0, exact::Value::fromRational({1, 3}), false);
        auto blob = serializeWorkspace(one);
        const std::uint8_t pattern[] = {1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0};
        std::size_t at = 0;
        for (std::size_t i = 0; i + sizeof(pattern) <= blob.size() && !at; ++i)
            if (std::equal(pattern, pattern + sizeof(pattern), blob.begin() + static_cast<std::ptrdiff_t>(i))) at = i;
        CHECK(at > 0);
        Workspace target;
        CHECK(deserializeWorkspace(blob.data(), blob.size(), target));
        auto zeroDen = blob; zeroDen[at + 10] = 0;                          // 1/0
        CHECK(!deserializeWorkspace(zeroDen.data(), zeroDen.size(), target));
        auto notReduced = blob; notReduced[at + 2] = 2; notReduced[at + 10] = 6;   // 2/6 is not canonical
        CHECK(!deserializeWorkspace(notReduced.data(), notReduced.size(), target));
        auto tooMany = blob; tooMany[at + 1] = 3;                           // three terms
        CHECK(!deserializeWorkspace(tooMany.data(), tooMany.size(), target));
    }
    // ---------------------------------------------------------------- keyboard flows through the firmware
    board::init();
    {
        Rig r;
        r.key(pckey::Enter);                                                // Run-Matrix
        CHECK(r.fw.apps().activeId() == AppId::RunMatrix);
        auto& ws = r.ws();
        ws.history.clear();
        // F1 fraction, F5 S<->D, F5 again.
        r.key(pckey::F1); r.type("1"); r.key(pckey::Down); r.type("3"); r.key(pckey::Enter);
        CHECK_EQ_STR(r.last(), "1/3");
        r.key(pckey::F5); CHECK_EQ_STR(r.last(), "0.3333333333");
        r.key(pckey::F5); CHECK_EQ_STR(r.last(), "1/3");
        // SHIFT F1: mixed fraction template 2 1/3; SHIFT F5 converts ab/c <-> d/c.
        r.shift(pckey::F1); r.type("2"); r.key(pckey::Right); r.type("1"); r.key(pckey::Right); r.type("3"); r.key(pckey::Enter);
        CHECK_EQ_STR(r.last(), "7/3");
        r.shift(pckey::F5); CHECK_EQ_STR(r.last(), "2 1/3");
        CHECK(!r.fw.apps().context().shift());
        // Decimal input and S<->D.
        r.enter("0.25"); CHECK_EQ_STR(r.last(), "0.25");
        r.key(pckey::F5); CHECK_EQ_STR(r.last(), "1/4");
        // Irrational: S<->D explains that there is no other form (toast), value unchanged.
        r.enter("sin(2)"); r.key(pckey::F5); CHECK_EQ_STR(r.last(), "0.9092974268");
        // MATH > periodo after "0.1": 0.1(6) = 1/6.
        r.type("0.1"); r.key(pckey::F4); r.key(pckey::Up); r.key(pckey::Enter); r.type("6"); r.key(pckey::Enter);
        CHECK_EQ_STR(r.last(), "1/6");
        // History editing in place (fx-CG50 history function).
        ws.history.clear();
        r.enter("1+2"); r.enter("Ans*2");
        r.key(pckey::Up); r.key(pckey::Up);
        CHECK(r.rm()->browsing() == 0);
        r.key(pckey::Enter);
        CHECK(r.rm()->editing() == 0);
        CHECK_EQ_STR(r.rm()->editor().toExpression(), "1+2");
        r.key(pckey::Backspace); r.type("3"); r.key(pckey::Enter);
        CHECK(r.rm()->editing() == -1);
        CHECK(ws.history[0].value.real() == 4 && ws.history[1].value.real() == 8);
        // EXIT cancels an edit and keeps what was typed in the input line.
        r.type("5+");
        r.key(pckey::Up);                                                    // cursor at top level: browse
        CHECK(r.rm()->browsing() == 1);
        r.key(pckey::F1); CHECK(r.rm()->editing() == 1);
        r.type("+1");
        r.key(pckey::Esc);
        CHECK(r.rm()->editing() == -1 && ws.history[1].expression == "Ans*2");
        r.key(pckey::Esc);
        CHECK_EQ_STR(r.rm()->editor().toExpression(), "5+");
        r.key(pckey::Break);
        // Browse: F2 reuses the expression, F3 the exact result.
        r.enter("2/3");
        r.key(pckey::Up); r.key(pckey::F2);
        CHECK_EQ_STR(r.rm()->editor().toExpression(), "2/3");               // the line as it was typed
        r.key(pckey::Break);
        r.key(pckey::Up); r.key(pckey::F3); r.type("+1/3"); r.key(pckey::Enter);
        CHECK_EQ_STR(r.last(), "1");
        // Errors in an edited line: the error box opens, the edit stays open for correction.
        r.key(pckey::Up); r.key(pckey::Enter);
        r.key(pckey::Backspace); r.key(pckey::Backspace); r.key(pckey::Backspace); r.type("/0");
        r.key(pckey::Enter);
        CHECK(r.fw.apps().errorShown());
        r.key(pckey::Esc);
        CHECK(r.rm()->editing() >= 0);
        r.key(pckey::Break);
        CHECK(r.rm()->editing() == -1);
        // VARS > V-WIN > Xmin inserts the shared variable.
        ws.view.xmin = -6.3;
        r.key(pckey::F9); r.type("3"); r.key(pckey::Enter);
        CHECK_EQ_STR(r.rm()->editor().toExpression(), "xmin");
        r.key(pckey::Enter);
        CHECK_EQ_STR(r.last(), "-6.3");
        // VARS > A~Z shows exact values and inserts the letter.
        r.enter("1/9->Q");
        r.key(pckey::F9); r.key(pckey::Enter);
        for (int i = 0; i < 16; ++i) r.key(pckey::Down);                   // Q
        r.key(pckey::Enter); r.type("*9"); r.key(pckey::Enter);
        CHECK_EQ_STR(r.last(), "1");
        // OPTN > LOGIC > and; OPTN > RELAZIONI > "≥".
        r.type("1"); r.key(pckey::F8); r.type("8"); r.key(pckey::Enter); r.type("0"); r.key(pckey::Enter);
        CHECK_EQ_STR(r.last(), "0");
        r.type("3"); r.key(pckey::F8); r.type("9"); r.type("5"); r.type("2"); r.key(pckey::Enter);
        CHECK_EQ_STR(r.last(), "1");
        // OPTN > RISULTATO > ▶Periodo on the last result.
        r.enter("1/7");
        r.key(pckey::F8); r.key(pckey::Up); r.key(pckey::Up); r.key(pckey::Enter); r.type("3");
        CHECK_EQ_STR(r.last(), "0.(142857)");
        // Persisted exactly across a reboot.
        r.key(pckey::Tab); r.frame();
    }
    {
        Rig again;
        auto& ws = again.ws();
        CHECK(again.fw.loadedFromFlash());
        CHECK(!ws.history.empty());
        exact::Value q; bool d = false;
        CHECK(ws.memory.exactRecall('Q', q, d) && q == exact::Value::fromRational({1, 9}));
        CHECK_EQ_STR(resultText(ws.history.back(), ws.settings), "0.(142857)");
    }
    return testutil::finish("run_matrix_test");
}
