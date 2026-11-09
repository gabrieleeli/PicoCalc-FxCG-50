#pragma once
#include <array>
#include <complex>
#include <cstddef>
#include <string>
#include <vector>
#include <cstdint>
#include <utility>
#include "limits.hpp"

namespace fx {
enum class AngleMode { Radians, Degrees, Gradians };
void setAngleMode(AngleMode mode);
AngleMode angleMode() noexcept;
double toRadians(double x);
double fromRadians(double x);
double factorial(unsigned n);
unsigned long long nPr(unsigned n, unsigned r);
unsigned long long nCr(unsigned n, unsigned r);
struct QuadraticResult { enum Kind { Linear, TwoReal, Double, Complex, Identity, Impossible } kind; double x1{}, x2{}, real{}, imag{}, delta{}; };
QuadraticResult solveQuadratic(double a, double b, double c);
// Solves a*x+b=0; reuses the linear/quadratic result conventions.
QuadraticResult solveLinearEquation(double a, double b);
enum class InequalityRelation { Less, LessEqual, Greater, GreaterEqual };
struct SolutionInterval { double lower{}, upper{}; bool lowerInclusive{}, upperInclusive{}; };
std::vector<SolutionInterval> solvePolynomialInequality(double a,double b,double c,InequalityRelation relation);
// Set operations on one-dimensional solution sets (interval collections).
std::vector<SolutionInterval> unionSolutionSets(const std::vector<SolutionInterval>& a,
                                                const std::vector<SolutionInterval>& b);
std::vector<SolutionInterval> intersectSolutionSets(const std::vector<SolutionInterval>& a,
                                                    const std::vector<SolutionInterval>& b);
// A system of inequalities is the intersection of each inequality's solution set.
std::vector<SolutionInterval> solveInequalitySystem(
    const std::vector<std::array<double,4>>& inequalities); // {a,b,c,relation: 0<,1<=,2>,3>=
struct LinearResult { enum Kind { Unique, Dependent, Inconsistent } kind; std::vector<double> solution; std::size_t rank{}; };
LinearResult solveLinearSystem(std::vector<std::vector<double>> A, std::vector<double> b, double eps=1e-10);
double integrateSimpson(double (*f)(double), double a, double b, unsigned intervals=200);
double bisectRoot(double (*f)(double), double a, double b, unsigned iterations=80);
double mean(const std::vector<double>& v);
double variance(const std::vector<double>& v, bool sample=true);
double stddev(const std::vector<double>& v, bool sample=true);
struct Regression { double slope{}, intercept{}, r{}; };
Regression linearRegression(const std::vector<double>& x, const std::vector<double>& y);
std::vector<double> vectorAdd(const std::vector<double>& a,const std::vector<double>& b);
double vectorDot(const std::vector<double>& a,const std::vector<double>& b);
std::vector<double> vectorCross3(const std::vector<double>& a,const std::vector<double>& b);
// Additional scientific-calculator functions (ES Plus / ClassWiz family coverage).
struct Fraction { std::int64_t numerator{}, denominator{1}; };
Fraction makeFraction(std::int64_t numerator, std::int64_t denominator);
Fraction addFractions(Fraction a, Fraction b);
Fraction multiplyFractions(Fraction a, Fraction b);
double percent(double value, double percentage);
double reciprocal(double value);
double nthRoot(double value, int n);
double logBase(double value, double base);
std::pair<double,double> polarToRect(double radius, double angle);
std::pair<double,double> rectToPolar(double x, double y);
struct Sexagesimal { int degrees{}; int minutes{}; double seconds{}; };
Sexagesimal decimalToSexagesimal(double degrees);
double sexagesimalToDecimal(Sexagesimal value);
std::string toBase(std::uint64_t value, unsigned base);
std::uint64_t fromBase(const std::string& value, unsigned base);
std::vector<std::uint64_t> primeFactors(std::uint64_t value);
using Matrix = std::vector<std::vector<double>>;
Matrix matrixAdd(const Matrix& a, const Matrix& b);
Matrix matrixMultiply(const Matrix& a, const Matrix& b);
double matrixDeterminant(Matrix a);
Matrix matrixInverse(Matrix a);
double vectorNorm(const std::vector<double>& a);

// Additional ClassWiz-style numerical/statistical core.
using Complex = std::complex<double>;
Complex complexPower(Complex z, Complex w);
Complex complexSqrt(Complex z);
double derivativeCentral(double (*f)(double), double x, double h=1e-5);
double solveNewton(double (*f)(double), double initial, unsigned iterations=80, double tolerance=1e-10);
double median(std::vector<double> values);
std::pair<double,double> quartiles(std::vector<double> values);
struct DescriptiveStats { std::size_t count{}; double sum{}, mean{}, population_sd{}, sample_sd{}, minimum{}, q1{}, median{}, q3{}, maximum{}; };
DescriptiveStats describe(const std::vector<double>& values);
Regression polynomialRegression(const std::vector<double>& x, const std::vector<double>& y, unsigned degree, std::vector<double>* coefficients=nullptr);
double normalPdf(double x, double mean=0.0, double sd=1.0);
double normalCdf(double x, double mean=0.0, double sd=1.0);
double inverseNormalCdf(double p, double mean=0.0, double sd=1.0);
double binomialPmf(unsigned n, unsigned k, double p);
double binomialCdf(unsigned n, unsigned k, double p);
double poissonPmf(unsigned k, double lambda);
double poissonCdf(unsigned k, double lambda);
// fx-CG50 sized sheet (999 rows x 26 columns on both boards). Storage is
// sparse (only non-zero cells, PSRAM when available), so the capacity is bound
// by free memory instead of a fixed 45x5 array; set() fails with
// std::bad_alloc without modifying the sheet when memory is exhausted.
struct Spreadsheet {
 static constexpr std::size_t Rows=limits::kProfile.spreadsheetRows, Cols=limits::kProfile.spreadsheetCols;
 void set(std::size_t row, std::size_t col, double value);
 double get(std::size_t row, std::size_t col) const;
 double sum(std::size_t col, std::size_t first, std::size_t last) const;
 double average(std::size_t col, std::size_t first, std::size_t last) const;
 std::size_t usedCells() const noexcept { return cells_.size(); }
private:
 std::vector<std::pair<std::uint32_t,double>> cells_; // sorted by row*Cols+col
};
}
