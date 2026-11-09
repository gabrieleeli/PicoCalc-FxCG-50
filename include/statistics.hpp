#pragma once
#include <string>
#include <utility>
#include <vector>

namespace fx::stats {

// fx-CG50 "1-Variable" results.
struct OneVar {
    double mean{}, sum{}, sumSq{}, popSd{}, sampleSd{}, n{}, min{}, q1{}, median{}, q3{}, max{};
    std::vector<double> modes; double modeCount{};
};
OneVar oneVariable(const std::vector<double>& x, const std::vector<double>* freq = nullptr);

// fx-CG50 "2-Variable" results.
struct TwoVar {
    double meanX{}, sumX{}, sumX2{}, popSdX{}, sampleSdX{}, n{};
    double meanY{}, sumY{}, sumY2{}, popSdY{}, sampleSdY{}, sumXY{};
    double minX{}, maxX{}, minY{}, maxY{};
};
TwoVar twoVariable(const std::vector<double>& x, const std::vector<double>& y);

enum class RegType { Linear, MedMed, Quad, Cubic, Quart, Log, Exp, AbExp, Power, Count };
const char* regName(RegType t) noexcept;          // "ax+b", "Med-Med", ...
const char* regFormula(RegType t) noexcept;       // "y=ax+b", "y=a+b·lnx", ...
struct Regression {
    RegType type{};
    std::vector<double> coef;                     // a, b, c, d, e in fx-CG50 order
    double r{}, r2{}, mse{};
    bool hasR{true};
    double predict(double x) const;
    std::string expression() const;               // linear text usable as a GRAPH function
};
Regression regress(RegType type, const std::vector<double>& x, const std::vector<double>& y);

// Distributions (lower tail unless stated).
double normalPd(double x, double sigma, double mu);
double normalCd(double lower, double upper, double sigma, double mu);
double inverseNormal(double area, double sigma, double mu);   // left tail
double binomialPd(double x, double n, double p);
double binomialCd(double x, double n, double p);              // P(X <= x)
double poissonPd(double x, double lambda);
double poissonCd(double x, double lambda);
double tCd(double lower, double upper, double df);
double chiSquareCd(double lower, double upper, double df);
double fCd(double lower, double upper, double df1, double df2);
double regularizedBeta(double x, double a, double b);
double regularizedGammaP(double a, double x);

struct TestResult { double statistic{}; double p{}; double df{}; double mean{}; double sd{}; double n{}; };
// One-sample Z and t tests against mu0; tail: 0 two-sided, -1 lower (<), +1 upper (>).
TestResult zTest1(const std::vector<double>& x, double mu0, double sigma, int tail);
TestResult tTest1(const std::vector<double>& x, double mu0, int tail);
struct Interval { double lower{}, upper{}, mean{}, n{}; };
Interval zInterval(const std::vector<double>& x, double sigma, double level);
Interval tInterval(const std::vector<double>& x, double level);
} // namespace fx::stats
