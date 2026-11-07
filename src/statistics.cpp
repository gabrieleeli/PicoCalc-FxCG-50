#include "statistics.hpp"
#include "expression.hpp"
#include "math_engine.hpp"
#include "number_format.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <numeric>
#include <stdexcept>

namespace fx::stats {
namespace {
void need(bool cond, const char* msg) { if (!cond) throw MathError(msg); }
double sq(double v) { return v * v; }
// Median of a sorted range [b, e).
double medianOf(const std::vector<double>& s, std::size_t b, std::size_t e) {
    const std::size_t n = e - b;
    if (n == 0) return std::numeric_limits<double>::quiet_NaN();
    return n % 2 ? s[b + n / 2] : 0.5 * (s[b + n / 2 - 1] + s[b + n / 2]);
}
double betaCF(double a, double b, double x) {
    const double eps = 3e-16, fpmin = 1e-300;
    double qab = a + b, qap = a + 1, qam = a - 1, c = 1, d = 1 - qab * x / qap;
    if (std::abs(d) < fpmin) d = fpmin;
    d = 1 / d; double h = d;
    for (int m = 1; m <= 300; ++m) {
        const int m2 = 2 * m;
        double aa = m * (b - m) * x / ((qam + m2) * (a + m2));
        d = 1 + aa * d; if (std::abs(d) < fpmin) d = fpmin;
        c = 1 + aa / c; if (std::abs(c) < fpmin) c = fpmin;
        d = 1 / d; h *= d * c;
        aa = -(a + m) * (qab + m) * x / ((a + m2) * (qap + m2));
        d = 1 + aa * d; if (std::abs(d) < fpmin) d = fpmin;
        c = 1 + aa / c; if (std::abs(c) < fpmin) c = fpmin;
        d = 1 / d; const double del = d * c; h *= del;
        if (std::abs(del - 1) < eps) break;
    }
    return h;
}
double tLower(double t, double df) {
    const double x = df / (df + t * t);
    const double ib = regularizedBeta(x, df / 2, 0.5);
    return t >= 0 ? 1 - 0.5 * ib : 0.5 * ib;
}
double chiLower(double x, double df) { return x <= 0 ? 0 : regularizedGammaP(df / 2, x / 2); }
double fLower(double x, double d1, double d2) { return x <= 0 ? 0 : regularizedBeta(d1 * x / (d1 * x + d2), d1 / 2, d2 / 2); }
double lowerOf(double v, double (*cdf)(double, double), double df) {
    if (v == -std::numeric_limits<double>::infinity()) return 0;
    if (v == std::numeric_limits<double>::infinity()) return 1;
    return cdf(v, df);
}
double pFromTail(double cdfAtStat, int tail) {
    if (tail < 0) return cdfAtStat;
    if (tail > 0) return 1 - cdfAtStat;
    return 2 * std::min(cdfAtStat, 1 - cdfAtStat);
}
double tQuantile(double p, double df) {
    double lo = -1e3, hi = 1e3;
    for (int i = 0; i < 200; ++i) { const double m = 0.5 * (lo + hi); if (tLower(m, df) < p) lo = m; else hi = m; }
    return 0.5 * (lo + hi);
}
void validate(const std::vector<double>& v) { for (double d : v) need(std::isfinite(d), "Math ERROR: dato non finito"); }
} // namespace

double regularizedBeta(double x, double a, double b) {
    need(a > 0 && b > 0, "Argument ERROR: parametri beta");
    if (x <= 0) return 0;
    if (x >= 1) return 1;
    const double bt = std::exp(std::lgamma(a + b) - std::lgamma(a) - std::lgamma(b) + a * std::log(x) + b * std::log1p(-x));
    if (x < (a + 1) / (a + b + 2)) return bt * betaCF(a, b, x) / a;
    return 1 - bt * betaCF(b, a, 1 - x) / b;
}
double regularizedGammaP(double a, double x) {
    need(a > 0, "Argument ERROR: parametro gamma");
    if (x <= 0) return 0;
    const double gln = std::lgamma(a);
    if (x < a + 1) {     // series
        double ap = a, del = 1 / a, sum = del;
        for (int n = 0; n < 1000; ++n) { ap += 1; del *= x / ap; sum += del; if (std::abs(del) < std::abs(sum) * 1e-16) break; }
        return sum * std::exp(-x + a * std::log(x) - gln);
    }
    double b = x + 1 - a, c = 1 / 1e-300, d = 1 / b, h = d;   // continued fraction (Lentz)
    for (int i = 1; i < 1000; ++i) {
        const double an = -i * (i - a); b += 2;
        d = an * d + b; if (std::abs(d) < 1e-300) d = 1e-300;
        c = b + an / c; if (std::abs(c) < 1e-300) c = 1e-300;
        d = 1 / d; const double del = d * c; h *= del;
        if (std::abs(del - 1) < 1e-16) break;
    }
    return 1 - std::exp(-x + a * std::log(x) - gln) * h;
}

OneVar oneVariable(const std::vector<double>& x, const std::vector<double>* freq) {
    need(!x.empty(), "Dimension ERROR: lista vuota");
    validate(x);
    std::vector<double> data;
    if (freq) {
        need(freq->size() == x.size(), "Dimension ERROR: frequenze di lunghezza diversa");
        for (std::size_t i = 0; i < x.size(); ++i) {
            const double f = (*freq)[i];
            need(f >= 0 && f == std::floor(f) && f <= 100000, "Argument ERROR: frequenze intere non negative");
            for (int k = 0; k < static_cast<int>(f); ++k) data.push_back(x[i]);
        }
        need(!data.empty(), "Dimension ERROR: frequenze tutte nulle");
    } else data = x;
    OneVar r;
    r.n = static_cast<double>(data.size());
    for (double v : data) { r.sum += v; r.sumSq += v * v; }
    r.mean = r.sum / r.n;
    double ss = 0; for (double v : data) ss += sq(v - r.mean);
    r.popSd = std::sqrt(ss / r.n);
    r.sampleSd = data.size() > 1 ? std::sqrt(ss / (r.n - 1)) : 0;
    std::vector<double> s = data; std::sort(s.begin(), s.end());
    r.min = s.front(); r.max = s.back();
    const std::size_t n = s.size();
    r.median = medianOf(s, 0, n);
    // fx-CG50 quartiles: medians of the lower and upper halves (median excluded for odd n).
    if (n == 1) { r.q1 = r.q3 = s[0]; }
    else { r.q1 = medianOf(s, 0, n / 2); r.q3 = medianOf(s, n % 2 ? n / 2 + 1 : n / 2, n); }
    std::map<double, int> counts; for (double v : s) ++counts[v];
    int best = 0; for (const auto& kv : counts) best = std::max(best, kv.second);
    for (const auto& kv : counts) if (kv.second == best) r.modes.push_back(kv.first);
    r.modeCount = best;
    return r;
}

TwoVar twoVariable(const std::vector<double>& x, const std::vector<double>& y) {
    need(!x.empty() && x.size() == y.size(), "Dimension ERROR: liste X e Y di lunghezza diversa o vuote");
    validate(x); validate(y);
    TwoVar r; r.n = static_cast<double>(x.size());
    r.minX = r.maxX = x[0]; r.minY = r.maxY = y[0];
    for (std::size_t i = 0; i < x.size(); ++i) {
        r.sumX += x[i]; r.sumX2 += x[i] * x[i]; r.sumY += y[i]; r.sumY2 += y[i] * y[i]; r.sumXY += x[i] * y[i];
        r.minX = std::min(r.minX, x[i]); r.maxX = std::max(r.maxX, x[i]); r.minY = std::min(r.minY, y[i]); r.maxY = std::max(r.maxY, y[i]);
    }
    r.meanX = r.sumX / r.n; r.meanY = r.sumY / r.n;
    double sxx = 0, syy = 0;
    for (std::size_t i = 0; i < x.size(); ++i) { sxx += sq(x[i] - r.meanX); syy += sq(y[i] - r.meanY); }
    r.popSdX = std::sqrt(sxx / r.n); r.popSdY = std::sqrt(syy / r.n);
    r.sampleSdX = x.size() > 1 ? std::sqrt(sxx / (r.n - 1)) : 0;
    r.sampleSdY = y.size() > 1 ? std::sqrt(syy / (r.n - 1)) : 0;
    return r;
}

const char* regName(RegType t) noexcept {
    static const char* n[] = {"ax+b", "Med-Med", "X\xC2\xB2", "X\xC2\xB3", "X\xE2\x81\xB4", "Log", "Exp", "ab^x", "Power"};
    return n[static_cast<int>(t) < static_cast<int>(RegType::Count) ? static_cast<int>(t) : 0];
}
const char* regFormula(RegType t) noexcept {
    static const char* f[] = {"y=ax+b", "y=ax+b (Med-Med)", "y=ax\xC2\xB2+bx+c", "y=ax\xC2\xB3+bx\xC2\xB2+cx+d", "y=ax\xE2\x81\xB4+bx\xC2\xB3+cx\xC2\xB2+dx+e",
                              "y=a+b\xC2\xB7lnx", "y=a\xC2\xB7" "e^(bx)", "y=a\xC2\xB7" "b^x", "y=a\xC2\xB7x^b"};
    return f[static_cast<int>(t) < static_cast<int>(RegType::Count) ? static_cast<int>(t) : 0];
}

double Regression::predict(double x) const {
    switch (type) {
    case RegType::Linear: case RegType::MedMed: return coef[0] * x + coef[1];
    case RegType::Quad: case RegType::Cubic: case RegType::Quart: { double v = 0; for (double c : coef) v = v * x + c; return v; }
    case RegType::Log: return coef[0] + coef[1] * std::log(x);
    case RegType::Exp: return coef[0] * std::exp(coef[1] * x);
    case RegType::AbExp: return coef[0] * std::pow(coef[1], x);
    case RegType::Power: return coef[0] * std::pow(x, coef[1]);
    default: return 0;
    }
}
std::string Regression::expression() const {
    auto num = [](double v) { std::string s = formatReal(v, {DisplayMode::Norm2, 0}); for (auto& ch : s) if (ch == 'E') ch = 'e'; return "(" + s + ")"; };
    switch (type) {
    case RegType::Linear: case RegType::MedMed: return num(coef[0]) + "x+" + num(coef[1]);
    case RegType::Quad: case RegType::Cubic: case RegType::Quart: {
        std::string s; const int deg = static_cast<int>(coef.size()) - 1;
        for (int i = 0; i <= deg; ++i) { if (i) s += "+"; s += num(coef[static_cast<std::size_t>(i)]); if (deg - i >= 1) s += "x"; if (deg - i >= 2) s += "^" + std::to_string(deg - i); }
        return s;
    }
    case RegType::Log: return num(coef[0]) + "+" + num(coef[1]) + "ln(x)";
    case RegType::Exp: return num(coef[0]) + "exp(" + num(coef[1]) + "x)";
    case RegType::AbExp: return num(coef[0]) + "*" + num(coef[1]) + "^x";
    case RegType::Power: return num(coef[0]) + "x^" + num(coef[1]);
    default: return "0";
    }
}

Regression regress(RegType type, const std::vector<double>& xIn, const std::vector<double>& yIn) {
    need(xIn.size() == yIn.size(), "Dimension ERROR: liste X e Y di lunghezza diversa");
    validate(xIn); validate(yIn);
    Regression r; r.type = type;
    std::vector<double> x = xIn, y = yIn;
    auto linearFit = [&](const std::vector<double>& u, const std::vector<double>& v, double& slope, double& icept, double& rr) {
        need(u.size() >= 2, "Dimension ERROR: servono almeno 2 coppie");
        const double n = static_cast<double>(u.size());
        const double mu = std::accumulate(u.begin(), u.end(), 0.0) / n, mv = std::accumulate(v.begin(), v.end(), 0.0) / n;
        double suu = 0, svv = 0, suv = 0;
        for (std::size_t i = 0; i < u.size(); ++i) { suu += sq(u[i] - mu); svv += sq(v[i] - mv); suv += (u[i] - mu) * (v[i] - mv); }
        need(suu > 0, "Math ERROR: valori X tutti uguali");
        slope = suv / suu; icept = mv - slope * mu; rr = svv > 0 ? suv / std::sqrt(suu * svv) : 1.0;
    };
    switch (type) {
    case RegType::Linear: { double a, b, rr; linearFit(x, y, a, b, rr); r.coef = {a, b}; r.r = rr; break; }
    case RegType::MedMed: {
        need(x.size() >= 3, "Dimension ERROR: Med-Med richiede almeno 3 punti");
        std::vector<std::pair<double, double>> p; for (std::size_t i = 0; i < x.size(); ++i) p.emplace_back(x[i], y[i]);
        std::sort(p.begin(), p.end());
        const std::size_t n = p.size(), k = n / 3, rem = n % 3;
        const std::size_t g1 = k + (rem == 2 ? 1 : 0), g3 = g1, g2 = n - g1 - g3;
        auto med = [&](std::size_t b, std::size_t e, bool useY) { std::vector<double> v; for (std::size_t i = b; i < e; ++i) v.push_back(useY ? p[i].second : p[i].first); std::sort(v.begin(), v.end()); return medianOf(v, 0, v.size()); };
        const double x1 = med(0, g1, false), y1 = med(0, g1, true), x2 = med(g1, g1 + g2, false), y2 = med(g1, g1 + g2, true), x3 = med(n - g3, n, false), y3 = med(n - g3, n, true);
        need(x3 != x1, "Math ERROR: gruppi Med-Med degeneri");
        const double a = (y3 - y1) / (x3 - x1);
        const double b = ((y1 - a * x1) + (y2 - a * x2) + (y3 - a * x3)) / 3.0;
        r.coef = {a, b}; r.hasR = false;
        break;
    }
    case RegType::Quad: case RegType::Cubic: case RegType::Quart: {
        const unsigned deg = type == RegType::Quad ? 2 : type == RegType::Cubic ? 3 : 4;
        need(x.size() > deg, "Dimension ERROR: dati insufficienti per il grado");
        std::vector<double> c;
        (void)polynomialRegression(x, y, deg, &c);       // ascending powers
        r.coef.assign(c.rbegin(), c.rend());               // a (highest) ... constant
        r.hasR = false;
        break;
    }
    case RegType::Log: {
        std::vector<double> lx; for (double v : x) { need(v > 0, "Math ERROR: Log richiede X>0"); lx.push_back(std::log(v)); }
        double b, a, rr; linearFit(lx, y, b, a, rr); r.coef = {a, b}; r.r = rr; break;
    }
    case RegType::Exp: case RegType::AbExp: {
        std::vector<double> ly; for (double v : y) { need(v > 0, "Math ERROR: Exp richiede Y>0"); ly.push_back(std::log(v)); }
        double b, lna, rr; linearFit(x, ly, b, lna, rr);
        r.coef = {std::exp(lna), type == RegType::Exp ? b : std::exp(b)}; r.r = rr; break;
    }
    case RegType::Power: {
        std::vector<double> lx, ly;
        for (std::size_t i = 0; i < x.size(); ++i) { need(x[i] > 0 && y[i] > 0, "Math ERROR: Power richiede X>0 e Y>0"); lx.push_back(std::log(x[i])); ly.push_back(std::log(y[i])); }
        double b, lna, rr; linearFit(lx, ly, b, lna, rr); r.coef = {std::exp(lna), b}; r.r = rr; break;
    }
    default: throw MathError("Argument ERROR: tipo di regressione");
    }
    // r^2 and MSe on the original scale (fx-CG50 reports MSe = SSres/(n-2) for 2-parameter models).
    double ssRes = 0, ssTot = 0; const double my = std::accumulate(y.begin(), y.end(), 0.0) / static_cast<double>(y.size());
    for (std::size_t i = 0; i < x.size(); ++i) { ssRes += sq(y[i] - r.predict(x[i])); ssTot += sq(y[i] - my); }
    const double params = static_cast<double>(r.coef.size());
    r.mse = x.size() > params ? ssRes / (static_cast<double>(x.size()) - params) : 0;
    r.r2 = r.hasR ? r.r * r.r : (ssTot > 0 ? 1 - ssRes / ssTot : 1.0);
    return r;
}

double normalPd(double x, double s, double m) { need(s > 0, "Argument ERROR: sigma>0"); return normalPdf(x, m, s); }
double normalCd(double lo, double hi, double s, double m) {
    need(s > 0, "Argument ERROR: sigma>0"); need(lo <= hi, "Argument ERROR: Lower<=Upper");
    auto cdf = [&](double v) { return std::isinf(v) ? (v < 0 ? 0.0 : 1.0) : normalCdf(v, m, s); };
    return cdf(hi) - cdf(lo);
}
double inverseNormal(double a, double s, double m) { need(a > 0 && a < 1 && s > 0, "Argument ERROR: 0<Area<1, sigma>0"); return inverseNormalCdf(a, m, s); }
double binomialPd(double x, double n, double p) {
    need(n >= 0 && n == std::floor(n) && p >= 0 && p <= 1, "Argument ERROR: n intero >=0, 0<=p<=1");
    if (x < 0 || x > n || x != std::floor(x)) return 0;
    if (p == 0) return x == 0 ? 1 : 0;
    if (p == 1) return x == n ? 1 : 0;
    return std::exp(std::lgamma(n + 1) - std::lgamma(x + 1) - std::lgamma(n - x + 1) + x * std::log(p) + (n - x) * std::log1p(-p));
}
double binomialCd(double x, double n, double p) {
    need(n >= 0 && n == std::floor(n) && n <= 1e6, "Argument ERROR: n intero 0..1e6");
    double s = 0; const double top = std::min(std::floor(x), n);
    for (double k = 0; k <= top; ++k) s += binomialPd(k, n, p);
    return std::min(1.0, s);
}
double poissonPd(double x, double l) {
    need(l > 0 && std::isfinite(l), "Argument ERROR: lambda>0");
    if (x < 0 || x != std::floor(x)) return 0;
    return std::exp(-l + x * std::log(l) - std::lgamma(x + 1));
}
double poissonCd(double x, double l) {
    need(l > 0 && x <= 1e6, "Argument ERROR: lambda>0");
    double s = 0; for (double k = 0; k <= std::floor(x); ++k) s += poissonPd(k, l);
    return std::min(1.0, s);
}
double tCd(double lo, double hi, double df) {
    need(df > 0 && lo <= hi, "Argument ERROR: df>0, Lower<=Upper");
    return lowerOf(hi, tLower, df) - lowerOf(lo, tLower, df);
}
double chiSquareCd(double lo, double hi, double df) {
    need(df > 0 && lo <= hi, "Argument ERROR: df>0, Lower<=Upper");
    return lowerOf(hi, chiLower, df) - lowerOf(lo, chiLower, df);
}
double fCd(double lo, double hi, double d1, double d2) {
    need(d1 > 0 && d2 > 0 && lo <= hi, "Argument ERROR: df>0, Lower<=Upper");
    auto cdf = [&](double v) { if (v == std::numeric_limits<double>::infinity()) return 1.0; if (v <= 0) return 0.0; return fLower(v, d1, d2); };
    return cdf(hi) - cdf(lo);
}

TestResult zTest1(const std::vector<double>& x, double mu0, double sigma, int tail) {
    need(x.size() >= 1 && sigma > 0, "Argument ERROR: dati e sigma>0");
    const auto o = oneVariable(x);
    TestResult t; t.n = o.n; t.mean = o.mean; t.sd = o.sampleSd;
    t.statistic = (o.mean - mu0) / (sigma / std::sqrt(o.n));
    t.p = pFromTail(normalCdf(t.statistic), tail);
    return t;
}
TestResult tTest1(const std::vector<double>& x, double mu0, int tail) {
    need(x.size() >= 2, "Dimension ERROR: servono almeno 2 dati");
    const auto o = oneVariable(x);
    need(o.sampleSd > 0, "Math ERROR: deviazione standard nulla");
    TestResult t; t.n = o.n; t.mean = o.mean; t.sd = o.sampleSd; t.df = o.n - 1;
    t.statistic = (o.mean - mu0) / (o.sampleSd / std::sqrt(o.n));
    t.p = pFromTail(tLower(t.statistic, t.df), tail);
    return t;
}
Interval zInterval(const std::vector<double>& x, double sigma, double level) {
    need(!x.empty() && sigma > 0 && level > 0 && level < 1, "Argument ERROR: 0<C-Level<1, sigma>0");
    const auto o = oneVariable(x);
    const double z = inverseNormalCdf(1 - (1 - level) / 2), h = z * sigma / std::sqrt(o.n);
    return {o.mean - h, o.mean + h, o.mean, o.n};
}
Interval tInterval(const std::vector<double>& x, double level) {
    need(x.size() >= 2 && level > 0 && level < 1, "Argument ERROR: almeno 2 dati, 0<C-Level<1");
    const auto o = oneVariable(x);
    const double t = tQuantile(1 - (1 - level) / 2, o.n - 1), h = t * o.sampleSd / std::sqrt(o.n);
    return {o.mean - h, o.mean + h, o.mean, o.n};
}
} // namespace fx::stats
