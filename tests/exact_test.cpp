// Exact arithmetic (fractions, surds, multiples of pi) and the exact
// evaluator, against values that can be checked by hand.
#include "exact.hpp"
#include "expression.hpp"
#include "math_engine.hpp"
#include "test_util.hpp"
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>

using namespace fx;
using exact::Rational;
using exact::Value;

namespace {
Rational q(std::int64_t n, std::int64_t d) { Rational r; (void)Rational::make(n, d, r); return r; }
Value v(std::int64_t n, std::int64_t d = 1) { return Value::fromRational(q(n, d)); }
Value surd(std::int64_t n, std::int64_t d, std::int64_t rad) { const exact::Term t{q(n, d), rad}; Value out; (void)Value::fromTerms(&t, 1, out); return out; }
std::string text(const Value& x, bool mixed = false) { return exact::toText(x, mixed); }
// Exact evaluation of linear text in the given angle mode.
bool ex(const char* s, Value& out, bool* decimal = nullptr) {
    EvalContext c;
    return evaluateExact(*parseExpression(s), c, out, decimal);
}
std::string exText(const char* s) { Value x; return ex(s, x) ? text(x) : std::string("<none>"); }
} // namespace

int main() {
    // ---------------------------------------------------------------- rationals
    {
        Rational r;
        CHECK(Rational::make(6, -4, r) && r.num == -3 && r.den == 2);
        CHECK(Rational::make(0, 5, r) && r.num == 0 && r.den == 1);
        CHECK(!Rational::make(1, 0, r));
        CHECK(!Rational::make(std::numeric_limits<std::int64_t>::min(), 1, r));
        CHECK(exact::add(q(1, 2), q(1, 3), r) && r == q(5, 6));
        CHECK(exact::sub(q(1, 2), q(1, 3), r) && r == q(1, 6));
        CHECK(exact::mul(q(2, 3), q(9, 4), r) && r == q(3, 2));
        CHECK(exact::div(q(2, 3), q(4, 9), r) && r == q(3, 2));
        CHECK(!exact::div(q(1, 2), q(0, 1), r));
        // Overflow is reported, never wrapped.
        const std::int64_t big = (static_cast<std::int64_t>(1) << 61) - 1;
        CHECK(!exact::mul(q(big, 1), q(big, 1), r));
        CHECK(!exact::add(q(1, big), q(1, big - 2), r));
        // Exact comparison even when cross products would overflow.
        CHECK(exact::compare(q(1, 3), q(1, 2)) < 0);
        CHECK(exact::compare(q(-1, 3), q(-1, 2)) > 0);
        CHECK(exact::compare(q(big, big - 1), q(big - 1, big - 2)) < 0);   // 1+1/(b-1) < 1+1/(b-2)
        CHECK(exact::compare(q(7, 7), q(1, 1)) == 0);
    }
    // ---------------------------------------------------------------- surds and pi
    {
        Value r;
        CHECK(exact::sqrt(v(12), r) && text(r) == "2\xE2\x88\x9A" "3");          // √12 = 2√3
        CHECK(exact::sqrt(v(1, 2), r) && text(r) == "\xE2\x88\x9A" "2/2");       // √(1/2) = √2/2
        CHECK(exact::sqrt(v(9, 4), r) && text(r) == "3/2");
        CHECK(!exact::sqrt(v(-4), r));
        CHECK(exact::sqrt(v(0), r) && r.isZero());
        CHECK(!exact::sqrt(v(1000003), r));                                         // prime > MaxRadicand: no form
        Value s2 = surd(1, 1, 2), s8;
        CHECK(exact::sqrt(v(8), s8) && exact::add(s2, s8, r) && text(r) == "3\xE2\x88\x9A" "2");   // √2+√8 = 3√2
        CHECK(exact::mul(s2, s2, r) && r == v(2));                                          // √2·√2 = 2
        CHECK(exact::mul(surd(1, 1, 6), surd(1, 1, 10), r) && text(r) == "2\xE2\x88\x9A" "15"); // √6·√10 = 2√15
        // (1+√2)(1-√2) = -1 ; 1/(1+√2) = √2-1 (rationalised denominator)
        Value onePlus, oneMinus;
        CHECK(exact::add(v(1), s2, onePlus) && exact::sub(v(1), s2, oneMinus));
        CHECK(exact::mul(onePlus, oneMinus, r) && r == v(-1));
        CHECK(exact::div(v(1), onePlus, r) && text(r) == "-1+\xE2\x88\x9A" "2");
        // Three distinct terms have no representation here (fx-CG50 shows a decimal).
        Value s3 = surd(1, 1, 3), s6 = surd(1, 1, 6), t;
        CHECK(exact::add(s2, s3, t) && !exact::add(t, s6, r));
        // Sign of a two-term surd from the exact squares: 3 - 2√2 > 0, 2 - 3√2 < 0.
        Value a, b;
        CHECK(exact::sub(v(3), surd(2, 1, 2), a) && a.sign() > 0);
        CHECK(exact::sub(v(2), surd(3, 1, 2), b) && b.sign() < 0);
        // π forms
        const Value pi = Value::piTimes(q(1, 1));
        CHECK(exact::mul(pi, v(2, 3), r) && text(r) == "2\xCF\x80/3");
        CHECK(exact::div(pi, pi, r) && r == v(1));
        CHECK(!exact::mul(pi, pi, r));                    // π² not represented
        CHECK(!exact::add(pi, v(1), r));                  // π+1 not represented
        CHECK(exact::powInt(s2, 4, r) && r == v(4));
        CHECK(exact::powInt(v(2, 3), -2, r) && r == v(9, 4));
        CHECK(!exact::powInt(v(0), -1, r));
        CHECK(exact::nthRoot(v(-27, 8), 3, r) && r == v(-3, 2));
        CHECK(!exact::nthRoot(v(2), 3, r));
        CHECK(!exact::nthRoot(v(-16), 4, r));
        CHECK(exact::powRational(v(8), q(2, 3), r) && r == v(4));
        CHECK(exact::powRational(v(4, 9), q(-1, 2), r) && r == v(3, 2));
        CHECK(std::abs(surd(3, 2, 5).toDouble() - 1.5 * std::sqrt(5.0)) < 1e-15);
    }
    // ---------------------------------------------------------------- literals and expansions
    {
        Rational r;
        CHECK(exact::parseDecimal("0.25", r) && r == q(1, 4));
        CHECK(exact::parseDecimal("12.375", r) && r == q(99, 8));
        CHECK(exact::parseDecimal("1.5e3", r) && r == q(1500, 1));
        CHECK(exact::parseDecimal("25E-2", r) && r == q(1, 4));
        CHECK(exact::parseDecimal("0._3", r) && r == q(1, 3));
        CHECK(exact::parseDecimal("0.1_6", r) && r == q(1, 6));
        CHECK(exact::parseDecimal("0._142857", r) && r == q(1, 7));
        CHECK(exact::parseDecimal("2.4_9", r) && r == q(5, 2));                   // 2.4999... = 2.5
        CHECK(exact::parseDecimal(".5", r) && r == q(1, 2));
        CHECK(!exact::parseDecimal("1.2.3", r));
        CHECK(!exact::parseDecimal("12345678901234567890", r));                   // > 18 digits: no exact form
        CHECK(exact::isPlainDecimal("0.5") && !exact::isPlainDecimal("0.1_6") && !exact::isPlainDecimal("12"));
        auto e = exact::decimalExpansion(q(1, 6));
        CHECK(e.integer == "0" && e.fixed == "1" && e.period == "6" && !e.truncated);
        e = exact::decimalExpansion(q(-22, 7));
        CHECK(e.negative && e.integer == "3" && e.fixed.empty() && e.period == "142857");
        e = exact::decimalExpansion(q(3, 8));
        CHECK(e.fixed == "375" && e.period.empty());
        e = exact::decimalExpansion(q(1, 97));                                     // period of 96 digits
        CHECK(e.truncated && e.period.empty());
    }
    // ---------------------------------------------------------------- display limits (fx-CG50 rules)
    {
        CHECK(exact::fitsNatural(v(1, 3), false));
        CHECK(exact::fractionChars(q(123456, 1), false) == 6);
        CHECK(exact::fractionChars(q(1 * 123456 + 1, 123456), true) == 10);        // 1{1{123456 = 10 characters
        CHECK(exact::fitsNatural(v(123457, 123456), true));
        CHECK(!exact::fitsNatural(v(3575, 5893308), false));                        // 1/2578+1/4572: decimal
        CHECK(exact::fitsRelaxed(v(3575, 5893308)));
        CHECK(exact::fitsNatural(surd(3, 1, 2), false));                            // 3√2
        CHECK(!exact::fitsNatural(surd(105, 1, 2), false));                         // 105√2: decimal
        CHECK(!exact::fitsNatural(surd(1, 1, 1003), false));                        // radicand >= 1000
        CHECK(exact::fitsNatural(Value::piTimes(q(156, 1)), false));
        CHECK(!exact::fitsNatural(Value::piTimes(q(1111104, 1)), false));           // |n| >= 10^6
        CHECK(exact::fitsNatural(Value::piTimes(q(7455, 103)), false));             // 105 71/103 π
        CHECK(!exact::fitsNatural(Value::piTimes(q(258, 1619)), false));            // 4-digit denominator
        CHECK(text(v(7, 3), true) == "2 1/3" && text(v(-7, 3), true) == "-2 1/3" && text(v(2, 3), true) == "2/3");
        CHECK(text(Value::piTimes(q(-1, 2))) == "-\xCF\x80/2");
    }
    // ---------------------------------------------------------------- exact evaluator on the shared AST
    {
        setAngleMode(AngleMode::Radians);
        CHECK(exText("1/2+1/3") == "5/6");
        CHECK(exText("(1)/(3)*3") == "1");
        CHECK(exText("6/2(1+2)") == "1");                                  // Casio priority, exact
        CHECK(exText("2^-3") == "1/8");
        CHECK(exText("(-2)^3") == "-8");
        CHECK(exText("-2^2") == "-4");
        CHECK(exText("sqrt(2)+sqrt(8)") == "3\xE2\x88\x9A" "2");
        CHECK(exText("2(3-2sqrt(5))") == "6-4\xE2\x88\x9A" "5");
        CHECK(exText("sqrt(2)+sqrt(3)+sqrt(6)") == "<none>");
        CHECK(exText("(sqrt(3))/(11)+(sqrt(2))/(10)") == "(11\xE2\x88\x9A" "2+10\xE2\x88\x9A" "3)/110");
        CHECK(exText("78pi*2") == "156\xCF\x80");
        CHECK(exText("cos(pi/3)") == "1/2");
        CHECK(exText("sin(pi/4)") == "\xE2\x88\x9A" "2/2");
        CHECK(exText("tan(pi/3)") == "\xE2\x88\x9A" "3");
        CHECK(exText("asin(1/2)") == "\xCF\x80/6");
        CHECK(exText("acos(-1)") == "\xCF\x80");
        CHECK(exText("atan(1)") == "\xCF\x80/4");
        CHECK(exText("sin(1)") == "<none>");                               // transcendental
        CHECK(exText("90\xC2\xB0") == "\xCF\x80/2");                       // degree postfix in Rad mode
        setAngleMode(AngleMode::Degrees);
        CHECK(exText("sin(30)") == "1/2");
        CHECK(exText("cos(150)") == "-\xE2\x88\x9A" "3/2");
        CHECK(exText("tan(45)") == "1");
        CHECK(exText("asin(sqrt(3)/2)") == "60");
        CHECK(exText("sin(15)") == "<none>");
        setAngleMode(AngleMode::Gradians);
        CHECK(exText("sin(50)") == "\xE2\x88\x9A" "2/2");                  // 50 gon = 45°
        setAngleMode(AngleMode::Radians);
        CHECK(exText("10^16+1-10^16") == "1");                             // exact where double gives 0
        CHECK(exText("(2/3)^(1/2)") == "\xE2\x88\x9A" "6/3");
        CHECK(exText("8^(1/3)") == "2");
        CHECK(exText("(-8)^(1/3)") == "-2");
        CHECK(exText("2^(1/3)") == "<none>");
        CHECK(exText("log(1000)") == "3" && exText("log(2,1/8)") == "-3" && exText("log2(1024)") == "10");
        CHECK(exText("log(2)") == "<none>" && exText("ln(1)") == "0" && exText("ln(2)") == "<none>");
        CHECK(exText("20!") == "2432902008176640000" && exText("21!") == "<none>");
        CHECK(exText("nCr(52,5)") == "2598960" && exText("nPr(10,4)") == "5040");
        CHECK(exText("gcd(28,35)") == "7" && exText("lcm(9,15)") == "45");
        CHECK(exText("abs(-3/4)") == "3/4" && exText("abs(1-sqrt(2))") == "-1+\xE2\x88\x9A" "2");
        CHECK(exText("int(-7/2)") == "-3" && exText("intg(-7/2)") == "-4" && exText("frac(-7/2)") == "-1/2");
        CHECK(exText("intg(sqrt(2))") == "1");
        CHECK(exText("mixed(2,1,3)") == "7/3" && exText("mixed(-2,1,3)") == "-7/3");
        CHECK(exText("107 intdiv 7") == "15" && exText("107 Rmdr 7") == "2" && exText("-7 rmdr 2") == "-1");
        CHECK(exText("3>2") == "1" && exText("1=2") == "0" && exText("sqrt(2)<3/2") == "1");
        CHECK(exText("(1<2)and(2<1)") == "0" && exText("1 or 0") == "1" && exText("3 xor 3") == "0" && exText("not 0") == "1");
        CHECK(exText("sum(x^2,1,10)") == "385" && exText("prod(x/(x+1),1,9)") == "1/10");
        CHECK(exText("integral(x,0,1)") == "<none>");                       // numerical method: no exact claim
        CHECK(exText("rand()") == "<none>" && exText("randint(1,6)") == "<none>");
        CHECK(exText("0.1_6") == "1/6");
        CHECK(exText("dms(30,30,0)") == "61/2");
        Value x; bool decimal = false;
        CHECK(ex("0.1+0.2", x, &decimal) && x == v(3, 10) && decimal);     // exact 3/10, decimal input
        CHECK(ex("1/3+1/6", x, &decimal) && x == v(1, 2) && !decimal);
        CHECK(ex("rndfix(200/7,2)", x, &decimal) && x == v(2857, 100) && decimal);
        // Memory: exact values survive Ans and A-Z; approximate ones do not become exact.
        CalculatorMemory m;
        m.storeExact('A', 1.0 / 3.0, v(1, 3), false);
        m.store('B', 0.1);                                                  // approximate store
        m.setAnswerExact(0.5, v(1, 2), true);
        EvalContext c; c.memory = &m;
        CHECK(evaluateExact(*parseExpression("3A"), c, x) && x == v(1));
        CHECK(!evaluateExact(*parseExpression("B+1"), c, x));
        CHECK(evaluateExact(*parseExpression("Z+1"), c, x) && x == v(1));   // unassigned variables are exactly 0
        CHECK(evaluateExact(*parseExpression("Ans+1/3"), c, x, &decimal) && x == v(5, 6) && decimal);
        // Long chains are walked iteratively (no deep recursion).
        std::string chain = "1";
        for (int i = 0; i < 78; ++i) chain += "+1/2";                    // 313 tokens, within MaxExpressionTokens
        CHECK(evaluateExact(*parseExpression(chain), EvalContext{}, x) && x == v(40));
        // Exact and floating-point evaluations agree on every exact result.
        const char* cases[] = {"1/7+2/9", "sqrt(50)/5", "(1+sqrt(3))^2", "1/(2-sqrt(3))", "3pi/4-pi/2", "cos(pi/6)^2", "(5/3)^3"};
        for (const char* s : cases) {
            Value e;
            CHECK(ex(s, e));
            CHECK_NEAR(e.toDouble(), evaluate(s), 1e-12 * std::max(1.0, std::abs(evaluate(s))));
        }
    }
    // ---------------------------------------------------------------- stack budget
    {
        // With a tiny budget the exact walk gives up quietly (no exception, no crash).
        std::string nested = "2";
        for (int i = 0; i < 30; ++i) nested = "sqrt(" + nested + ")^2";
        const auto tree = parseExpression(nested);
        const std::size_t saved = evalStackBudget();
        setEvalStackBudget(512);
        Value x;
        bool threw = false;
        try { (void)evaluateExact(*tree, EvalContext{}, x); } catch (...) { threw = true; }
        CHECK(!threw);
        setEvalStackBudget(saved);
        CHECK(evaluateExact(*tree, EvalContext{}, x) && x == v(2));
    }
    // ---------------------------------------------------------------- canonical text for reuse
    {
        const Value forms[] = {v(-5, 3), surd(-1, 2, 2), Value::piTimes(q(-3, 4)), Value::piTimes(q(2, 1)), v(42)};
        for (const Value& f : forms) {
            Value back;
            CHECK(ex(exact::toExpression(f).c_str(), back) && back == f);
        }
        Value two, s;
        CHECK(exact::sqrt(v(5), s) && exact::add(v(3), s, two) && exact::div(two, v(7), two));
        Value back;
        CHECK(ex(exact::toExpression(two).c_str(), back) && back == two);
    }
    return testutil::finish("exact_test");
}
