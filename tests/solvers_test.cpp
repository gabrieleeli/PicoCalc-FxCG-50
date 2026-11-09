// EQUATION mode numerics: polynomial roots, polynomial inequalities, SOLVER.
#include "solvers.hpp"
#include "test_util.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

using namespace fx;
static bool hasRoot(const std::vector<Complex>& r, Complex z, double tol = 1e-9) {
    for (const auto& x : r) if (std::abs(x - z) < tol) return true;
    return false;
}
static int realCount(const std::vector<Complex>& r) { return static_cast<int>(std::count_if(r.begin(), r.end(), [](const Complex& z) { return z.imag() == 0; })); }

int main() {
    auto r = polynomialRoots({1, -6, 11, -6});                     // (x-1)(x-2)(x-3)
    CHECK(r.size() == 3 && realCount(r) == 3);
    CHECK_NEAR(r[0].real(), 1, 1e-12); CHECK_NEAR(r[1].real(), 2, 1e-12); CHECK_NEAR(r[2].real(), 3, 1e-12);
    r = polynomialRoots({1, 0, 1});                                 // x^2+1
    CHECK(r.size() == 2 && hasRoot(r, {0, 1}) && hasRoot(r, {0, -1}));
    r = polynomialRoots({1, 0, 0, 0, -1});                          // x^4-1
    CHECK(r.size() == 4 && realCount(r) == 2 && hasRoot(r, {1, 0}) && hasRoot(r, {-1, 0}) && hasRoot(r, {0, 1}) && hasRoot(r, {0, -1}));
    CHECK(r[0].real() == -1 || std::abs(r[0].real() + 1) < 1e-12);  // real roots first, ascending
    r = polynomialRoots({1, 0, -3, 2});                             // (x-1)^2 (x+2): double root
    CHECK(r.size() == 3 && realCount(r) == 3 && hasRoot(r, {-2, 0}, 1e-9) && hasRoot(r, {1, 0}, 1e-6));
    r = polynomialRoots({1, 0, 0, 0, 0, 0, -1});                    // x^6-1: roots of unity
    CHECK(r.size() == 6 && realCount(r) == 2);
    for (int k = 0; k < 6; ++k) CHECK(hasRoot(r, std::polar(1.0, k * 3.14159265358979323846 / 3), 1e-9));
    r = polynomialRoots({1, -21, 175, -735, 1624, -1764, 720});     // (x-1)...(x-6)
    CHECK(r.size() == 6 && realCount(r) == 6);
    for (int k = 1; k <= 6; ++k) CHECK(hasRoot(r, {static_cast<double>(k), 0}, 1e-7));
    r = polynomialRoots({2, -3, 0, 5, -1, 7});                      // generic quintic: residual check
    CHECK(r.size() == 5);
    for (const auto& z : r) { Complex p = 0; for (double c : {2.0, -3.0, 0.0, 5.0, -1.0, 7.0}) p = p * z + c; CHECK(std::abs(p) < 1e-9); }
    for (const auto& z : r) if (z.imag() != 0) CHECK(hasRoot(r, std::conj(z), 1e-15));   // exact conjugate pairs
    r = polynomialRoots({0, 0, 3, -6});                             // leading zeros: 3x-6
    CHECK(r.size() == 1 && r[0] == Complex(2, 0));
    r = polynomialRoots({1, -2, 0, 0});                             // x^3-2x^2: zero roots
    CHECK(r.size() == 3 && hasRoot(r, {0, 0}) && hasRoot(r, {2, 0}));
    CHECK_THROWS_AS(polynomialRoots({0, 0, 0}), MathError);
    // Inequalities.
    using R = InequalityRelation;
    const double inf = std::numeric_limits<double>::infinity();
    auto s = polynomialInequality({1, -3, 2}, R::LessEqual);
    CHECK(s.size() == 1 && std::abs(s[0].lower - 1) < 1e-12 && std::abs(s[0].upper - 2) < 1e-12 && s[0].lowerInclusive && s[0].upperInclusive);
    CHECK_EQ_STR(formatSolutionSet(s), "1\xE2\x89\xA4x\xE2\x89\xA4" "2");
    s = polynomialInequality({1, 0, 1}, R::Greater); CHECK(s.size() == 1 && s[0].lower == -inf && s[0].upper == inf);
    CHECK_EQ_STR(formatSolutionSet(s), "Tutti i reali");
    s = polynomialInequality({1, 0, 1}, R::Less); CHECK(s.empty());
    CHECK_EQ_STR(formatSolutionSet(s), "Nessuna soluzione");
    s = polynomialInequality({1, -2, 1}, R::GreaterEqual); CHECK(s.size() == 1 && s[0].lower == -inf && s[0].upper == inf);
    s = polynomialInequality({1, -2, 1}, R::Greater); CHECK(s.size() == 2 && s[0].upper == 1 && !s[0].upperInclusive && s[1].lower == 1);
    s = polynomialInequality({1, -2, 1}, R::LessEqual); CHECK(s.size() == 1 && s[0].lower == 1 && s[0].upper == 1);
    CHECK_EQ_STR(formatSolutionSet(s), "x=1");
    s = polynomialInequality({1, 0, -1, 0}, R::Greater);            // x^3-x > 0
    CHECK(s.size() == 2 && std::abs(s[0].lower + 1) < 1e-12 && s[0].upper == 0 && s[1].lower == 1 && s[1].upper == inf);
    CHECK_EQ_STR(formatSolutionSet(s), "-1<x<0 , 1<x");
    s = polynomialInequality({2, -4}, R::Greater); CHECK(s.size() == 1 && s[0].lower == 2 && s[0].upper == inf);
    s = polynomialInequality({-1, 0, 4}, R::GreaterEqual);          // 4-x^2 >= 0
    CHECK(s.size() == 1 && s[0].lower == -2 && s[0].upper == 2);
    s = polynomialInequality({0, 0, 3}, R::Greater); CHECK(s.size() == 1);   // constant 3 > 0
    // Solver.
    CHECK_NEAR(solveEquation("x^2=2", 1).x, std::sqrt(2.0), 1e-12);
    CHECK_NEAR(solveEquation("cos(x)=x", 1).x, 0.7390851332151607, 1e-12);
    CHECK_NEAR(solveEquation("e^x=5", 0).x, std::log(5.0), 1e-12);
    CHECK_NEAR(solveEquation("x^3-2x-5", 2).x, 2.0945514815423265, 1e-12);
    auto sr = solveEquation("2x+1=7", 0); CHECK_NEAR(sr.x, 3, 1e-12); CHECK_NEAR(sr.left, 7, 1e-12); CHECK_NEAR(sr.right, 7, 1e-12);
    CHECK_THROWS_AS(solveEquation("1/x=0", 1), MathError);
    CHECK_THROWS_AS(solveEquation("x^2=-1", 1), MathError);
    CHECK_THROWS(solveEquation("x=1=2", 0));
    return testutil::finish("solvers_test");
}
