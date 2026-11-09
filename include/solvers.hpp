#pragma once
#include "expression.hpp"
#include "math_engine.hpp"
#include "number_format.hpp"
#include <string>
#include <vector>

namespace fx {

// All complex roots of a real polynomial, coefficients highest power first
// (degree 1..6 used by EQUATION, any degree accepted). Aberth-Ehrlich iteration
// with Newton polishing; nearly-real roots are snapped to the real axis and
// complex roots are returned as exact conjugate pairs. Throws for the zero polynomial.
std::vector<Complex> polynomialRoots(std::vector<double> coeffs);
double polynomialValue(const std::vector<double>& coeffs, double x) noexcept;

// Real solution set of P(x) rel 0 (rel: Less, LessEqual, Greater, GreaterEqual).
std::vector<SolutionInterval> polynomialInequality(const std::vector<double>& coeffs, InequalityRelation rel);
// "x<-1 , 2<=x<=3" style text; "Tutti i reali" / "Nessuna soluzione" for the trivial sets.
std::string formatSolutionSet(const std::vector<SolutionInterval>& set, DisplayFormat format = {});

// SOLVER: f(x)=g(x) (or an expression = 0) solved numerically for x.
struct SolverResult { double x{}; double left{}; double right{}; };
SolverResult solveEquation(const std::string& equation, double guess, const CalculatorMemory* memory = nullptr);
} // namespace fx
