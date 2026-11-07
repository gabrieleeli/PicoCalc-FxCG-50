// Parser/evaluator/number-format tests with known reference values.
#include "expression.hpp"
#include "math_engine.hpp"
#include "number_format.hpp"
#include "test_util.hpp"
#include <cmath>

using namespace fx;
static double ev(const char* s, double x = 0.0) { return evaluate(s, x); }
static Complex evc(const char* s) { EvalContext c; c.complexMode = ComplexMode::Rectangular; return evaluateComplex(*parseExpression(s), c); }

int main() {
    const double pi = std::acos(-1.0);
    setAngleMode(AngleMode::Radians);
    // --- arithmetic and Casio priority
    CHECK_NEAR(ev("1+2*3"), 7, 1e-12);
    CHECK_NEAR(ev("(1+2)*3"), 9, 1e-12);
    CHECK_NEAR(ev("2^3^2"), 512, 1e-9);
    CHECK_NEAR(ev("-2^2"), -4, 1e-12);
    CHECK_NEAR(ev("2^-2"), 0.25, 1e-12);
    CHECK_NEAR(ev("1/2x", 4), 0.125, 1e-12);          // implicit product binds tighter
    CHECK_NEAR(ev("6/2(1+2)"), 1, 1e-12);             // Casio: 6/(2(1+2))
    CHECK_NEAR(ev("2pi"), 2 * pi, 1e-12);
    CHECK_NEAR(ev("2x^2", 3), 18, 1e-12);
    CHECK_NEAR(ev("x2", 3), 6, 1e-12);
    CHECK_NEAR(ev("17%5"), 2, 1e-12);
    CHECK_NEAR(ev("5!"), 120, 1e-12);
    CHECK_NEAR(ev("3!!"), 720, 1e-9);
    CHECK_NEAR(ev("(2+3"), 5, 1e-12);                 // missing ')' at end allowed
    CHECK_NEAR(ev("1.5e3"), 1500, 1e-9);
    CHECK_NEAR(ev("0x1F+0b101+0o17"), 31 + 5 + 15, 1e-12);
    CHECK_NEAR(ev("2e"), 2 * std::exp(1.0), 1e-12);
    // --- prefix functions without parentheses
    CHECK_NEAR(ev("sin 2x", 0.25), std::sin(0.5), 1e-12);
    CHECK_NEAR(ev("sinx", 0.5), std::sin(0.5), 1e-12);
    CHECK_NEAR(ev("sin x cos x", 0.5), std::sin(0.5) * std::cos(0.5), 1e-12);
    CHECK_NEAR(ev("2sqrt(9)"), 6, 1e-12);
    CHECK_NEAR(ev("ln e"), 1, 1e-12);
    // --- functions
    CHECK_NEAR(ev("log(100)"), 2, 1e-12);
    CHECK_NEAR(ev("log(2,8)"), 3, 1e-12);             // fx-CG50: log(base, value)
    CHECK_NEAR(ev("root(5,32)"), 2, 1e-12);
    CHECK_NEAR(ev("root3(27)"), 3, 1e-12);
    CHECK_NEAR(ev("cbrt(-8)"), -2, 1e-12);
    CHECK_NEAR(ev("(-8)^(1/3)"), -2, 1e-12);          // odd-denominator rational exponent
    CHECK_NEAR(ev("nCr(52,5)"), 2598960, 1e-6);
    CHECK_NEAR(ev("nPr(10,3)"), 720, 1e-9);
    CHECK_NEAR(ev("nCr(100,50)"), 1.0089134454556419e29, 1e16);  // no 64-bit overflow
    CHECK_NEAR(ev("gcd(84,36)"), 12, 1e-12);
    CHECK_NEAR(ev("lcm(4,6)"), 12, 1e-12);
    CHECK_NEAR(ev("round(2.345,2)"), 2.35, 1e-12);
    CHECK_NEAR(ev("intg(-2.5)"), -3, 1e-12);
    CHECK_NEAR(ev("int(-2.5)"), -2, 1e-12);
    CHECK_NEAR(ev("abs(-3)+sign(-2)"), 2, 1e-12);
    CHECK_NEAR(ev("asinh(sinh(1.2))"), 1.2, 1e-12);
    CHECK_NEAR(ev("atanh(tanh(0.3))"), 0.3, 1e-12);
    CHECK_NEAR(ev("dms(30,30,0)"), 30.5, 1e-12);
    // --- angle modes and exact trig
    setAngleMode(AngleMode::Degrees);
    CHECK(ev("sin(180)") == 0.0);
    CHECK(ev("cos(90)") == 0.0);
    CHECK_NEAR(ev("sin(30)"), 0.5, 1e-15);
    CHECK_NEAR(ev("asin(0.5)"), 30, 1e-12);
    CHECK_THROWS_AS(ev("tan(90)"), MathError);
    setAngleMode(AngleMode::Gradians);
    CHECK(ev("sin(200)") == 0.0);
    CHECK_NEAR(ev("cos(50)"), std::cos(pi / 4), 1e-12);
    setAngleMode(AngleMode::Radians);
    CHECK_NEAR(ev("sin(pi)"), 0, 1e-15);
    CHECK_NEAR(ev("90°"), pi / 2, 1e-12);              // degree postfix converts to current unit
    // --- errors
    CHECK_THROWS_AS(ev("1/0"), MathError);
    CHECK_THROWS_AS(ev("sqrt(-1)"), NonRealError);
    CHECK_THROWS_AS(ev("ln(-1)"), NonRealError);
    CHECK_THROWS_AS(ev("ln(0)"), MathError);
    CHECK_THROWS_AS(ev("asin(2)"), NonRealError);
    CHECK_THROWS_AS(ev("171!"), MathError);
    CHECK_THROWS_AS(ev("2.5!"), MathError);
    CHECK_THROWS_AS(ev("1+"), SyntaxError);
    CHECK_THROWS_AS(ev(")"), SyntaxError);
    CHECK_THROWS_AS(ev("2 3"), SyntaxError);
    CHECK_THROWS_AS(ev(""), SyntaxError);
    CHECK_THROWS(ev("foo(2)"));                       // f*o*o*(2): undefined variables without memory
    CHECK_THROWS_AS(ev("sin("), SyntaxError);
    CHECK_THROWS_AS(ev("10^400"), MathError);
    CHECK_THROWS_AS(ev("i"), NonRealError);
    // --- calculus (reference values)
    CHECK_NEAR(ev("integral(x^2,0,3)"), 9, 1e-10);
    CHECK_NEAR(ev("integral(sin(x),0,pi)"), 2, 1e-10);
    CHECK_NEAR(ev("integral(exp(-x^2),-10,10)"), std::sqrt(pi), 1e-9);
    CHECK_NEAR(ev("integral(1/x,1,e)"), 1, 1e-10);
    CHECK_NEAR(ev("derivative(x^3,2)"), 12, 1e-8);
    CHECK_NEAR(ev("derivative(sin(x),0)"), 1, 1e-9);
    CHECK_NEAR(ev("derivative2(x^3,2)"), 12, 1e-5);
    CHECK_NEAR(ev("sum(x^2,1,10)"), 385, 1e-9);
    CHECK_NEAR(ev("prod(x,1,6)"), 720, 1e-9);
    CHECK_NEAR(ev("solve(x^2-2,1)"), std::sqrt(2.0), 1e-10);
    CHECK_NEAR(ev("solve(cos(x)-x,0.5)"), 0.7390851332151607, 1e-10);
    CHECK_NEAR(ev("integral(integral(x,0,1),0,2)"), 1.0, 1e-9); // nested: inner rebinding of x
    // --- memory variables
    CalculatorMemory m; m.store('A', 12.5); m.setAnswer(4);
    EvalContext ctx; ctx.memory = &m;
    CHECK_NEAR(evaluateReal(*parseExpression("A+2"), ctx), 14.5, 1e-12);
    CHECK_NEAR(evaluateReal(*parseExpression("2A"), ctx), 25, 1e-12);
    CHECK_NEAR(evaluateReal(*parseExpression("Ans^2"), ctx), 16, 1e-12);
    CHECK_NEAR(evaluateReal(*parseExpression("B"), ctx), 0, 0);          // unset variables are 0
    CHECK_NEAR(evaluateReal(*parseExpression("e"), ctx), std::exp(1.0), 1e-15);
    m.store('E', 3);
    CHECK_NEAR(evaluateReal(*parseExpression("E"), ctx), 3, 0);          // uppercase E is a variable
    CHECK_NEAR(evaluateReal(*parseExpression("ab"), ctx), 0, 0);         // a*b, both 0
    m.store('X', 7);
    CHECK_NEAR(evaluateReal(*parseExpression("x+1"), ctx), 8, 0);        // X from memory when unbound
    // --- compiled expressions
    CompiledExpression f("x^2-4");
    CHECK_NEAR(f(3), 5, 1e-12);
    double out = 0;
    CHECK(!CompiledExpression("1/x").tryEval(0, out));
    CompiledExpression polar("2cos(theta)", "theta");
    CHECK_NEAR(polar(0), 2, 1e-12);
    CHECK_NEAR(integrateAdaptive(f, 0, 2), 8.0 / 3 - 8, 1e-10);
    CHECK_NEAR(differentiate(f, 5), 10, 1e-8);
    // --- complex mode
    Complex z = evc("(1+2i)(3-i)");
    CHECK_NEAR(z.real(), 5, 1e-12); CHECK_NEAR(z.imag(), 5, 1e-12);
    z = evc("i^2"); CHECK(z == Complex(-1, 0));
    z = evc("sqrt(-4)"); CHECK_NEAR(z.imag(), 2, 1e-12);
    z = evc("e^(i pi)"); CHECK_NEAR(z.real(), -1, 1e-12); CHECK(z.imag() == 0);
    z = evc("ln(-1)"); CHECK_NEAR(z.imag(), pi, 1e-12);
    z = evc("abs(3+4i)"); CHECK_NEAR(z.real(), 5, 1e-12);
    z = evc("conj(2+3i)"); CHECK_NEAR(z.imag(), -3, 1e-12);
    z = evc("(1+i)/(1-i)"); CHECK_NEAR(z.imag(), 1, 1e-12);
    setAngleMode(AngleMode::Degrees);
    z = evc("arg(1+i)"); CHECK_NEAR(z.real(), 45, 1e-12);
    setAngleMode(AngleMode::Radians);
    // --- number formatting (fx-CG50 Norm1 reference strings)
    CHECK_EQ_STR(formatReal(0.1 + 0.2), "0.3");
    CHECK_EQ_STR(formatReal(1.0 / 3), "0.3333333333");
    CHECK_EQ_STR(formatReal(2.0 / 3), "0.6666666667");
    CHECK_EQ_STR(formatReal(123456789012.0), "1.23456789E+11");
    CHECK_EQ_STR(formatReal(1e10), "1E+10");
    CHECK_EQ_STR(formatReal(9999999999.0), "9999999999");
    CHECK_EQ_STR(formatReal(0.01), "0.01");
    CHECK_EQ_STR(formatReal(0.009), "9E-03");
    CHECK_EQ_STR(formatReal(0.009, {DisplayMode::Norm2, 0}), "0.009");
    CHECK_EQ_STR(formatReal(-42), "-42");
    CHECK_EQ_STR(formatReal(0), "0");
    CHECK_EQ_STR(formatReal(3.14159265358979), "3.141592654");
    CHECK_EQ_STR(formatReal(2.5, {DisplayMode::Fix, 3}), "2.500");
    CHECK_EQ_STR(formatReal(-0.0001, {DisplayMode::Fix, 2}), "0.00");
    CHECK_EQ_STR(formatReal(1234.5, {DisplayMode::Sci, 3}), "1.23E+03");
    CHECK_EQ_STR(formatReal(12345.0, {DisplayMode::Eng, 0}), "12.345E+03");
    CHECK_EQ_STR(formatReal(0.00012, {DisplayMode::Eng, 0}), "120E-06");
    CHECK_EQ_STR(formatValue({3, 4}, ComplexMode::Rectangular), "3+4i");
    CHECK_EQ_STR(formatValue({0, -1}, ComplexMode::Rectangular), "-i");
    CHECK_EQ_STR(formatValue({2, 0}, ComplexMode::Rectangular), "2");
    setAngleMode(AngleMode::Degrees);
    CHECK_EQ_STR(formatValue({0, 2}, ComplexMode::Polar), "2\xE2\x88\xA0" "90");
    setAngleMode(AngleMode::Radians);
    long long p = 0, q = 0;
    CHECK(exactFraction(1.0 / 3 + 1.0 / 6, p, q) && p == 1 && q == 2);
    CHECK(exactFraction(-0.75, p, q) && p == -3 && q == 4);
    CHECK(!exactFraction(std::sqrt(2.0), p, q));
    CHECK(!exactFraction(pi, p, q));
    return testutil::finish("expression_test");
}
