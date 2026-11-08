#pragma once
// G-SOLVE numerical analysis (fx-CG50 manual 5-56..5-61) on real functions:
//  - roots: scan of the interval, Brent's method on every sign change, touching
//    roots (even multiplicity) found as extrema whose value is zero, poles and
//    jumps rejected by the residual;
//  - extrema: scan, then the zero of the Ridders derivative by Brent's method
//    inside the bracket, with Brent's parabolic minimisation as fallback;
//  - integrals: adaptive Gauss-Kronrod of the shared engine; areas split at the
//    zeros of the integrand.
// Functions return false where they are undefined.
#include <cstddef>
#include <functional>
#include <vector>

namespace fx::graph {

using RealFunction = std::function<bool(double, double&)>;

constexpr int DefaultSolveSamples = 1280;     // 4 per screen column
constexpr std::size_t MaxSolutions = 100;

// Brent-Dekker root of f in [a,b] given f(a) and f(b) of opposite signs.
// False if f becomes undefined inside the bracket.
bool brentRoot(const RealFunction& f, double a, double b, double fa, double fb, double& root);
// Brent's parabolic/golden minimisation of f in [a,b] (maximisation with sign = -1).
bool brentExtremum(const RealFunction& f, double a, double b, double sign, double& x);

std::vector<double> findRoots(const RealFunction& f, double a, double b, int samples = DefaultSolveSamples);
std::vector<double> findExtrema(const RealFunction& f, double a, double b, bool maximum, int samples = DefaultSolveSamples);
// Zeros of f - g where both are defined.
std::vector<double> findIntersections(const RealFunction& f, const RealFunction& g, double a, double b, int samples = DefaultSolveSamples);
// Solutions of f(x) = y.
std::vector<double> findValue(const RealFunction& f, double y, double a, double b, int samples = DefaultSolveSamples);

// Integral of f over [a,b] (a > b gives the negative). Throws MathError where f is undefined.
double integrate(const RealFunction& f, double a, double b);
// Integral of |f| over [a,b], split at the zeros of f. Throws MathError.
double area(const RealFunction& f, double a, double b);
} // namespace fx::graph
