// EQUATION v1.12.0: parita' con il capitolo 4 del manuale fx-CG50 (4-1...4-5)
// e le parti collegate (1-29 Math, 1-33 VARS › EQUATION, 1-35/1-36 SET UP,
// tabella degli errori α-1...α-3). Un gruppo di verifiche per ogni punto del
// §4 della richiesta, con gli esempi del manuale come casi attesi.
#include "app_rig.hpp"
#include "equation.hpp"
#include "expression.hpp"
#include "math_renderer.hpp"
#include "natural_editor.hpp"
#include "run_matrix.hpp"
#include "solvers.hpp"
#include "test_util.hpp"
#include "workspace.hpp"
#include <cmath>
#include <cstring>

using namespace fx;
using namespace fx::eq;
using testrig::Rig;
using app::EquationApp;

namespace {
const std::uint8_t kRecordV7[] = {
#include "fixtures/workspace_v1_11_0.inc"
};

struct Calc {
    CalculatorMemory mem;
    EvalContext ec;
    explicit Calc(ComplexMode mode = ComplexMode::Rectangular) { ec.memory = &mem; ec.complexMode = mode; }
    Number n(const char* text) const { return evaluateNumber(text, ec); }
    std::vector<Number> v(std::initializer_list<const char*> texts) const { std::vector<Number> out; for (const char* t : texts) out.push_back(n(t)); return out; }
};
View rect() { View v; v.complex = ComplexMode::Rectangular; return v; }
View real() { View v; v.complex = ComplexMode::Real; return v; }
View polar() { View v; v.complex = ComplexMode::Polar; return v; }
View line() { View v; v.complex = ComplexMode::Rectangular; v.natural = false; return v; }
std::string rootsText(const std::vector<Root>& r, const View& v) {
    std::string s;
    for (const Root& x : r) { if (!s.empty()) s += " | "; s += numberText(x.value, v); if (x.multiplicity > 1) s += " x" + std::to_string(x.multiplicity); }
    return s;
}
std::string formulas(const SimulResult& r, const View& v) {
    static const char* const names3[] = {"X", "Y", "Z"};
    static const char* const names6[] = {"X1", "X2", "X3", "X4", "X5", "X6"};
    std::string s;
    for (int k = 0; k < r.unknowns; ++k) { if (k) s += " ; "; s += formulaText(r, k, r.unknowns <= 3 ? names3 : names6, v); }
    return s;
}
bool hasKind(const ast::Node& n, ast::Kind k) {
    if (n.kind == k) return true;
    for (const auto& c : n.children) if (c && hasKind(*c, k)) return true;
    return false;
}
// Pressione di un tasto con SHIFT (PicoCalc F7).
void shifted(Rig& r, std::uint8_t code) { r.key(pckey::F7); r.key(code); }
} // namespace

int main() {
    board::init();

    // ================================================================ 4.2 coefficienti come espressioni
    {
        Calc c;
        c.mem.storeExact('A', 1.0 / 3.0, exact::Value::fromRational({1, 3}), false);
        CHECK_EQ_STR(numberText(c.n("1/3"), rect()), "1/3");
        CHECK(c.n("1/3").hasExact && !c.n("1/3").decimal);
        CHECK_EQ_STR(numberText(c.n("sqrt(12)"), rect()), "2\xE2\x88\x9A" "3");
        CHECK_EQ_STR(numberText(c.n("pi/2"), rect()), "\xCF\x80/2");
        CHECK_EQ_STR(numberText(c.n("3A"), rect()), "1");                           // variabile con la sua forma esatta
        CHECK_EQ_STR(numberText(c.n("2+i"), rect()), "2+i");
        CHECK_EQ_STR(numberText(c.n("(1+sqrt(3)i)/2"), rect()), "(1+\xE2\x88\x9A" "3i)/2");
        CHECK_EQ_STR(numberText(c.n("(1+i)^2"), rect()), "2i");                      // polinomio in i: forma esatta
        CHECK(!c.n("1/(1+i)").hasExact && std::abs(c.n("1/(1+i)").value - Complex(0.5, -0.5)) < 1e-15);
        CHECK(c.n("0.5").decimal && numberText(c.n("0.5"), rect()) == "0.5");
        CHECK(!c.n("sin(1)").hasExact);
        // Ans e' quello di Run-Matrix
        c.mem.setAnswerExact(0.25, exact::Value::fromRational({1, 4}), false);
        CHECK_EQ_STR(numberText(c.n("Ans"), rect()), "1/4");
        // Modo Real: un input con i si calcola in complesso (come l'editor MAT/VCT); √(−1) e' Non-Real
        Calc r(ComplexMode::Real);
        CHECK(r.n("3i").value == Complex(0, 3));
        CHECK_THROWS_AS(r.n("sqrt(-1)"), NonRealError);
        CHECK_THROWS_AS(r.n("1+"), SyntaxError);
        CHECK_THROWS_AS(r.n("{1,2}"), TypeError);
        CHECK_THROWS_AS(r.n("1E100*10"), MathError);
        // Testo per EDIT: la forma esatta torna nell'editor e si rilegge uguale
        for (const char* t : {"1/3", "-2sqrt(3)/5", "pi/4", "2+i", "(1-sqrt(3)i)/2", "-i", "0.75", "7"}) {
            const Number a = c.n(t);
            const Number b = c.n(editText(a).c_str());
            ::testutil::report(std::abs(a.value - b.value) < 1e-15 && a.hasExact == b.hasExact, "editText round trip", __FILE__, __LINE__, t);
        }
        // Record precedenti: il double diventa il decimale piu' corto, senza perdita
        for (double d : {0.1, 1.0 / 3.0, -2.5, 1e-30, 123456789.125, 7.0}) {
            const Number s = fromStoredDouble(d);
            ::testutil::report(s.value.real() == d && (!s.hasExact || s.exact.re.toDouble() == d), "fromStoredDouble lossless", __FILE__, __LINE__, std::to_string(d));
        }
        CHECK(fromStoredDouble(0.1).hasExact && fromStoredDouble(-2.5).hasExact && fromStoredDouble(1.0 / 3.0).hasExact && fromStoredDouble(7).hasExact);
        CHECK_EQ_STR(numberText(fromStoredDouble(-2.5), rect()), "-2.5");
        CHECK(fromStoredDouble(0.1).decimal && !fromStoredDouble(7).decimal);
    }

    // ================================================================ 4.3 SIML
    {
        Calc c;
        // Esempio del manuale 4-2: 4x+y−2z=−1, x+6y+3z=1, −5x+4y+z=−7 -> 1, −1, 2
        SimulResult r = solveSimultaneous(c.v({"4", "1", "-2", "-1", "1", "6", "3", "1", "-5", "4", "1", "-7"}), 3);
        CHECK(r.kind == SimulResult::Kind::Unique && r.exactPath);
        CHECK_EQ_STR(formulas(r, rect()), "X=1 ; Y=-1 ; Z=2");
        // Frazioni esatte (Math) e decimali (Line)
        r = solveSimultaneous(c.v({"1", "2", "1/3", "3", "-1", "2"}), 2);
        CHECK_EQ_STR(formulas(r, rect()), "X=13/21 ; Y=-1/7");
        CHECK_EQ_STR(formulas(r, line()), "X=0.619047619 ; Y=-0.1428571429");
        // Radicali e π nei coefficienti
        r = solveSimultaneous(c.v({"sqrt(2)", "0", "2", "0", "1", "pi"}), 2);
        CHECK_EQ_STR(formulas(r, rect()), "X=\xE2\x88\x9A" "2 ; Y=\xCF\x80");
        // 2...6 incognite: matrice di Hilbert 6x6 (mal condizionata), termini noti = somme delle righe -> tutti 1
        {
            std::vector<Number> h;
            for (int i = 1; i <= 6; ++i) {
                exact::Rational sum{0, 1};
                for (int j = 1; j <= 6; ++j) { exact::Rational t; exact::Rational::make(1, i + j - 1, t); h.push_back(Number::fromRational(t)); (void)exact::add(sum, t, sum); }
                h.push_back(Number::fromRational(sum));
            }
            r = solveSimultaneous(h, 6);
            CHECK(r.kind == SimulResult::Kind::Unique && r.exactPath);
            bool ones = true;
            for (const Number& x : r.solution) ones = ones && x.hasExact && x.exact.re == exact::Value::fromInteger(1);
            CHECK(ones);
            // Lo stesso sistema con i decimali (10 cifre): risultato vicino a 1, mostrato in decimale
            std::vector<Number> d;
            for (const Number& x : h) { char buf[32]; std::snprintf(buf, sizeof(buf), "%.10f", x.value.real()); d.push_back(c.n(buf)); }
            r = solveSimultaneous(d, 6);
            CHECK(r.kind == SimulResult::Kind::Unique);
            CHECK((r.solution[0].decimal || !r.solution[0].hasExact) && std::abs(r.solution[0].value.real() - 1) < 1e-2);
        }
        for (int n = 2; n <= 6; ++n) {                     // diagonale: x_k = k
            std::vector<Number> a(static_cast<std::size_t>(n * (n + 1)));
            for (int k = 0; k < n; ++k) { a[static_cast<std::size_t>(k * (n + 1) + k)] = c.n("2"); a[static_cast<std::size_t>(k * (n + 1) + n)] = c.n(std::to_string(2 * (k + 1)).c_str()); }
            r = solveSimultaneous(a, n);
            CHECK(r.kind == SimulResult::Kind::Unique && r.solution[static_cast<std::size_t>(n - 1)].value == Complex(n, 0));
        }
        // Coefficienti grandi (nessuna forma esatta): in virgola mobile
        r = solveSimultaneous(c.v({"1E50", "2E50", "5E50", "3E50", "4E50", "6E50"}), 2);
        CHECK(r.kind == SimulResult::Kind::Unique && std::abs(r.solution[0].value.real() + 4) < 1e-12 && std::abs(r.solution[1].value.real() - 4.5) < 1e-12);
        // No Solution / Infinitely Many Solutions con la formula
        r = solveSimultaneous(c.v({"1", "1", "1", "1", "1", "2"}), 2);
        CHECK(r.kind == SimulResult::Kind::None && r.rref.size() == 6);
        r = solveSimultaneous(c.v({"1", "2", "3", "4", "2", "4", "6", "8", "1", "1", "1", "1"}), 3);
        CHECK(r.kind == SimulResult::Kind::Infinite);
        CHECK_EQ_STR(formulas(r, rect()), "X=-2+Z ; Y=3-2Z ; Z=Z");
        // Esempio del manuale 4-2 (coefficienti complessi): X=−½i, Y=1.4−1.8i−(2+i)Z, Z=Z
        r = solveSimultaneous(c.v({"2", "0", "0", "-i", "0", "1", "2+i", "1.4-1.8i", "0", "2", "4+2i", "2.8-3.6i"}), 3);
        CHECK(r.kind == SimulResult::Kind::Infinite);
        CHECK_EQ_STR(formulas(r, rect()), "X=-i/2 ; Y=1.4-1.8i-(2+i)Z ; Z=Z");
        CHECK(hasKind(*formulaTree(r, 0, std::array<const char*, 3>{"X", "Y", "Z"}.data(), rect()), ast::Kind::Fraction));
        // Soluzioni complesse e r∠θ
        r = solveSimultaneous(c.v({"1", "1", "2", "1", "-1", "2i"}), 2);
        CHECK_EQ_STR(formulas(r, rect()), "X=1+i ; Y=1-i");
        CHECK_EQ_STR(numberText(r.solution[0], polar()), "\xE2\x88\x9A" "2\xE2\x88\xA0\xCF\x80/4");
        // Ma ERROR fuori dall'intervallo, Underflow
        CHECK_THROWS_AS(solveSimultaneous(c.v({"1E-60", "0", "1E60", "0", "1", "1"}), 2), MathError);
        CHECK_THROWS_AS(solveSimultaneous(c.v({"1E60", "0", "1E-60", "0", "1", "1"}), 2), MathError);
        // Formati Fix / Sci / Eng
        View f = line(); f.display = {DisplayMode::Fix, 3};
        r = solveSimultaneous(c.v({"3", "0", "1", "0", "1", "2"}), 2);
        CHECK_EQ_STR(numberText(r.solution[0], f), "0.333");
        f.display = {DisplayMode::Sci, 3};
        CHECK_EQ_STR(numberText(r.solution[0], f), "3.33E-01");
        f.display = {DisplayMode::Eng, 0};
        CHECK_EQ_STR(numberText(r.solution[0], f), "333.3333333m");
        // S⇔D: una forma esatta diventa decimale e viceversa
        View d = rect(); d.mode = 1;
        CHECK_EQ_STR(numberText(r.solution[0], d), "0.3333333333");
        View e = line(); e.mode = 2;
        CHECK_EQ_STR(numberText(r.solution[0], e, true), "1\xE2\x8C\x9F" "3");
        CHECK(hasOtherForm(r.solution[0], line()) && !hasOtherForm(r.solution[1], line()));
        View mixed = rect(); mixed.mixed = true;
        CHECK_EQ_STR(numberText(c.n("7/3"), mixed), "2 1/3");
    }

    // ================================================================ 4.4 POLY
    {
        Calc c;
        // Manuale 4-3: x³−2x²−x+2=0 -> 2, 1, −1 (reali decrescenti)
        CHECK_EQ_STR(rootsText(solvePolynomial(c.v({"1", "-2", "-1", "2"})), rect()), "2 | 1 | -1");
        // Radici multiple: x³+3x²+3x+1=0 -> −1 ×3
        auto roots = solvePolynomial(c.v({"1", "3", "3", "1"}));
        CHECK(roots.size() == 1 && roots[0].multiplicity == 3 && roots[0].value.value == Complex(-1, 0));
        // Manuale 4-4: x³+2x²+3x+2=0 nei tre Complex Mode
        roots = solvePolynomial(c.v({"1", "2", "3", "2"}));
        CHECK_EQ_STR(rootsText(visibleRoots(roots, ComplexMode::Real), real()), "-1");
        CHECK_EQ_STR(rootsText(roots, rect()), "-1 | (-1+\xE2\x88\x9A" "7i)/2 | (-1-\xE2\x88\x9A" "7i)/2");
        View d = rect(); d.mode = 1;
        CHECK_EQ_STR(rootsText(roots, d), "-1 | -0.5+1.322875656i | -0.5-1.322875656i");
        CHECK_EQ_STR(numberText(roots[0].value, polar()), "1\xE2\x88\xA0\xCF\x80");      // 1∠π (manuale 4-4)
        View pd = polar(); pd.mode = 1;
        CHECK_EQ_STR(numberText(roots[1].value, pd), "1.414213562\xE2\x88\xA0" "1.932163451");
        View deg = polar(); deg.angle = AngleMode::Degrees;
        CHECK_EQ_STR(numberText(roots[0].value, deg), "1\xE2\x88\xA0" "180");
        // Manuale 1-29: x²+3x+5=0 -> (−3+√11 i)/2 come frazione 2D
        roots = solvePolynomial(c.v({"1", "3", "5"}));
        CHECK_EQ_STR(numberText(roots[0].value, rect()), "(-3+\xE2\x88\x9A" "11i)/2");
        CHECK(hasKind(*numberTree(roots[0].value, rect()), ast::Kind::Fraction) && hasKind(*numberTree(roots[0].value, rect()), ast::Kind::Radical));
        // Radicali reali, biquadratiche, grado 6
        CHECK_EQ_STR(rootsText(solvePolynomial(c.v({"1", "-1", "-1"})), rect()), "(1+\xE2\x88\x9A" "5)/2 | (1-\xE2\x88\x9A" "5)/2");
        CHECK_EQ_STR(rootsText(solvePolynomial(c.v({"1", "0", "-5", "0", "6"})), rect()), "\xE2\x88\x9A" "3 | \xE2\x88\x9A" "2 | -\xE2\x88\x9A" "2 | -\xE2\x88\x9A" "3");
        CHECK_EQ_STR(rootsText(solvePolynomial(c.v({"1", "0", "-4", "0", "4"})), rect()), "\xE2\x88\x9A" "2 x2 | -\xE2\x88\x9A" "2 x2");
        CHECK_EQ_STR(rootsText(solvePolynomial(c.v({"1", "4", "1", "-10", "-4", "8", "0"})), rect()), "1 x2 | 0 | -2 x3");
        CHECK_EQ_STR(rootsText(solvePolynomial(c.v({"1", "-21", "175", "-735", "1624", "-1764", "720"})), rect()), "6 | 5 | 4 | 3 | 2 | 1");
        roots = solvePolynomial(c.v({"1", "0", "0", "0", "0", "0", "-1"}));
        CHECK(roots.size() == 6 && roots[0].value.value == Complex(1, 0) && roots[1].value.value == Complex(-1, 0));
        CHECK_EQ_STR(numberText(roots[2].value, rect()), "(1+\xE2\x88\x9A" "3i)/2");
        // Frazioni e decimali nei coefficienti
        CHECK_EQ_STR(rootsText(solvePolynomial(c.v({"1/2", "0", "1/3"})), rect()), "\xE2\x88\x9A" "6i/3 | -\xE2\x88\x9A" "6i/3");
        CHECK_EQ_STR(rootsText(solvePolynomial(c.v({"0.5", "0", "-0.5"})), rect()), "1 | -1");
        CHECK(solvePolynomial(c.v({"0.5", "0", "-1"}))[0].value.decimal);
        // Irriducibile di grado 3: decimali
        CHECK_EQ_STR(rootsText(visibleRoots(solvePolynomial(c.v({"1", "0", "0", "-2"})), ComplexMode::Real), real()), "1.25992105");
        // Coefficienti non razionali: radice doppia √2 riconosciuta numericamente
        roots = solvePolynomial(c.v({"1", "-2sqrt(2)", "2"}));
        CHECK(roots.size() == 1 && roots[0].multiplicity == 2 && std::abs(roots[0].value.value.real() - std::sqrt(2.0)) < 1e-7);
        // Radici vicine ma distinte restano due
        roots = solvePolynomial(c.v({"1", "-2.001", "1.001"}));
        CHECK(roots.size() == 2 && roots[0].multiplicity == 1);
        // Coefficienti complessi: (x−i)² -> i ×2
        roots = solvePolynomial(c.v({"1", "-2i", "-1"}));
        CHECK(roots.size() == 1 && roots[0].multiplicity == 2 && std::abs(roots[0].value.value - Complex(0, 1)) < 1e-6);
        // Coefficienti grandi: radici razionali con costante 10^9
        CHECK_EQ_STR(rootsText(solvePolynomial(c.v({"1", "-1000000001", "1000000000"})), rect()), "1000000000 | 1");
        // Errori: a = 0, Underflow (tabella α-2), Ma ERROR fuori intervallo
        CHECK_THROWS_AS(solvePolynomial(c.v({"0", "1", "2"})), MathError);
        bool underflow = false;
        try { (void)solvePolynomial(c.v({"1E99", "1E99", "1E-99"})); } catch (const MathError& ex) { underflow = std::strncmp(ex.what(), "Underflow", 9) == 0; }
        CHECK(underflow);
        CHECK_THROWS_AS(solvePolynomial(c.v({"1E-50", "-1E60"})), MathError);
    }

    // ================================================================ 4.5 SOLVER
    {
        Calc c(ComplexMode::Real);
        CHECK((solverVariables("H=VT-((1)/(2))GT^(2)") == std::vector<char>{'H', 'V', 'T', 'G'}));
        CHECK((solverVariables("x^2+X=e^i") == std::vector<char>{'X'}));     // x e X sono la stessa variabile; e, i costanti
        c.mem.store('H', 14); c.mem.store('T', 2); c.mem.store('G', 9.8); c.mem.store('V', 0);
        const SolverOutcome o = solveFor("H=VT-((1)/(2))GT^(2)", 'V', -9e99, 9e99, c.ec);
        CHECK_NEAR(o.value, 16.8, 1e-12); CHECK_NEAR(o.left, 14, 1e-12); CHECK_NEAR(o.right, 14, 1e-12); CHECK(!o.retry);
        c.mem.store('V', 16.8);
        CHECK_NEAR(solveFor("H=VT-((1)/(2))GT^(2)", 'T', 0.1, 9e99, c.ec).value, 2, 1e-12);   // un'altra variabile
        c.mem.store('X', 1);
        CHECK_NEAR(solveFor("x^2-2", 'X', -9e99, 9e99, c.ec).value, std::sqrt(2.0), 1e-14);   // senza "=": = 0
        CHECK_NEAR(solveFor("cos(x)=x", 'X', -9e99, 9e99, c.ec).value, 0.7390851332151607, 1e-14);
        CHECK_NEAR(solveFor("x^2=4", 'X', -10, 0, c.ec).value, -2, 1e-14);                    // Lower/Upper
        auto message = [&](const char* eqn, char v, double lo, double hi) {
            try { (void)solveFor(eqn, v, lo, hi, c.ec); } catch (const std::exception& ex) { return std::string(ex.what()); }
            return std::string("ok");
        };
        CHECK(message("x^2=4", 'X', 3, 10).rfind("Can't Solve!", 0) == 0);
        CHECK(message("x^2=-1", 'X', -9e99, 9e99).rfind("Time Out", 0) == 0);
        CHECK(message("3=4", 'X', -9e99, 9e99).rfind("No Variable", 0) == 0);
        CHECK(message("x=1=2", 'X', -9e99, 9e99).rfind("Syntax ERROR", 0) == 0);
        CHECK(message("1/x=0", 'X', -9e99, 9e99).rfind("Time Out", 0) == 0);               // polo: nessuna soluzione
        c.mem.store('X', 0.5);
        CHECK(solveFor("1E-8(2int(X)-1)=0", 'X', -9e99, 9e99, c.ec).retry);                // convergenza non sufficiente
    }

    // ================================================================ 4.1 / 4.5 / 4.7 schermate e tasti (firmware)
    {
        Rig r;
        Workspace& ws = r.ws();
        app::Context& ctx = r.fw.apps().context();
        r.type("a");
        auto* eq = r.active<EquationApp>();
        CHECK(eq && eq->screen() == EquationApp::Screen::Menu);
        r.key(pckey::Down); r.key(pckey::Enter);                                // EXE sulla voce: Polynomial
        CHECK(eq->screen() == EquationApp::Screen::ChooseSize && eq->mode() == EquationApp::Mode::Polynomial);
        r.key(pckey::Esc); CHECK(eq->screen() == EquationApp::Screen::Menu);
        r.key(pckey::F1); r.key(pckey::F2);                                   // SIMUL, 3 incognite
        CHECK(eq->screen() == EquationApp::Screen::Coefficients && ws.equation.unknowns == 3);
        for (const char* v : {"4", "1", "-2", "-1", "1", "6", "3", "1", "-5", "4", "1", "-7"}) r.enter(v);
        CHECK(ws.equation.sim(2, 3).value == Complex(-7, 0));
        // EXIT durante l'immissione: il coefficiente resta (manuale 4-1)
        r.key(pckey::Up); r.type("99"); r.key(pckey::Esc);
        CHECK(!eq->editing() && ws.equation.sim(1, 3).value == Complex(1, 0));
        // F4 EDIT con il valore esatto; F2 DELETE; EXE senza input passa alla cella successiva
        r.key(pckey::F4); CHECK(eq->editing()); r.key(pckey::Esc);
        r.key(pckey::F1);                                                       // SOLVE
        CHECK(eq->screen() == EquationApp::Screen::Solutions && eq->solutionRows() == 3);
        CHECK_EQ_STR(eq->solutionText(0, ctx), "X=1");
        CHECK_EQ_STR(eq->solutionText(1, ctx), "Y=-1");
        CHECK_EQ_STR(eq->solutionText(2, ctx), "Z=2");
        CHECK((ws.equation.simResult.rows == 3 && ws.equation.simResult.cols == 1));
        r.key(pckey::Down); CHECK_EQ_STR(eq->selectedText(ctx), "-1");
        r.key(pckey::F1);                                                       // REPEAT: coefficienti conservati
        CHECK(eq->screen() == EquationApp::Screen::Coefficients && ws.equation.sim(0, 0).value == Complex(4, 0));
        // Frazione digitata con il modello (F1) e radice (F2) in Math
        r.key(pckey::F3);                                                       // CLEAR: tutto a zero, cursore su a1
        CHECK(ws.equation.sim(2, 3).isZero() && eq->row() == 0 && eq->col() == 0 && ws.equation.simResult.empty());
        r.key(pckey::Esc); r.key(pckey::F1);                                  // 2 incognite
        r.type("1"); r.key(pckey::Enter);
        r.type("2"); r.key(pckey::Enter);
        r.type("1"); r.key(pckey::F1); r.type("3"); r.key(pckey::Enter);     // cella c1: 1, F1 frazione, 3 -> 1/3
        CHECK(ws.equation.sim(0, 2).hasExact && std::abs(ws.equation.sim(0, 2).value.real() - 1.0 / 3.0) < 1e-15);
        r.type("3"); r.key(pckey::Enter); r.type("-1"); r.key(pckey::Enter); r.type("2"); r.key(pckey::Enter);
        r.key(pckey::F1);
        CHECK_EQ_STR(eq->solutionText(0, ctx), "X=13/21");
        r.key(pckey::F5);                                                       // S⇔D
        CHECK_EQ_STR(eq->selectedText(ctx), "0.619047619");
        r.key(pckey::F5);
        CHECK_EQ_STR(eq->selectedText(ctx), "13/21");
        r.key(pckey::Esc);
        // Errore di sintassi in una cella: Syntax ERROR, l'immissione resta aperta
        r.type("2+"); r.key(pckey::Enter);
        CHECK(r.error() && r.errorTitle() == "Syntax ERROR");
        r.closeError();
        CHECK(eq->editing());
        r.key(pckey::Esc);
        // Line: le soluzioni sono decimali
        ws.settings.inputMode = Settings::InputMode::Line;
        r.key(pckey::F1);
        CHECK_EQ_STR(eq->solutionText(0, ctx), "X=0.619047619");
        ws.settings.inputMode = Settings::InputMode::Math;
        r.key(pckey::Esc); r.key(pckey::Esc); r.key(pckey::Esc);
        CHECK(eq->screen() == EquationApp::Screen::Menu);
        // POLY: ×k, Complex Mode, No Solution, risultati lunghi scorrevoli
        r.key(pckey::F2); r.key(pckey::F2);
        for (const char* v : {"1", "3", "3", "1"}) r.enter(v);
        r.key(pckey::F1);
        CHECK_EQ_STR(eq->solutionText(0, ctx), "X1=-1 \xC3\x97" "3");
        r.key(pckey::Esc); r.key(pckey::F3);
        for (const char* v : {"1", "2", "3", "2"}) r.enter(v);
        r.key(pckey::F1);
        CHECK(eq->solutionRows() == 1);                                        // Real: solo X1 = −1
        // SET UP dalla modalita': EXIT torna a EQUATION con la nuova impostazione
        r.key(pckey::F10);
        CHECK(r.fw.apps().activeId() == app::AppId::System);
        r.key(pckey::Down); r.key(pckey::Down); r.key(pckey::Down); r.key(pckey::Right); r.key(pckey::Esc);
        CHECK(r.fw.apps().activeId() == app::AppId::Equation && ws.settings.complex == ComplexMode::Rectangular);
        eq = r.active<EquationApp>();
        CHECK(eq->screen() == EquationApp::Screen::Solutions && eq->solutionRows() == 3);
        CHECK_EQ_STR(eq->solutionText(1, ctx), "X2=(-1+\xE2\x88\x9A" "7i)/2");
        r.key(pckey::Down);
        r.key(pckey::Right);                                                    // scorrimento del valore in basso (qui entra)
        CHECK(!r.error());
        r.key(pckey::Esc); r.key(pckey::Esc); r.key(pckey::F1);              // grado 2: x^2+1
        r.key(pckey::F3);
        for (const char* v : {"1", "0", "1"}) r.enter(v);
        ws.settings.complex = ComplexMode::Real;
        r.key(pckey::F1);
        CHECK_EQ_STR(eq->message(), "No Solution");
        CHECK(ws.equation.polyResult.empty());
        r.key(pckey::Esc); r.key(pckey::F3);
        r.enter("0"); r.enter("1"); r.enter("1");
        r.key(pckey::F1);
        CHECK(r.error() && r.errorTitle() == "Ma ERROR");                     // a = 0
        r.closeError();
        r.key(pckey::Esc); r.key(pckey::Esc);
        // SOLVER (manuale 4-5): H=VT−½GT², H=14, V=0, T=2, G=9.8 -> V=16.8, Lft=Rgt=14
        r.key(pckey::F3);
        CHECK(eq->screen() == EquationApp::Screen::SolverTable && eq->editing());
        r.type("H=VT-"); r.key(pckey::F1); r.type("1"); r.key(pckey::Down); r.type("2"); r.key(pckey::Right);
        r.type("GT"); shifted(r, pckey::F2);                                  // SHIFT F2: x²
        r.key(pckey::Enter);
        CHECK((eq->solverVars() == std::vector<char>{'H', 'V', 'T', 'G'}));
        r.enter("14"); r.enter("0"); r.enter("2"); r.enter("9.8");
        CHECK(ws.memory.recall('G') == 9.8 && ws.equation.solverLower == -9e99 && ws.equation.solverUpper == 9e99);
        r.key(pckey::Up); r.key(pckey::Up); r.key(pckey::Up);                // riga V
        r.key(pckey::F6);                                                       // SOLVE
        CHECK(!r.error() && eq->screen() == EquationApp::Screen::SolverResult);
        CHECK_NEAR(eq->solverOutcome().value, 16.8, 1e-12);
        CHECK_NEAR(eq->solverOutcome().left, 14, 1e-12);
        CHECK_NEAR(eq->solverOutcome().right, 14, 1e-12);
        CHECK_NEAR(ws.memory.recall('V'), 16.8, 1e-12);                         // la variabile risolta resta memorizzata
        CHECK(ws.equation.solverVariable == 'V');
        r.key(pckey::F1);                                                       // REPEAT
        CHECK(eq->screen() == EquationApp::Screen::SolverTable);
        r.key(pckey::F1);                                                       // RECALL: V torna 0
        CHECK(ws.memory.recall('V') == 0);
        // Lower / Upper: Can't Solve! fuori dall'intervallo
        r.key(pckey::Down); r.key(pckey::Down); r.key(pckey::Down);         // dalla riga V alla riga Lower
        r.enter("20"); r.enter("30");
        CHECK(ws.equation.solverLower == 20 && ws.equation.solverUpper == 30);
        r.key(pckey::Up); r.key(pckey::Up); r.key(pckey::Up); r.key(pckey::Up);   // riga V
        r.key(pckey::F6);
        CHECK(r.error() && r.errorTitle() == "Can't Solve!");
        r.closeError();
        // F2 DELETE sulla riga Eq: equazione cancellata, si riscrive
        r.key(pckey::Up); r.key(pckey::Up);                                    // riga Eq
        r.key(pckey::F2);
        CHECK(ws.equation.solverExpr.empty() && eq->editing());
        r.enter("3=4");
        r.key(pckey::F6);
        CHECK(r.error() && r.errorTitle() == "No Variable");
        r.closeError();
        r.home();
    }

    // ================================================================ 4.6 uso dei risultati (VARS › EQUATION, MatAns)
    {
        Workspace ws;
        Calc c;
        ws.equation.unknowns = 2;
        CHECK_THROWS_AS(runCalculation(ws, "SimRes"), MathError);               // nessuna soluzione calcolata
        CHECK_THROWS_AS(runCalculation(ws, "SimCoef"), MathError);              // nessun coefficiente
        const auto coeffs = c.v({"1", "1", "1/3", "2", "1", "1/2"});
        for (int k = 0; k < 6; ++k) ws.equation.sim(static_cast<std::size_t>(k / 3), static_cast<std::size_t>(k % 3)) = coeffs[static_cast<std::size_t>(k)];
        const SimulResult r = solveSimultaneous(ws.equation.simCoefficients(), 2);
        ws.equation.simResult = toMatrix(r.solution, 2, 1);
        (void)runCalculation(ws, "SimRes");
        CHECK_EQ_STR(resultText(ws.history.back(), ws.settings), "[[1/6][1/6]]");
        CHECK(ws.matrices.matAns.rows == 2 && ws.matrices.matAns.cols == 1);  // in MatAns (manuale 1-33)
        (void)runCalculation(ws, "SimCoef");
        CHECK_EQ_STR(resultText(ws.history.back(), ws.settings), "[[1,1,1/3][2,1,1/2]]");
        (void)runCalculation(ws, "SimRes*6");
        CHECK_EQ_STR(resultText(ws.history.back(), ws.settings), "[[1][1]]");
        CHECK_THROWS_AS(runCalculation(ws, "SimRes+SimCoef"), MathError);    // non insieme (nota *4)
        // Senza soluzione unica SimRes e' la Rref
        const SimulResult none = solveSimultaneous(c.v({"1", "1", "1", "1", "1", "2"}), 2);
        ws.equation.simResult = toMatrix(none.rref, 2, 3);
        (void)runCalculation(ws, "SimRes");
        CHECK_EQ_STR(resultText(ws.history.back(), ws.settings), "[[1,1,0][0,0,1]]");
        // PlyRes / PlyCoef
        ws.equation.degree = 2;
        ws.equation.polynomial[0] = c.n("1"); ws.equation.polynomial[1] = c.n("-3"); ws.equation.polynomial[2] = c.n("2");
        CHECK_THROWS_AS(runCalculation(ws, "PlyRes"), MathError);
        std::vector<Number> values;
        for (const Root& x : solvePolynomial(ws.equation.polyCoefficients())) values.push_back(x.value);
        ws.equation.polyResult = toMatrix(values, values.size(), 1);
        (void)runCalculation(ws, "PlyRes");
        CHECK_EQ_STR(resultText(ws.history.back(), ws.settings), "[[2][1]]");
        (void)runCalculation(ws, "PlyCoef");
        CHECK_EQ_STR(resultText(ws.history.back(), ws.settings), "[[1,-3,2]]");
        (void)runCalculation(ws, "Trn(PlyCoef)");                             // un parser: PlyCoef e' una matrice come le altre
        CHECK_EQ_STR(resultText(ws.history.back(), ws.settings), "[[1][-3][2]]");
        // Il nome si importa come un token nell'editor
        NaturalEditor ed; ed.setExpression("SimRes+1");
        CHECK(ed.root().children.size() == 3 && ed.root().children[0]->text == "SimRes");
        CHECK(equationMatrixIndex("PlyCoef") == CollectionResolver::PlyCoef && equationMatrixIndex("plycoef") < 0);
        // Il Solver memorizza la variabile come la fx-CG50 (verificato sopra con V)
    }

    // ================================================================ record v8 e migrazione dalla v1.11.0 (v7)
    {
        Calc c;
        Workspace ws;
        ws.equation.unknowns = 4;
        ws.equation.sim(0, 0) = c.n("1/3"); ws.equation.sim(3, 4) = c.n("2+sqrt(3)i"); ws.equation.sim(2, 1) = c.n("sin(1)");
        ws.equation.polynomial[0] = c.n("0.25"); ws.equation.degree = 5;
        ws.equation.solverExpr = "H=VT"; ws.equation.solverLower = -5; ws.equation.solverUpper = 7; ws.equation.solverVariable = 'V';
        ws.equation.inequality[1] = c.n("-2"); ws.equation.ineqDegree = 3; ws.equation.ineqRelation = 3;
        ws.equation.simResult = toMatrix(c.v({"1/2", "3"}), 2, 1);
        ws.equation.polyResult = toMatrix(c.v({"1+i"}), 1, 1);
        const auto bytes = serializeWorkspace(ws);
        CHECK(bytes[4] == workspaceRecordVersion() && bytes[5] == 0);
        Workspace back;
        CHECK(deserializeWorkspace(bytes.data(), bytes.size(), back));
        const EquationState& e = back.equation;
        CHECK(e.unknowns == 4 && e.degree == 5 && e.ineqDegree == 3 && e.ineqRelation == 3);
        CHECK(e.sim(0, 0) == ws.equation.sim(0, 0) && e.sim(3, 4) == ws.equation.sim(3, 4) && e.sim(2, 1) == ws.equation.sim(2, 1));
        CHECK(e.polynomial[0].decimal && e.inequality[1] == ws.equation.inequality[1]);
        CHECK(e.solverExpr == "H=VT" && e.solverLower == -5 && e.solverUpper == 7 && e.solverVariable == 'V');
        CHECK(e.simResult == ws.equation.simResult && e.simResult.hasExact() && e.polyResult == ws.equation.polyResult);
        // Record troncati o alterati: rifiutati senza toccare la destinazione
        Workspace target; target.equation.unknowns = 5;
        for (std::size_t cut = bytes.size() - 1500; cut < bytes.size(); cut += 37) CHECK(!deserializeWorkspace(bytes.data(), cut, target));
        CHECK(target.equation.unknowns == 5);
        auto bad = bytes;
        bad.back() = 2;                                                        // indicatore delle forme esatte non valido
        CHECK(!deserializeWorkspace(bad.data(), bad.size(), target) && target.equation.unknowns == 5);

        // Migrazione dal record v7 della v1.11.0 (fixture generata con i sorgenti del tag v1.11.0)
        Workspace old;
        CHECK(kRecordV7[4] == 7);
        CHECK(deserializeWorkspace(kRecordV7, sizeof(kRecordV7), old));
        const EquationState& o = old.equation;
        CHECK(o.unknowns == 3 && o.degree == 4 && o.ineqDegree == 2 && o.ineqRelation == 2);
        // Il sistema 3x4 del manuale torna alla sua riga e colonna
        const double expect[] = {4, 1, -2, -1, 1, 6, 3, 1, -5, 4, 1, -7};
        bool same = true;
        for (int k = 0; k < 12; ++k) same = same && o.sim(static_cast<std::size_t>(k / 4), static_cast<std::size_t>(k % 4)).value == Complex(expect[k], 0);
        CHECK(same);
        // Le celle oltre il sistema attivo restano (0.1 e 1/3 come double, senza perdita)
        bool tenth = false, third = false;
        for (const Number& n : o.simultaneous) { tenth = tenth || n.value.real() == 0.1; third = third || n.value.real() == 1.0 / 3.0; }
        CHECK(tenth && third);
        CHECK(o.polynomial[0].value == Complex(1, 0) && !o.polynomial[0].decimal && o.polynomial[2].value == Complex(-5, 0));
        CHECK(o.polynomial[5].value.real() == 0.25 && o.polynomial[5].decimal && o.polynomial[6].value.real() == -1.5);
        CHECK(o.inequality[1].value.real() == 0.5 && o.inequality[2].value.real() == -3);
        CHECK(o.solverExpr == "x^3-2x=1.5" && o.solverLower == -9e99 && o.solverUpper == 9e99);
        CHECK(old.memory.recall('X') == 2);                                    // il valore iniziale diventa X
        CHECK(old.memory.recall('B') == 2.5 && old.matrices.mat[0].rows == 2 && old.settings.complex == ComplexMode::Rectangular);
        // I coefficienti migrati si risolvono come prima: il sistema del manuale da' 1, −1, 2
        const SimulResult m = solveSimultaneous(o.simCoefficients(), 3);
        CHECK(m.kind == SimulResult::Kind::Unique && m.solution[0].value == Complex(1, 0) && m.solution[2].value == Complex(2, 0));
        // Il record migrato si salva nella versione corrente (v9) e si rilegge identico
        const auto again = serializeWorkspace(old);
        Workspace round;
        CHECK(again[4] == workspaceRecordVersion() && deserializeWorkspace(again.data(), again.size(), round) && round.equation.sim(2, 3) == o.sim(2, 3));
    }

    // ================================================================ editor (correzioni della v1.12.0 usate dal Solver)
    {
        CalculatorMemory m; m.store('X', 4); m.store('G', 3); m.store('T', 2);
        EvalContext ec; ec.memory = &m;
        NaturalEditor ed;
        ed.insertFraction(); ed.insert('1'); (void)ed.moveDown(); ed.insert('2'); (void)ed.moveRight(); ed.insert('X');
        CHECK_EQ_STR(ed.toExpression(), "((1)/(2))X");                       // ½X = X/2, non 1/(2X)
        CHECK_NEAR(evaluateReal(*parseExpression(ed.toExpression()), ec), 2, 0);
        NaturalEditor back; back.setExpression(ed.toExpression());
        CHECK(back.root().children.size() == 2 && back.root().children[0]->kind == ast::Kind::Fraction);   // si reimporta come frazione
        CHECK_EQ_STR(back.toExpression(), "((1)/(2))X");
        NaturalEditor sq; for (char ch : std::string("GT")) sq.insert(ch); sq.insertSquare();
        CHECK_EQ_STR(sq.toExpression(), "G(T)^(2)");                           // GT² = G·T²
        NaturalEditor num; for (char ch : std::string("12")) num.insert(ch); num.insertSquare();
        CHECK_EQ_STR(num.toExpression(), "(12)^(2)");
        NaturalEditor plain; plain.insertFraction(); plain.insert('1'); (void)plain.moveDown(); plain.insert('3'); (void)plain.moveRight(); plain.insert('+');
        CHECK_EQ_STR(plain.toExpression(), "(1)/(3)+");                        // seguita da un operatore: invariata
    }

    // ================================================================ 4.8 estensioni
    {
        const auto set = polynomialInequality({1, -3, 2}, InequalityRelation::Greater);
        CHECK(set.size() == 2 && formatSolutionSet(set) == "x<1 , 2<x");
        const auto sys = solveInequalitySystem({{1, 0, -4, 0}, {0, 1, 0, 2}});   // x²−4<0 e x>0
        CHECK(sys.size() == 1 && std::abs(sys[0].lower) < 1e-12 && std::abs(sys[0].upper - 2) < 1e-12);
    }
    return testutil::finish("equation_v112_test");
}
