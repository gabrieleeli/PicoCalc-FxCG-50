// G-SOLVE numerical analysis checked against exact reference values,
// including the examples of the fx-CG50 manual (pages 5-57..5-60).
#include "graph_solve.hpp"
#include "expression.hpp"
#include "math_engine.hpp"
#include "test_util.hpp"
#include <cmath>

using namespace fx;
using namespace fx::graph;

namespace {
constexpr double kPi = 3.14159265358979323846;
RealFunction fn(const std::string& text) {
    auto ast = std::shared_ptr<const ast::Node>(parseExpression(text));
    return [ast](double x, double& y) {
        try { EvalContext c; c.bind("x", x); y = evaluateReal(*ast, c); return std::isfinite(y); } catch (...) { return false; }
    };
}
bool near(const std::vector<double>& got, std::initializer_list<double> want, double tol) {
    if (got.size() != want.size()) return false;
    std::size_t i = 0;
    for (double w : want) if (std::abs(got[i++] - w) > tol) return false;
    return true;
}
} // namespace

int main() {
    setAngleMode(AngleMode::Radians);
    // ---------------------------------------------------------------- roots
    CHECK(near(findRoots(fn("x^2-2"), -6.3, 6.3), {-std::sqrt(2.0), std::sqrt(2.0)}, 1e-14));
    CHECK(near(findRoots(fn("x^3-4x"), -6.3, 6.3), {-2, 0, 2}, 1e-14));                 // manual 5-57
    CHECK(near(findRoots(fn("sin(x)"), -6.3, 6.3), {-2 * kPi, -kPi, 0, kPi, 2 * kPi}, 1e-14));
    CHECK(near(findRoots(fn("x^2"), -6.3, 6.3), {0}, 1e-7));                            // touching root
    CHECK(near(findRoots(fn("(x-1)^2(x+2)"), -6.3, 6.3), {-2, 1}, 1e-7));
    CHECK(findRoots(fn("x^2+1"), -6.3, 6.3).empty());
    CHECK(findRoots(fn("1/x"), -6.3, 6.3).empty());                                     // pole: sign change, no root
    CHECK(near(findRoots(fn("tan(x)"), -6.3, 6.3), {-2 * kPi, -kPi, 0, kPi, 2 * kPi}, 1e-12));
    CHECK(findRoots(fn("int(x)+0.5"), -1.5, 1.5).empty());                              // jump -0.5 -> 0.5 at x = -1
    CHECK(findRoots(fn("sign(x)"), -1.3, 1.1).empty());                                  // jump through 0
    CHECK(near(findRoots(fn("sqrt(x)-1"), -6.3, 6.3), {1}, 1e-14));
    CHECK(near(findRoots(fn("ln(x)"), -6.3, 6.3), {1}, 1e-14));
    CHECK(findRoots(fn("x"), 1, 1).empty());
    // ---------------------------------------------------------------- extrema
    {
        const auto mx = findExtrema(fn("x^3-3x"), -6.3, 6.3, true);
        const auto mn = findExtrema(fn("x^3-3x"), -6.3, 6.3, false);
        CHECK(near(mx, {-1}, 1e-9) && near(mn, {1}, 1e-9));
        CHECK(near(findExtrema(fn("x^2-2"), -6.3, 6.3, false), {0}, 1e-10));
        CHECK(near(findExtrema(fn("sin(x)"), -6.3, 6.3, true), {-1.5 * kPi, 0.5 * kPi}, 1e-9));
        CHECK(near(findExtrema(fn("abs(x-1)"), -6.3, 6.3, false), {1}, 1e-9));             // corner
        CHECK(findExtrema(fn("3"), -6.3, 6.3, true).empty());                               // flat: none
        CHECK(findExtrema(fn("x"), -6.3, 6.3, false).empty());                              // only at the border
        CHECK(near(findExtrema(fn("x(x+2)(x-2)"), -6.3, 6.3, true), {-2 / std::sqrt(3.0)}, 1e-9));
        // value at the extremum
        double y; fn("x^3-3x")(mx[0], y); CHECK_NEAR(y, 2, 1e-12);
    }
    // ---------------------------------------------------------------- intersections, X-CAL
    {
        // manual 5-58: Y1 = x + 1, Y2 = x^2
        CHECK(near(findIntersections(fn("x+1"), fn("x^2"), -6.3, 6.3), {(1 - std::sqrt(5.0)) / 2, (1 + std::sqrt(5.0)) / 2}, 1e-14));
        CHECK(findIntersections(fn("x+1"), fn("x+2"), -6.3, 6.3).empty());              // parallel
        CHECK(near(findIntersections(fn("sin(x)"), fn("cos(x)"), 0, 6.3), {kPi / 4, 5 * kPi / 4}, 1e-13));
        // manual 5-58: X-CAL of y = 2.2 on Y2 = x(x+2)(x-2): three solutions
        const auto xs = findValue(fn("x(x+2)(x-2)"), 2.2, -6.3, 6.3);
        CHECK(xs.size() == 3);
        for (double x : xs) CHECK_NEAR(x * (x + 2) * (x - 2), 2.2, 1e-12);
        CHECK(findValue(fn("x^2"), -1, -6.3, 6.3).empty());
        CHECK(near(findValue(fn("x^2"), 0, -6.3, 6.3), {0}, 1e-7));
    }
    // ---------------------------------------------------------------- integrals and areas
    {
        CHECK_NEAR(integrate(fn("x(x+2)(x-2)"), -2, 0), 4, 1e-12);                        // manual 5-59
        CHECK_NEAR(integrate(fn("x(x+2)(x-2)"), 0, -2), -4, 1e-12);
        CHECK_NEAR(integrate(fn("x^2-2"), 0, 3), 3, 1e-12);
        CHECK_NEAR(integrate(fn("sin(x)"), -kPi, kPi), 0, 1e-12);
        CHECK_NEAR(area(fn("sin(x)"), -kPi, kPi), 4, 1e-10);                              // manual 5-60
        CHECK_NEAR(area(fn("x(x+2)(x-2)"), -2, 2), 8, 1e-10);
        CHECK_NEAR(integrate(fn("x(x+2)(x-2)"), -2, 2), 0, 1e-12);
        CHECK_NEAR(integrate(fn("e^x"), 0, 1), std::exp(1.0) - 1, 1e-12);
        CHECK_THROWS_AS(integrate(fn("sqrt(x)"), -1, 1), MathError);                      // undefined part
        CHECK_THROWS_AS(integrate(fn("1/x"), -1, 1), MathError);                          // pole at a sample
        CHECK(integrate(fn("x"), 2, 2) == 0);
        // Region between sin x and sin(x - pi/2) (manual 5-60, INTSECT)
        auto diff = [](double x, double& y) { y = std::sin(x) - std::sin(x - kPi / 2); return true; };
        const auto is = findIntersections(fn("sin(x)"), fn("sin(x-pi/2)"), -6.3, 6.3);
        CHECK(is.size() == 4);
        CHECK_NEAR(area(diff, is[1], is[2]), 2 * std::sqrt(2.0), 1e-10);
        CHECK_NEAR(integrate(diff, is[1], is[2]), 2 * std::sqrt(2.0), 1e-10);
    }
    // ---------------------------------------------------------------- Brent building blocks
    {
        double r = 0;
        CHECK(brentRoot(fn("x^3-2"), 0, 3, -2, 25, r)); CHECK_NEAR(r, std::cbrt(2.0), 1e-15);
        CHECK(!brentRoot(fn("x^2+1"), -1, 1, 2, 2, r));
        double m = 0;
        CHECK(brentExtremum(fn("(x-0.3)^2+1"), -1, 2, 1.0, m)); CHECK_NEAR(m, 0.3, 1e-7);
        CHECK(brentExtremum(fn("-(x-0.7)^2"), -1, 2, -1.0, m)); CHECK_NEAR(m, 0.7, 1e-7);
    }
    return testutil::finish("graph_solve_test");
}
