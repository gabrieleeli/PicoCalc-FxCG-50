// Nucleo di Statistics della v1.18.0 (solo Pimoroni Pico Plus 2 W): vedi stat_engine.hpp.
// Formule dal comportamento descritto nel capitolo 6 del manuale fx-CG50
// (nessun codice Casio). I quantili vengono da calc_library (lo stesso motore
// delle funzioni DIST di Run-Matrix).
#include "stat_engine.hpp"
#include "calc_library.hpp"
#include "expression.hpp"
#include "mode_codec.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <limits>
#include <numeric>

namespace fx::stats {
namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kInf = std::numeric_limits<double>::infinity();
void need(bool cond, const char* msg) { if (!cond) throw MathError(msg); }
void needDim(bool cond, const char* msg) { if (!cond) throw DimensionError(msg); }
double sq(double v) { return v * v; }
void finiteAll(const std::vector<double>& v) { for (double d : v) need(std::isfinite(d), "Math ERROR: dato non finito"); }
// Un risultato oltre il double (dati enormi) e' un Math ERROR come sulla calcolatrice (trovato dal fuzzing).
double finiteResult(double v) { need(std::isfinite(v), "Math ERROR: risultato fuori intervallo"); return v; }

// Coppie (valore, peso > 0) ordinate per valore; pesi interi.
struct Weighted { std::vector<std::pair<double, double>> d; double n{0}; };
Weighted weighted(const std::vector<double>& x, const std::vector<double>* freq) {
    finiteAll(x);
    checkFrequencies(x.size(), freq);
    Weighted w;
    for (std::size_t i = 0; i < x.size(); ++i) {
        const double f = freq ? (*freq)[i] : 1.0;
        if (f > 0) { w.d.emplace_back(x[i], f); w.n += f; }
    }
    needDim(!w.d.empty(), "Dimension ERROR: nessun dato (lista vuota o frequenze nulle)");
    std::stable_sort(w.d.begin(), w.d.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    return w;
}
// Elemento k (da 0) dei dati ripetuti e ordinati.
double at(const Weighted& w, double k) {
    double seen = 0;
    for (const auto& [v, f] : w.d) { seen += f; if (k < seen) return v; }
    return w.d.back().first;
}
double medianRange(const Weighted& w, double b, double e) {
    const double len = e - b;
    if (len <= 0) return std::numeric_limits<double>::quiet_NaN();
    const double half = std::floor(len / 2);
    return std::fmod(len, 2) != 0 ? at(w, b + half) : 0.5 * (at(w, b + half - 1) + at(w, b + half));
}
// Q1Q3 Type OnData: primo dato con frequenza cumulata relativa > p; se = p, media con il successivo.
double onDataQuantile(const Weighted& w, double num, double den) {
    double cum = 0;
    for (std::size_t i = 0; i < w.d.size(); ++i) {
        cum += w.d[i].second;
        const double lhs = cum * den, rhs = w.n * num;       // cum/n confrontato con num/den senza divisioni
        if (lhs == rhs) return i + 1 < w.d.size() ? 0.5 * (w.d[i].first + w.d[i + 1].first) : w.d[i].first;
        if (lhs > rhs) return w.d[i].first;
    }
    return w.d.back().first;
}

// Minimi quadrati pesati di una retta v = slope*u + icept.
struct Line { double slope{0}, icept{0}, r{0}, n{0}; };
Line lineFit(const std::vector<double>& u, const std::vector<double>& v, const std::vector<double>& w) {
    double W = 0, su = 0, sv = 0;
    for (std::size_t i = 0; i < u.size(); ++i) { W += w[i]; su += w[i] * u[i]; sv += w[i] * v[i]; }
    needDim(W >= 2, "Dimension ERROR: servono almeno 2 dati");
    const double mu = su / W, mv = sv / W;
    double suu = 0, svv = 0, suv = 0;
    for (std::size_t i = 0; i < u.size(); ++i) { suu += w[i] * sq(u[i] - mu); svv += w[i] * sq(v[i] - mv); suv += w[i] * (u[i] - mu) * (v[i] - mv); }
    need(suu > 0, "Math ERROR: valori X tutti uguali");
    Line l;
    l.slope = suv / suu; l.icept = mv - l.slope * mu; l.n = W;
    l.r = svv > 0 ? suv / std::sqrt(suu * svv) : 1.0;
    return l;
}

// Minimi quadrati pesati di un polinomio (QR di Householder con colonne scalate).
// coef: dal grado piu' alto al termine noto.
std::vector<double> polyFit(const std::vector<double>& x, const std::vector<double>& y, const std::vector<double>& w, unsigned deg) {
    const std::size_t k = deg + 1;
    std::vector<std::size_t> rows;
    for (std::size_t i = 0; i < x.size(); ++i) if (w[i] > 0) rows.push_back(i);
    const std::size_t m = rows.size();
    needDim(m > deg, "Dimension ERROR: dati insufficienti per il grado");
    std::vector<double> A(m * k), b(m), scale(k, 0);
    for (std::size_t r = 0; r < m; ++r) {
        const std::size_t i = rows[r];
        const double sw = std::sqrt(w[i]);
        for (std::size_t j = 0; j < k; ++j) A[r * k + j] = sw * std::pow(x[i], static_cast<double>(deg - j));
        b[r] = sw * y[i];
    }
    for (std::size_t j = 0; j < k; ++j) {
        double s = 0; for (std::size_t r = 0; r < m; ++r) s += sq(A[r * k + j]);
        s = std::sqrt(s);
        need(s > 0, "Math ERROR: valori X degeneri");
        scale[j] = s;
        for (std::size_t r = 0; r < m; ++r) A[r * k + j] /= s;
    }
    for (std::size_t j = 0; j < k; ++j) {
        double norm = 0; for (std::size_t r = j; r < m; ++r) norm += sq(A[r * k + j]);
        norm = std::sqrt(norm);
        need(norm > 1e-13, "Math ERROR: valori X degeneri per il grado");
        const double alpha = A[j * k + j] > 0 ? -norm : norm;
        std::vector<double> v(m - j);
        for (std::size_t r = j; r < m; ++r) v[r - j] = A[r * k + j];
        v[0] -= alpha;
        double vv = 0; for (double t : v) vv += t * t;
        if (vv == 0) continue;
        for (std::size_t c = j; c < k; ++c) {
            double dot = 0; for (std::size_t r = j; r < m; ++r) dot += v[r - j] * A[r * k + c];
            const double f = 2 * dot / vv;
            for (std::size_t r = j; r < m; ++r) A[r * k + c] -= f * v[r - j];
        }
        double dot = 0; for (std::size_t r = j; r < m; ++r) dot += v[r - j] * b[r];
        const double f = 2 * dot / vv;
        for (std::size_t r = j; r < m; ++r) b[r] -= f * v[r - j];
    }
    std::vector<double> c(k);
    for (std::size_t jj = k; jj-- > 0;) {
        double s = b[jj];
        for (std::size_t q = jj + 1; q < k; ++q) s -= A[jj * k + q] * c[q];
        need(std::abs(A[jj * k + jj]) > 1e-13, "Math ERROR: valori X degeneri per il grado");
        c[jj] = s / A[jj * k + jj];
    }
    for (std::size_t j = 0; j < k; ++j) c[j] /= scale[j];
    return c;
}
// Sistema lineare k x k (eliminazione con pivot parziale); false se singolare.
bool solveSmall(std::vector<double> M, std::vector<double> rhs, std::size_t k, std::vector<double>& out) {
    for (std::size_t c = 0; c < k; ++c) {
        std::size_t piv = c;
        for (std::size_t r = c + 1; r < k; ++r) if (std::abs(M[r * k + c]) > std::abs(M[piv * k + c])) piv = r;
        if (!(std::abs(M[piv * k + c]) > 1e-300)) return false;
        if (piv != c) { for (std::size_t q = 0; q < k; ++q) std::swap(M[c * k + q], M[piv * k + q]); std::swap(rhs[c], rhs[piv]); }
        for (std::size_t r = c + 1; r < k; ++r) {
            const double f = M[r * k + c] / M[c * k + c];
            for (std::size_t q = c; q < k; ++q) M[r * k + q] -= f * M[c * k + q];
            rhs[r] -= f * rhs[c];
        }
    }
    out.assign(k, 0);
    for (std::size_t r = k; r-- > 0;) {
        double s = rhs[r];
        for (std::size_t q = r + 1; q < k; ++q) s -= M[r * k + q] * out[q];
        out[r] = s / M[r * k + r];
    }
    for (double v : out) if (!std::isfinite(v)) return false;
    return true;
}
// Modelli non lineari: valore e gradiente rispetto ai parametri.
double sinModel(const std::array<double, 4>& p, double x, double* g) {
    const double ph = p[1] * x + p[2], s = std::sin(ph), c = std::cos(ph);
    if (g) { g[0] = s; g[1] = p[0] * x * c; g[2] = p[0] * c; g[3] = 1; }
    return p[0] * s + p[3];
}
double logisticModel(const std::array<double, 4>& p, double x, double* g) {
    const double e = std::exp(-p[1] * x), den = 1 + p[0] * e;
    if (g) { g[0] = -p[2] * e / (den * den); g[1] = p[2] * p[0] * x * e / (den * den); g[2] = 1 / den; g[3] = 0; }
    return p[2] / den;
}
double wrapAngle(double c) {
    c = std::fmod(c, 2 * kPi);
    if (c <= -kPi) c += 2 * kPi;
    if (c > kPi) c -= 2 * kPi;
    return c;
}
std::string num15(double v) {
    char b[40];
    std::snprintf(b, sizeof b, "%.15g", v);
    return std::string("(") + b + ")";
}
} // namespace

// ---------------------------------------------------------------- dati
void checkFrequencies(std::size_t size, const std::vector<double>* freq) {
    if (!freq) return;
    needDim(freq->size() == size, "Dimension ERROR: frequenze di lunghezza diversa dai dati");
    for (double f : *freq) need(std::isfinite(f) && f >= 0 && f == std::floor(f) && f <= 100000, "Argument ERROR: frequenze intere da 0 a 100000");
}

OneVar oneVarEx(const std::vector<double>& x, const std::vector<double>* freq, QuartileRule rule) {
    needDim(!x.empty(), "Dimension ERROR: lista vuota");
    const Weighted w = weighted(x, freq);
    OneVar r;
    r.n = w.n;
    for (const auto& [v, f] : w.d) { r.sum += f * v; r.sumSq += f * v * v; }
    r.mean = r.sum / r.n;
    double ss = 0; for (const auto& [v, f] : w.d) ss += f * sq(v - r.mean);
    r.popSd = std::sqrt(ss / r.n);
    r.sampleSd = r.n > 1 ? std::sqrt(ss / (r.n - 1)) : 0;
    r.min = w.d.front().first; r.max = w.d.back().first;
    r.median = medianRange(w, 0, r.n);
    if (r.n == 1) { r.q1 = r.q3 = r.min; }
    else if (rule == QuartileRule::OnData) { r.q1 = onDataQuantile(w, 1, 4); r.q3 = onDataQuantile(w, 3, 4); }
    else {
        const double h = std::floor(r.n / 2);
        r.q1 = medianRange(w, 0, h);
        r.q3 = medianRange(w, std::fmod(r.n, 2) != 0 ? h + 1 : h, r.n);
    }
    std::vector<std::pair<double, double>> counts;
    for (const auto& [v, f] : w.d) {
        if (!counts.empty() && counts.back().first == v) counts.back().second += f;
        else counts.emplace_back(v, f);
    }
    double best = 0; for (const auto& c : counts) best = std::max(best, c.second);
    for (const auto& c : counts) if (c.second == best) r.modes.push_back(c.first);
    r.modeCount = best;
    for (double v : {r.sum, r.sumSq, r.mean, r.popSd, r.sampleSd, r.median, r.q1, r.q3}) (void)finiteResult(v);
    return r;
}

TwoVar twoVarEx(const std::vector<double>& x, const std::vector<double>& y, const std::vector<double>* freq) {
    needDim(!x.empty() && x.size() == y.size(), "Dimension ERROR: liste X e Y di lunghezza diversa o vuote");
    finiteAll(x); finiteAll(y);
    checkFrequencies(x.size(), freq);
    TwoVar r;
    bool first = true;
    for (std::size_t i = 0; i < x.size(); ++i) {
        const double f = freq ? (*freq)[i] : 1.0;
        if (f <= 0) continue;
        r.n += f; r.sumX += f * x[i]; r.sumX2 += f * x[i] * x[i]; r.sumY += f * y[i]; r.sumY2 += f * y[i] * y[i]; r.sumXY += f * x[i] * y[i];
        if (first) { r.minX = r.maxX = x[i]; r.minY = r.maxY = y[i]; first = false; }
        r.minX = std::min(r.minX, x[i]); r.maxX = std::max(r.maxX, x[i]); r.minY = std::min(r.minY, y[i]); r.maxY = std::max(r.maxY, y[i]);
    }
    needDim(r.n > 0, "Dimension ERROR: frequenze tutte nulle");
    r.meanX = r.sumX / r.n; r.meanY = r.sumY / r.n;
    double sxx = 0, syy = 0;
    for (std::size_t i = 0; i < x.size(); ++i) {
        const double f = freq ? (*freq)[i] : 1.0;
        sxx += f * sq(x[i] - r.meanX); syy += f * sq(y[i] - r.meanY);
    }
    r.popSdX = std::sqrt(sxx / r.n); r.popSdY = std::sqrt(syy / r.n);
    r.sampleSdX = r.n > 1 ? std::sqrt(sxx / (r.n - 1)) : 0;
    r.sampleSdY = r.n > 1 ? std::sqrt(syy / (r.n - 1)) : 0;
    for (double v : {r.sumX, r.sumX2, r.sumY, r.sumY2, r.sumXY, r.meanX, r.meanY, r.popSdX, r.popSdY, r.sampleSdX, r.sampleSdY}) (void)finiteResult(v);
    return r;
}

// ---------------------------------------------------------------- code
double normalLower(double z) { return 0.5 * std::erfc(-z / std::sqrt(2.0)); }
double normalUpper(double z) { return 0.5 * std::erfc(z / std::sqrt(2.0)); }
double tUpperTail(double t, double df) {
    need(df > 0, "Argument ERROR: df>0");
    if (std::isinf(t)) return t > 0 ? 0 : 1;
    const double ib = regularizedBeta(df / (df + t * t), df / 2, 0.5);
    return t >= 0 ? 0.5 * ib : 1 - 0.5 * ib;
}
double tLowerTail(double t, double df) { return tUpperTail(-t, df); }
double gammaQ(double a, double x) {
    need(a > 0, "Argument ERROR: parametro gamma");
    if (x <= 0) return 1;
    if (x < a + 1) return 1 - regularizedGammaP(a, x);
    const double gln = std::lgamma(a);
    double b = x + 1 - a, c = 1 / 1e-300, d = 1 / b, h = d;
    for (int i = 1; i < 2000; ++i) {
        const double an = -i * (i - a); b += 2;
        d = an * d + b; if (std::abs(d) < 1e-300) d = 1e-300;
        c = b + an / c; if (std::abs(c) < 1e-300) c = 1e-300;
        d = 1 / d; const double del = d * c; h *= del;
        if (std::abs(del - 1) < 1e-16) break;
    }
    return std::exp(-x + a * std::log(x) - gln) * h;
}
double chiUpperTail(double x, double df) { need(df > 0, "Argument ERROR: df>0"); return x <= 0 ? 1 : gammaQ(df / 2, x / 2); }
double fLowerTail(double x, double d1, double d2) {
    need(d1 > 0 && d2 > 0, "Argument ERROR: df>0");
    if (x <= 0) return 0;
    if (std::isinf(x)) return 1;
    return regularizedBeta(d1 * x / (d1 * x + d2), d1 / 2, d2 / 2);
}
double fUpperTail(double x, double d1, double d2) {
    need(d1 > 0 && d2 > 0, "Argument ERROR: df>0");
    if (x <= 0) return 1;
    if (std::isinf(x)) return 0;
    return regularizedBeta(d2 / (d2 + d1 * x), d2 / 2, d1 / 2);
}
double pValue(double lower, double upper, int tail) {
    if (tail < 0) return lower;
    if (tail > 0) return upper;
    return std::min(1.0, 2 * std::min(lower, upper));
}
double zCritical(double level) {
    need(level > 0 && level < 1, "Argument ERROR: 0<C-Level<1");
    return -calclib::invNormCD(-1, (1 - level) / 2, 1, 0);    // per simmetria: niente 1 - area
}
double tCritical(double level, double df) {
    need(level > 0 && level < 1, "Argument ERROR: 0<C-Level<1");
    return calclib::invTCD((1 - level) / 2, df);
}

// ---------------------------------------------------------------- regressioni
const char* regTitle(Reg t) noexcept {
    static const char* const n[] = {"LinearReg(ax+b)", "Med-MedLine", "QuadReg", "CubicReg", "QuartReg", "LogReg", "ExpReg(a\xC2\xB7" "e^bx)",
                                    "ExpReg(a\xC2\xB7" "b^x)", "PowerReg", "SinReg", "LogisticReg", "LinearReg(a+bx)"};
    const auto i = static_cast<std::size_t>(t);
    return i < 12 ? n[i] : "?";
}
const char* regModel(Reg t) noexcept {
    static const char* const f[] = {"y=a\xC2\xB7x+b", "y=a\xC2\xB7x+b", "y=a\xC2\xB7x\xC2\xB2+b\xC2\xB7x+c", "y=a\xC2\xB7x\xC2\xB3+b\xC2\xB7x\xC2\xB2+c\xC2\xB7x+d",
                                    "y=a\xC2\xB7x\xE2\x81\xB4+b\xC2\xB7x\xC2\xB3+c\xC2\xB7x\xC2\xB2+d\xC2\xB7x+e", "y=a+b\xC2\xB7lnx", "y=a\xC2\xB7" "e^(b\xC2\xB7x)",
                                    "y=a\xC2\xB7" "b^x", "y=a\xC2\xB7x^b", "y=a\xC2\xB7sin(b\xC2\xB7x+c)+d", "y=c/(1+a\xC2\xB7" "e^(-b\xC2\xB7x))", "y=a+b\xC2\xB7x"};
    const auto i = static_cast<std::size_t>(t);
    return i < 12 ? f[i] : "?";
}
std::size_t regCoefficientCount(Reg t) noexcept {
    switch (t) {
    case Reg::Quad: case Reg::Logistic: return 3;
    case Reg::Cubic: case Reg::Sin: return 4;
    case Reg::Quart: return 5;
    default: return 2;
    }
}
double Fit::predict(double x) const {
    const auto& c = coef;
    switch (type) {
    case Reg::LinearAx: case Reg::MedMed: return c[0] * x + c[1];
    case Reg::LinearA: return c[0] + c[1] * x;
    case Reg::Quad: case Reg::Cubic: case Reg::Quart: { double v = 0; for (std::size_t i = 0; i < ncoef; ++i) v = v * x + c[i]; return v; }
    case Reg::Log: return x > 0 ? c[0] + c[1] * std::log(x) : std::numeric_limits<double>::quiet_NaN();
    case Reg::ExpE: return c[0] * std::exp(c[1] * x);
    case Reg::ExpAb: return c[0] * std::pow(c[1], x);
    case Reg::Power: return c[0] * std::pow(x, c[1]);
    case Reg::Sin: return c[0] * std::sin(c[1] * x + c[2]) + c[3];
    case Reg::Logistic: return c[2] / (1 + c[0] * std::exp(-c[1] * x));
    default: return std::numeric_limits<double>::quiet_NaN();
    }
}
std::string Fit::expression() const {
    const auto& c = coef;
    switch (type) {
    case Reg::LinearAx: case Reg::MedMed: return num15(c[0]) + "x+" + num15(c[1]);
    case Reg::LinearA: return num15(c[0]) + "+" + num15(c[1]) + "x";
    case Reg::Quad: case Reg::Cubic: case Reg::Quart: {
        std::string s; const int deg = static_cast<int>(ncoef) - 1;
        for (int i = 0; i <= deg; ++i) { if (i) s += "+"; s += num15(c[static_cast<std::size_t>(i)]); if (deg - i >= 1) s += "x"; if (deg - i >= 2) s += "^" + std::to_string(deg - i); }
        return s;
    }
    case Reg::Log: return num15(c[0]) + "+" + num15(c[1]) + "ln(x)";
    case Reg::ExpE: return num15(c[0]) + "exp(" + num15(c[1]) + "x)";
    case Reg::ExpAb: return num15(c[0]) + "*" + num15(c[1]) + "^x";
    case Reg::Power: return num15(c[0]) + "x^" + num15(c[1]);
    // Q5: la sinusoidale e' in radianti qualunque sia Angle (suffisso ʳ).
    case Reg::Sin: return num15(c[0]) + "sin((" + num15(c[1]) + "x+" + num15(c[2]) + ")\xCA\xB3)+" + num15(c[3]);
    case Reg::Logistic: return num15(c[2]) + "/(1+" + num15(c[0]) + "exp(-" + num15(c[1]) + "x))";
    default: return "0";
    }
}
std::vector<std::pair<std::string, double>> fitRows(const Fit& f) {
    static const char* const names[] = {"a", "b", "c", "d", "e"};
    std::vector<std::pair<std::string, double>> rows;
    for (std::size_t i = 0; i < f.ncoef; ++i) rows.emplace_back(names[i], f.coef[i]);
    if (f.type == Reg::MedMed) return rows;                  // Med-Med: solo a e b
    if (f.hasR) rows.emplace_back("r", f.r);
    if (f.hasR2) rows.emplace_back(f.hasR ? "r\xC2\xB2" : "R\xC2\xB2", f.r2);
    rows.emplace_back("MSe", f.mse);
    return rows;
}
std::vector<double> residuals(const Fit& f, const std::vector<double>& x, const std::vector<double>& y) {
    needDim(x.size() == y.size(), "Dimension ERROR: liste X e Y di lunghezza diversa");
    std::vector<double> out(x.size());
    for (std::size_t i = 0; i < x.size(); ++i) out[i] = y[i] - f.predict(x[i]);
    return out;
}
std::vector<double> estimateX(const Fit& f, double y) {
    const auto& c = f.coef;
    auto one = [](double v) { need(std::isfinite(v), "Math ERROR: x\xCC\x82 non esiste"); return std::vector<double>{v}; };
    switch (f.type) {
    case Reg::LinearAx: case Reg::MedMed: need(c[0] != 0, "Math ERROR: a = 0"); return one((y - c[1]) / c[0]);
    case Reg::LinearA: need(c[1] != 0, "Math ERROR: b = 0"); return one((y - c[0]) / c[1]);
    case Reg::Quad: case Reg::Cubic: case Reg::Quart: {
        // Radici reali di p(x) - y: bisezione tra i punti critici (derivate successive).
        std::vector<double> p(c.begin(), c.begin() + static_cast<long>(f.ncoef));
        p.back() -= y;
        while (!p.empty() && p.front() == 0) p.erase(p.begin());
        need(p.size() >= 2, "Math ERROR: x\xCC\x82 non esiste");
        auto eval = [](const std::vector<double>& q, double x) { double v = 0; for (double k : q) v = v * x + k; return v; };
        std::function<std::vector<double>(const std::vector<double>&)> roots = [&](const std::vector<double>& q) -> std::vector<double> {
            const std::size_t deg = q.size() - 1;
            if (deg == 1) return {-q[1] / q[0]};
            std::vector<double> d;
            for (std::size_t i = 0; i < deg; ++i) d.push_back(q[i] * static_cast<double>(deg - i));
            std::vector<double> crit = roots(d);
            double bound = 0; for (std::size_t i = 1; i < q.size(); ++i) bound = std::max(bound, std::abs(q[i] / q[0]));
            bound += 1;
            std::vector<double> pts = {-bound};
            for (double r : crit) if (r > -bound && r < bound) pts.push_back(r);
            pts.push_back(bound);
            std::sort(pts.begin(), pts.end());
            std::vector<double> out;
            for (std::size_t i = 0; i + 1 < pts.size(); ++i) {
                double lo = pts[i], hi = pts[i + 1], flo = eval(q, lo), fhi = eval(q, hi);
                if (flo == 0) { out.push_back(lo); continue; }
                if ((flo < 0) == (fhi < 0)) continue;
                for (int k = 0; k < 200; ++k) { const double m = 0.5 * (lo + hi), fm = eval(q, m); if ((fm < 0) == (flo < 0)) { lo = m; flo = fm; } else hi = m; }
                out.push_back(0.5 * (lo + hi));
            }
            if (eval(q, pts.back()) == 0) out.push_back(pts.back());
            std::sort(out.begin(), out.end());
            out.erase(std::unique(out.begin(), out.end(), [](double a, double b) { return std::abs(a - b) <= 1e-12 * std::max(1.0, std::abs(a)); }), out.end());
            return out;
        };
        std::vector<double> r = roots(p);
        need(!r.empty(), "Math ERROR: x\xCC\x82 non esiste");
        return r;
    }
    case Reg::Log: need(c[1] != 0, "Math ERROR: b = 0"); return one(std::exp((y - c[0]) / c[1]));
    case Reg::ExpE: need(c[0] != 0 && c[1] != 0 && y / c[0] > 0, "Math ERROR: x\xCC\x82 non esiste"); return one(std::log(y / c[0]) / c[1]);
    case Reg::ExpAb: need(c[0] != 0 && c[1] > 0 && c[1] != 1 && y / c[0] > 0, "Math ERROR: x\xCC\x82 non esiste"); return one(std::log(y / c[0]) / std::log(c[1]));
    case Reg::Power: need(c[0] != 0 && c[1] != 0 && y / c[0] > 0, "Math ERROR: x\xCC\x82 non esiste"); return one(std::pow(y / c[0], 1 / c[1]));
    case Reg::Logistic: {
        need(y != 0 && c[0] != 0 && c[1] != 0, "Math ERROR: x\xCC\x82 non esiste");
        const double t = (c[2] / y - 1) / c[0];
        need(t > 0, "Math ERROR: x\xCC\x82 non esiste");
        return one(-std::log(t) / c[1]);
    }
    default: throw MathError("Math ERROR: x\xCC\x82 non disponibile per SinReg");
    }
}

// FitTask: stadi 0 preparazione, 1 stima iniziale (Sin: griglia di frequenze;
// Logistic: avvii con c diversi), 2 Levenberg-Marquardt, 3 fine.
FitTask::FitTask(Reg type, std::vector<double> x, std::vector<double> y, std::vector<double> freq)
    : type_(type), x_(std::move(x)), y_(std::move(y)), w_(std::move(freq)) {}

void FitTask::prepare() {
    needDim(x_.size() == y_.size(), "Dimension ERROR: liste X e Y di lunghezza diversa");
    needDim(!x_.empty(), "Dimension ERROR: lista vuota");
    finiteAll(x_); finiteAll(y_);
    if (w_.empty()) w_.assign(x_.size(), 1.0);
    checkFrequencies(x_.size(), &w_);
    double W = 0; for (double w : w_) W += w;
    needDim(W > 0, "Dimension ERROR: frequenze tutte nulle");
    fit_ = Fit{};
    fit_.type = type_;
    fit_.ncoef = regCoefficientCount(type_);
    fit_.n = W;
    if (type_ == Reg::Sin) {
        std::size_t pts = 0; for (double w : w_) if (w > 0) ++pts;
        needDim(pts >= 4, "Dimension ERROR: SinReg richiede almeno 4 dati");
        std::vector<double> xs;
        for (std::size_t i = 0; i < x_.size(); ++i) if (w_[i] > 0) xs.push_back(x_[i]);
        std::sort(xs.begin(), xs.end());
        xs.erase(std::unique(xs.begin(), xs.end()), xs.end());
        needDim(xs.size() >= 4, "Dimension ERROR: SinReg richiede almeno 4 valori X diversi");
        std::vector<double> gaps;
        for (std::size_t i = 1; i < xs.size(); ++i) gaps.push_back(xs[i] - xs[i - 1]);
        std::nth_element(gaps.begin(), gaps.begin() + static_cast<long>(gaps.size() / 2), gaps.end());
        const double span = xs.back() - xs.front(), dx = gaps[gaps.size() / 2];
        const double bLo = kPi / (2 * span), bHi = kPi / dx;
        constexpr std::size_t K = 480;
        grid_.clear();
        for (std::size_t k = 0; k < K; ++k) grid_.push_back(bLo * std::pow(bHi / bLo, static_cast<double>(k) / (K - 1)));
        total_ = K + 400;
    } else if (type_ == Reg::Logistic) {
        std::size_t pts = 0; for (double w : w_) if (w > 0) ++pts;
        needDim(pts >= 3, "Dimension ERROR: LogisticReg richiede almeno 3 dati");
        double ymax = -kInf; for (std::size_t i = 0; i < y_.size(); ++i) if (w_[i] > 0) ymax = std::max(ymax, y_[i]);
        need(ymax > 0, "Math ERROR: LogisticReg richiede dati Y positivi");
        grid_ = {1.0001, 1.01, 1.05, 1.15, 1.4, 2.0, 3.5};
        for (double& g : grid_) g *= ymax;
        total_ = grid_.size() * 121;
    } else {
        total_ = 1;
    }
}

void FitTask::finishClosed() {
    Fit& f = fit_;
    auto& c = f.coef;
    const std::size_t m = x_.size();
    auto ssOf = [&](auto&& resid) { double s = 0; for (std::size_t i = 0; i < m; ++i) if (w_[i] > 0) s += w_[i] * sq(resid(i)); return s; };
    auto ssTot = [&]() {
        double W = 0, s = 0; for (std::size_t i = 0; i < m; ++i) { W += w_[i]; s += w_[i] * y_[i]; }
        const double my = s / W; double t = 0; for (std::size_t i = 0; i < m; ++i) t += w_[i] * sq(y_[i] - my); return t;
    };
    switch (type_) {
    case Reg::LinearAx: case Reg::LinearA: {
        const Line l = lineFit(x_, y_, w_);
        if (type_ == Reg::LinearAx) { c[0] = l.slope; c[1] = l.icept; } else { c[0] = l.icept; c[1] = l.slope; }
        f.r = l.r; f.r2 = l.r * l.r; f.hasR = f.hasR2 = true;
        f.mse = f.n > 2 ? ssOf([&](std::size_t i) { return y_[i] - f.predict(x_[i]); }) / (f.n - 2) : 0;
        break;
    }
    case Reg::MedMed: {
        needDim(f.n >= 3, "Dimension ERROR: Med-Med richiede almeno 3 dati");
        std::vector<std::pair<std::pair<double, double>, double>> p;    // ((x, y), peso)
        for (std::size_t i = 0; i < m; ++i) if (w_[i] > 0) p.push_back({{x_[i], y_[i]}, w_[i]});
        std::sort(p.begin(), p.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
        const double n = f.n, k = std::floor(n / 3), rem = n - 3 * k;
        const double g1 = k + (rem == 2 ? 1 : 0), g3 = g1, g2 = n - g1 - g3;
        // Mediane pesate di x e y dei dati ripetuti con indice in [b, e).
        auto groupMedians = [&](double b, double e, double& mx, double& my) {
            std::vector<std::pair<double, double>> xs, ys;
            double pos = 0;
            for (const auto& q : p) {
                const double lo = std::max(pos, b), hi = std::min(pos + q.second, e);
                if (hi > lo) { xs.emplace_back(q.first.first, hi - lo); ys.emplace_back(q.first.second, hi - lo); }
                pos += q.second;
            }
            auto med = [](std::vector<std::pair<double, double>> v) {
                std::stable_sort(v.begin(), v.end(), [](const auto& a, const auto& b2) { return a.first < b2.first; });
                Weighted w; w.d = std::move(v); for (const auto& t : w.d) w.n += t.second;
                return medianRange(w, 0, w.n);
            };
            mx = med(std::move(xs)); my = med(std::move(ys));
        };
        double x1, y1, x2, y2, x3, y3;
        groupMedians(0, g1, x1, y1);
        groupMedians(g1, g1 + g2, x2, y2);
        groupMedians(n - g3, n, x3, y3);
        need(x3 != x1, "Math ERROR: gruppi Med-Med degeneri");
        c[0] = (y3 - y1) / (x3 - x1);
        c[1] = ((y1 - c[0] * x1) + (y2 - c[0] * x2) + (y3 - c[0] * x3)) / 3.0;
        f.pts = {x1, y1, x2, y2, x3, y3}; f.hasPts = true;
        f.mse = f.n > 2 ? ssOf([&](std::size_t i) { return y_[i] - f.predict(x_[i]); }) / (f.n - 2) : 0;
        break;
    }
    case Reg::Quad: case Reg::Cubic: case Reg::Quart: {
        const unsigned deg = static_cast<unsigned>(f.ncoef - 1);
        const std::vector<double> cf = polyFit(x_, y_, w_, deg);
        for (std::size_t i = 0; i < cf.size(); ++i) c[i] = cf[i];
        const double ss = ssOf([&](std::size_t i) { return y_[i] - f.predict(x_[i]); }), st = ssTot();
        f.r2 = st > 0 ? 1 - ss / st : 1.0; f.hasR2 = true;
        f.mse = f.n > deg + 1 ? ss / (f.n - deg - 1) : 0;
        break;
    }
    case Reg::Log: case Reg::ExpE: case Reg::ExpAb: case Reg::Power: {
        std::vector<double> u(m), v(m);
        for (std::size_t i = 0; i < m; ++i) {
            const bool lx = type_ == Reg::Log || type_ == Reg::Power, ly = type_ != Reg::Log;
            if (w_[i] > 0 && lx) need(x_[i] > 0, type_ == Reg::Log ? "Math ERROR: LogReg richiede X>0" : "Math ERROR: PowerReg richiede X>0 e Y>0");
            if (w_[i] > 0 && ly) need(y_[i] > 0, type_ == Reg::Power ? "Math ERROR: PowerReg richiede X>0 e Y>0" : "Math ERROR: ExpReg richiede Y>0");
            u[i] = lx ? (x_[i] > 0 ? std::log(x_[i]) : 0) : x_[i];
            v[i] = ly ? (y_[i] > 0 ? std::log(y_[i]) : 0) : y_[i];
        }
        const Line l = lineFit(u, v, w_);
        if (type_ == Reg::Log) { c[0] = l.icept; c[1] = l.slope; }
        else { c[0] = std::exp(l.icept); c[1] = type_ == Reg::ExpAb ? std::exp(l.slope) : l.slope; }
        f.r = l.r; f.r2 = l.r * l.r; f.hasR = f.hasR2 = true;
        // MSe sulla scala trasformata (y per Log, ln y per Exp e Power).
        const double ss = ssOf([&](std::size_t i) { return v[i] - (l.icept + l.slope * u[i]); });
        f.mse = f.n > 2 ? ss / (f.n - 2) : 0;
        break;
    }
    default: break;
    }
    for (std::size_t i = 0; i < f.ncoef; ++i) need(std::isfinite(c[i]), "Math ERROR: coefficienti non finiti");
    for (double v : {f.r, f.r2, f.mse}) (void)finiteResult(v);
}

bool FitTask::step(step::Budget& budget) {
    if (stage_ == 0) { prepare(); stage_ = 1; cand_ = 0; bestSs_ = kInf; }
    if (type_ != Reg::Sin && type_ != Reg::Logistic) { finishClosed(); done_ = total_; stage_ = 3; return true; }
    auto model = type_ == Reg::Sin ? &sinModel : &logisticModel;
    const std::size_t np = type_ == Reg::Sin ? 4 : 3;
    auto ss = [&](const std::array<double, 4>& p) {
        double s = 0;
        for (std::size_t i = 0; i < x_.size(); ++i) if (w_[i] > 0) s += w_[i] * sq(y_[i] - model(p, x_[i], nullptr));
        return std::isfinite(s) ? s : kInf;
    };
    // Un passo di Levenberg-Marquardt da p_ (true quando converge o finisce le iterazioni).
    auto lmStep = [&]() -> bool {
        std::vector<double> JtJ(np * np, 0), g(np, 0);
        double gr[4];
        const double s0 = ss(p_);
        for (std::size_t i = 0; i < x_.size(); ++i) {
            if (w_[i] <= 0) continue;
            const double r = y_[i] - model(p_, x_[i], gr);
            for (std::size_t a = 0; a < np; ++a) { g[a] += w_[i] * gr[a] * r; for (std::size_t b = 0; b < np; ++b) JtJ[a * np + b] += w_[i] * gr[a] * gr[b]; }
        }
        for (int tries = 0; tries < 12; ++tries) {
            std::vector<double> M = JtJ, d;
            for (std::size_t a = 0; a < np; ++a) M[a * np + a] += lambda_ * (JtJ[a * np + a] > 0 ? JtJ[a * np + a] : 1.0);
            if (!solveSmall(M, g, np, d)) { lambda_ *= 10; continue; }
            std::array<double, 4> q = p_;
            for (std::size_t a = 0; a < np; ++a) q[a] += d[a];
            const double s1 = ss(q);
            if (s1 <= s0) {
                double rel = 0; for (std::size_t a = 0; a < np; ++a) rel = std::max(rel, std::abs(d[a]) / (std::abs(p_[a]) + 1e-12));
                p_ = q; lambda_ = std::max(lambda_ / 3, 1e-12);
                return (s0 - s1) <= 1e-15 * s0 || rel < 1e-14 || s1 == 0;
            }
            lambda_ *= 4;
            if (lambda_ > 1e16) return true;
        }
        return true;
    };
    while (stage_ < 3) {
        if (stage_ == 1 && type_ == Reg::Sin) {
            // Griglia di b: minimi quadrati lineari di y = A sin(bx) + B cos(bx) + D.
            while (cand_ < grid_.size()) {
                const double b = grid_[cand_++];
                double M[9] = {0}, rhs[3] = {0};
                for (std::size_t i = 0; i < x_.size(); ++i) {
                    if (w_[i] <= 0) continue;
                    const double v[3] = {std::sin(b * x_[i]), std::cos(b * x_[i]), 1};
                    for (int a = 0; a < 3; ++a) { rhs[a] += w_[i] * v[a] * y_[i]; for (int q = 0; q < 3; ++q) M[a * 3 + q] += w_[i] * v[a] * v[q]; }
                }
                std::vector<double> sol;
                if (solveSmall(std::vector<double>(M, M + 9), std::vector<double>(rhs, rhs + 3), 3, sol)) {
                    const std::array<double, 4> p = {std::hypot(sol[0], sol[1]), b, std::atan2(sol[1], sol[0]), sol[2]};
                    const double s = ss(p);
                    if (s < bestSs_) { bestSs_ = s; best_ = p; }
                }
                ++done_;
                if (!budget.consume()) return false;
            }
            need(std::isfinite(bestSs_), "Math ERROR: SinReg senza soluzione");
            p_ = best_; lambda_ = 1e-3; iter_ = 0; stage_ = 2;
        } else if (stage_ == 1) {
            // Logistic: per ogni c di partenza, a e b dalla retta di ln(c/y - 1) = ln a - b x, poi LM.
            if (cand_ >= grid_.size()) { need(std::isfinite(bestSs_), "Math ERROR: LogisticReg senza soluzione"); p_ = best_; stage_ = 3; break; }
            const double c0 = grid_[cand_];
            std::vector<double> u, v, w;
            for (std::size_t i = 0; i < x_.size(); ++i) if (w_[i] > 0 && y_[i] > 0 && y_[i] < c0) { u.push_back(x_[i]); v.push_back(std::log(c0 / y_[i] - 1)); w.push_back(w_[i]); }
            p_ = {1, 1, c0, 0};
            try { if (u.size() >= 2) { const Line l = lineFit(u, v, w); p_ = {std::exp(l.icept), -l.slope, c0, 0}; } } catch (const MathError&) {}
            lambda_ = 1e-3; iter_ = 0; stage_ = 2;
        } else if (stage_ == 2) {
            const bool end = lmStep() || ++iter_ >= (type_ == Reg::Sin ? 400 : 120);
            ++done_;
            if (end) {
                if (type_ == Reg::Sin) { best_ = p_; stage_ = 3; }
                else {
                    const double s = ss(p_);
                    if (s < bestSs_) { bestSs_ = s; best_ = p_; }
                    ++cand_; stage_ = 1;
                }
            }
            if (stage_ != 3 && !budget.consume()) return false;
        }
    }
    // Fine: normalizzazione (Sin: a > 0, b > 0, -π < c <= π), MSe /(n-2).
    std::array<double, 4> p = best_;
    if (type_ == Reg::Sin) {
        if (p[1] < 0) { p[1] = -p[1]; p[2] = kPi - p[2]; }
        if (p[0] < 0) { p[0] = -p[0]; p[2] += kPi; }
        p[2] = wrapAngle(p[2]);
    }
    for (std::size_t i = 0; i < np; ++i) { need(std::isfinite(p[i]), "Math ERROR: coefficienti non finiti"); fit_.coef[i] = p[i]; }
    fit_.mse = finiteResult(fit_.n > 2 ? ss(p) / (fit_.n - 2) : 0);
    done_ = total_;
    return true;
}

Fit fit(Reg type, const std::vector<double>& x, const std::vector<double>& y, const std::vector<double>* freq) {
    FitTask t(type, x, y, freq ? *freq : std::vector<double>{});
    step::Budget unlimited;
    while (!t.step(unlimited)) {}
    return t.result();
}

// ---------------------------------------------------------------- TEST e INTR
namespace {
std::string g10(double v) { char b[32]; std::snprintf(b, sizeof b, "%.10g", v); return b; }
// Titolo con l'ipotesi alternativa: "μ≠0", "p1<p2"...
std::string alternative(const std::string& lhs, int tail, const std::string& rhs) {
    return lhs + (tail < 0 ? "<" : tail > 0 ? ">" : "\xE2\x89\xA0") + rhs;
}
struct Summary { double mean{0}, sd{0}, n{0}; bool list{false}; };
Summary summarize(const Sample& s, bool needSd) {
    Summary r; r.list = s.list;
    if (s.list) {
        const OneVar o = oneVarEx(s.x, s.f.empty() ? nullptr : &s.f, QuartileRule::Std);
        r.mean = o.mean; r.sd = o.sampleSd; r.n = o.n;
    } else {
        need(std::isfinite(s.mean) && std::isfinite(s.sd) && std::isfinite(s.n), "Math ERROR: dato non finito");
        need(s.n >= 1 && s.n == std::floor(s.n), "Argument ERROR: n intero >= 1");
        if (needSd) need(s.sd > 0, "Argument ERROR: sx > 0");
        r.mean = s.mean; r.sd = s.sd; r.n = s.n;
    }
    return r;
}
void addRow(Outcome& o, const char* label, double v, int var = -1) {
    (void)finiteResult(v);
    o.rows.push_back({label, v});
    if (var >= 0) o.vars.emplace_back(static_cast<std::uint8_t>(var), v);
}
void needProportion(double x, double n) {
    need(std::isfinite(x) && std::isfinite(n) && n >= 1 && n == std::floor(n), "Argument ERROR: n intero >= 1");
    need(x >= 0 && x <= n && x == std::floor(x), "Argument ERROR: x intero tra 0 e n");
}
} // namespace

Outcome zTest1S(const Sample& s, double mu0, double sigma, int tail) {
    need(sigma > 0 && std::isfinite(sigma) && std::isfinite(mu0), "Argument ERROR: \xCF\x83>0");
    const Summary m = summarize(s, false);
    Outcome o; o.title = alternative("\xCE\xBC", tail, g10(mu0));
    const double z = (m.mean - mu0) / (sigma / std::sqrt(m.n));
    addRow(o, "z", z, VZ); addRow(o, "p", pValue(normalLower(z), normalUpper(z), tail), VP);
    addRow(o, "\xEE\x80\x81", m.mean, VInMean);
    if (m.list) addRow(o, "sx", m.sd, VInSx);
    addRow(o, "n", m.n, VInN);
    o.curve = Curve::Normal; o.stat = z; o.tail = tail;
    return o;
}
Outcome zTest2S(const Sample& a, const Sample& b, double s1, double s2, int tail) {
    need(s1 > 0 && s2 > 0 && std::isfinite(s1) && std::isfinite(s2), "Argument ERROR: \xCF\x83>0");
    const Summary m1 = summarize(a, false), m2 = summarize(b, false);
    Outcome o; o.title = alternative("\xCE\xBC" "1", tail, "\xCE\xBC" "2");
    const double z = (m1.mean - m2.mean) / std::sqrt(s1 * s1 / m1.n + s2 * s2 / m2.n);
    addRow(o, "z", z, VZ); addRow(o, "p", pValue(normalLower(z), normalUpper(z), tail), VP);
    addRow(o, "\xEE\x80\x81" "1", m1.mean, VInMean1); addRow(o, "\xEE\x80\x81" "2", m2.mean, VInMean2);
    if (m1.list) addRow(o, "sx1", m1.sd, VInSx1);
    if (m2.list) addRow(o, "sx2", m2.sd, VInSx2);
    addRow(o, "n1", m1.n, VInN1); addRow(o, "n2", m2.n, VInN2);
    o.curve = Curve::Normal; o.stat = z; o.tail = tail;
    return o;
}
Outcome zTest1P(double p0, double x, double n, int tail) {
    need(p0 > 0 && p0 < 1, "Argument ERROR: 0<p0<1");
    needProportion(x, n);
    Outcome o; o.title = alternative("Prop", tail, g10(p0));
    const double ph = x / n, z = (ph - p0) / std::sqrt(p0 * (1 - p0) / n);
    addRow(o, "z", z, VZ); addRow(o, "p", pValue(normalLower(z), normalUpper(z), tail), VP);
    addRow(o, "p\xCC\x82", ph, VPHat); addRow(o, "n", n, VInN);
    o.curve = Curve::Normal; o.stat = z; o.tail = tail;
    return o;
}
Outcome zTest2P(double x1, double n1, double x2, double n2, int tail) {
    needProportion(x1, n1); needProportion(x2, n2);
    const double p1 = x1 / n1, p2 = x2 / n2, pp = (x1 + x2) / (n1 + n2);
    need(pp > 0 && pp < 1, "Math ERROR: p\xCC\x82 = 0 o 1");
    Outcome o; o.title = alternative("p1", tail, "p2");
    const double z = (p1 - p2) / std::sqrt(pp * (1 - pp) * (1 / n1 + 1 / n2));
    addRow(o, "z", z, VZ); addRow(o, "p", pValue(normalLower(z), normalUpper(z), tail), VP);
    addRow(o, "p\xCC\x82" "1", p1, VPHat1); addRow(o, "p\xCC\x82" "2", p2, VPHat2); addRow(o, "p\xCC\x82", pp, VPHat);
    addRow(o, "n1", n1, VInN1); addRow(o, "n2", n2, VInN2);
    o.curve = Curve::Normal; o.stat = z; o.tail = tail;
    return o;
}
Outcome tTest1S(const Sample& s, double mu0, int tail) {
    need(std::isfinite(mu0), "Math ERROR: dato non finito");
    const Summary m = summarize(s, true);
    needDim(m.n >= 2, "Dimension ERROR: servono almeno 2 dati");
    need(m.sd > 0, "Math ERROR: sx = 0");
    Outcome o; o.title = alternative("\xCE\xBC", tail, g10(mu0));
    const double df = m.n - 1, t = (m.mean - mu0) / (m.sd / std::sqrt(m.n));
    addRow(o, "t", t, VT); addRow(o, "p", pValue(tLowerTail(t, df), tUpperTail(t, df), tail), VP);
    addRow(o, "\xEE\x80\x81", m.mean, VInMean); addRow(o, "sx", m.sd, VInSx); addRow(o, "n", m.n, VInN);
    o.vars.emplace_back(VDf, df);
    o.curve = Curve::T; o.df1 = df; o.stat = t; o.tail = tail;
    return o;
}
namespace {
struct TwoT { double t, df, sp, se; };
TwoT twoSampleT(const Summary& a, const Summary& b, bool pooled) {
    needDim(a.n >= 2 && b.n >= 2, "Dimension ERROR: servono almeno 2 dati per campione");
    need(a.sd > 0 || b.sd > 0, "Math ERROR: sx = 0");
    TwoT r{};
    if (pooled) {
        r.df = a.n + b.n - 2;
        r.sp = std::sqrt(((a.n - 1) * a.sd * a.sd + (b.n - 1) * b.sd * b.sd) / r.df);
        r.se = r.sp * std::sqrt(1 / a.n + 1 / b.n);
    } else {
        const double va = a.sd * a.sd / a.n, vb = b.sd * b.sd / b.n;
        r.se = std::sqrt(va + vb);
        r.df = sq(va + vb) / (va * va / (a.n - 1) + vb * vb / (b.n - 1));
    }
    need(r.se > 0, "Math ERROR: sx = 0");
    r.t = (a.mean - b.mean) / r.se;
    return r;
}
} // namespace
Outcome tTest2S(const Sample& a, const Sample& b, bool pooled, int tail) {
    const Summary m1 = summarize(a, true), m2 = summarize(b, true);
    const TwoT r = twoSampleT(m1, m2, pooled);
    Outcome o; o.title = alternative("\xCE\xBC" "1", tail, "\xCE\xBC" "2");
    addRow(o, "t", r.t, VT); addRow(o, "p", pValue(tLowerTail(r.t, r.df), tUpperTail(r.t, r.df), tail), VP); addRow(o, "df", r.df, VDf);
    addRow(o, "\xEE\x80\x81" "1", m1.mean, VInMean1); addRow(o, "\xEE\x80\x81" "2", m2.mean, VInMean2);
    addRow(o, "sx1", m1.sd, VInSx1); addRow(o, "sx2", m2.sd, VInSx2);
    if (pooled) addRow(o, "sp", r.sp, VInSp);
    addRow(o, "n1", m1.n, VInN1); addRow(o, "n2", m2.n, VInN2);
    o.curve = Curve::T; o.df1 = r.df; o.stat = r.t; o.tail = tail;
    return o;
}
Outcome tTestReg(const std::vector<double>& x, const std::vector<double>& y, const std::vector<double>* freq, int tail) {
    const Fit f = fit(Reg::LinearA, x, y, freq);       // y = a + b·x
    needDim(f.n >= 3, "Dimension ERROR: servono almeno 3 dati");
    const double df = f.n - 2, r = f.r;
    need(std::abs(r) < 1, "Math ERROR: r = \xC2\xB1" "1");
    const double t = r * std::sqrt(df / (1 - r * r));
    Outcome o; o.title = alternative("\xCE\xB2 & \xCF\x81", tail, "0");
    addRow(o, "t", t, VT); addRow(o, "p", pValue(tLowerTail(t, df), tUpperTail(t, df), tail), VP); addRow(o, "df", df, VDf);
    addRow(o, "a", f.coef[0]); addRow(o, "b", f.coef[1]);
    addRow(o, "s", std::sqrt(f.mse), VSe); addRow(o, "r", r, VR); addRow(o, "r\xC2\xB2", r * r, VR2);
    o.curve = Curve::T; o.df1 = df; o.stat = t; o.tail = tail;
    return o;
}
Outcome chiGof(const std::vector<double>& obs, const std::vector<double>& exp, double df) {
    needDim(!obs.empty() && obs.size() == exp.size(), "Dimension ERROR: Observed ed Expected di lunghezza diversa o vuote");
    finiteAll(obs); finiteAll(exp);
    need(df >= 1 && df == std::floor(df), "Argument ERROR: df intero >= 1");
    Outcome o; o.title = "\xCF\x87\xC2\xB2 GOF";
    double chi = 0;
    for (std::size_t i = 0; i < obs.size(); ++i) {
        need(obs[i] >= 0, "Argument ERROR: Observed >= 0");
        need(exp[i] > 0, "Argument ERROR: Expected > 0");
        const double c = sq(obs[i] - exp[i]) / exp[i];
        o.listOut.push_back(c); chi += c;
    }
    addRow(o, "\xCF\x87\xC2\xB2", chi, VChi); addRow(o, "p", chiUpperTail(chi, df), VP); addRow(o, "df", df, VDf);
    o.curve = Curve::Chi; o.df1 = df; o.stat = chi; o.tail = 1;
    return o;
}
Outcome chi2Way(const std::vector<double>& obs, std::size_t rows, std::size_t cols, std::vector<double>& expected) {
    needDim(rows >= 2 && cols >= 2 && obs.size() == rows * cols, "Dimension ERROR: Observed almeno 2x2");
    finiteAll(obs);
    std::vector<double> rs(rows, 0), cs(cols, 0);
    double tot = 0;
    for (std::size_t r = 0; r < rows; ++r)
        for (std::size_t c = 0; c < cols; ++c) { const double v = obs[r * cols + c]; need(v >= 0, "Argument ERROR: Observed >= 0"); rs[r] += v; cs[c] += v; tot += v; }
    need(tot > 0, "Math ERROR: Observed tutta nulla");
    expected.assign(rows * cols, 0);
    double chi = 0;
    for (std::size_t r = 0; r < rows; ++r)
        for (std::size_t c = 0; c < cols; ++c) {
            const double e = rs[r] * cs[c] / tot;
            need(e > 0, "Math ERROR: riga o colonna nulla");
            expected[r * cols + c] = e;
            chi += sq(obs[r * cols + c] - e) / e;
        }
    const double df = static_cast<double>((rows - 1) * (cols - 1));
    Outcome o; o.title = "\xCF\x87\xC2\xB2 2-way";
    addRow(o, "\xCF\x87\xC2\xB2", chi, VChi); addRow(o, "p", chiUpperTail(chi, df), VP); addRow(o, "df", df, VDf);
    o.curve = Curve::Chi; o.df1 = df; o.stat = chi; o.tail = 1;
    return o;
}
Outcome fTest2S(const Sample& a, const Sample& b, int tail) {
    const Summary m1 = summarize(a, true), m2 = summarize(b, true);
    needDim(m1.n >= 2 && m2.n >= 2, "Dimension ERROR: servono almeno 2 dati per campione");
    need(m2.sd > 0, "Math ERROR: sx2 = 0");
    const double F = m1.sd * m1.sd / (m2.sd * m2.sd), d1 = m1.n - 1, d2 = m2.n - 1;
    Outcome o; o.title = alternative("\xCF\x83" "1", tail, "\xCF\x83" "2");
    addRow(o, "F", F, VF); addRow(o, "p", pValue(fLowerTail(F, d1, d2), fUpperTail(F, d1, d2), tail), VP);
    addRow(o, "sx1", m1.sd, VInSx1); addRow(o, "sx2", m2.sd, VInSx2);
    if (m1.list) addRow(o, "\xEE\x80\x81" "1", m1.mean, VInMean1);
    if (m2.list) addRow(o, "\xEE\x80\x81" "2", m2.mean, VInMean2);
    addRow(o, "n1", m1.n, VInN1); addRow(o, "n2", m2.n, VInN2);
    o.curve = Curve::F; o.df1 = d1; o.df2 = d2; o.stat = F; o.tail = tail;
    return o;
}
Outcome zInt1S(const Sample& s, double level, double sigma) {
    need(sigma > 0 && std::isfinite(sigma), "Argument ERROR: \xCF\x83>0");
    const Summary m = summarize(s, false);
    const double h = zCritical(level) * sigma / std::sqrt(m.n);
    Outcome o; o.title = "1-Sample ZInterval";
    addRow(o, "Left", m.mean - h, VLeft); addRow(o, "Right", m.mean + h, VRight);
    addRow(o, "\xEE\x80\x81", m.mean, VInMean);
    if (m.list) addRow(o, "sx", m.sd, VInSx);
    addRow(o, "n", m.n, VInN);
    return o;
}
Outcome zInt2S(const Sample& a, const Sample& b, double level, double s1, double s2) {
    need(s1 > 0 && s2 > 0 && std::isfinite(s1) && std::isfinite(s2), "Argument ERROR: \xCF\x83>0");
    const Summary m1 = summarize(a, false), m2 = summarize(b, false);
    const double d = m1.mean - m2.mean, h = zCritical(level) * std::sqrt(s1 * s1 / m1.n + s2 * s2 / m2.n);
    Outcome o; o.title = "2-Sample ZInterval";
    addRow(o, "Left", d - h, VLeft); addRow(o, "Right", d + h, VRight);
    addRow(o, "\xEE\x80\x81" "1", m1.mean, VInMean1); addRow(o, "\xEE\x80\x81" "2", m2.mean, VInMean2);
    if (m1.list) addRow(o, "sx1", m1.sd, VInSx1);
    if (m2.list) addRow(o, "sx2", m2.sd, VInSx2);
    addRow(o, "n1", m1.n, VInN1); addRow(o, "n2", m2.n, VInN2);
    return o;
}
Outcome zInt1P(double level, double x, double n) {
    needProportion(x, n);
    const double ph = x / n, h = zCritical(level) * std::sqrt(ph * (1 - ph) / n);
    Outcome o; o.title = "1-Prop ZInterval";
    addRow(o, "Left", ph - h, VLeft); addRow(o, "Right", ph + h, VRight); addRow(o, "p\xCC\x82", ph, VIPHat); addRow(o, "n", n, VInN);
    return o;
}
Outcome zInt2P(double level, double x1, double n1, double x2, double n2) {
    needProportion(x1, n1); needProportion(x2, n2);
    const double p1 = x1 / n1, p2 = x2 / n2, h = zCritical(level) * std::sqrt(p1 * (1 - p1) / n1 + p2 * (1 - p2) / n2);
    Outcome o; o.title = "2-Prop ZInterval";
    addRow(o, "Left", p1 - p2 - h, VLeft); addRow(o, "Right", p1 - p2 + h, VRight);
    addRow(o, "p\xCC\x82" "1", p1, VIPHat1); addRow(o, "p\xCC\x82" "2", p2, VIPHat2); addRow(o, "n1", n1, VInN1); addRow(o, "n2", n2, VInN2);
    return o;
}
Outcome tInt1S(const Sample& s, double level) {
    const Summary m = summarize(s, true);
    needDim(m.n >= 2, "Dimension ERROR: servono almeno 2 dati");
    const double h = tCritical(level, m.n - 1) * m.sd / std::sqrt(m.n);
    Outcome o; o.title = "1-Sample tInterval";
    addRow(o, "Left", m.mean - h, VLeft); addRow(o, "Right", m.mean + h, VRight);
    addRow(o, "\xEE\x80\x81", m.mean, VInMean); addRow(o, "sx", m.sd, VInSx); addRow(o, "n", m.n, VInN);
    return o;
}
Outcome tInt2S(const Sample& a, const Sample& b, double level, bool pooled) {
    const Summary m1 = summarize(a, true), m2 = summarize(b, true);
    const TwoT r = twoSampleT(m1, m2, pooled);
    const double d = m1.mean - m2.mean, h = tCritical(level, r.df) * r.se;
    Outcome o; o.title = "2-Sample tInterval";
    addRow(o, "Left", d - h, VLeft); addRow(o, "Right", d + h, VRight); addRow(o, "df", r.df, VIDf);
    addRow(o, "\xEE\x80\x81" "1", m1.mean, VInMean1); addRow(o, "\xEE\x80\x81" "2", m2.mean, VInMean2);
    addRow(o, "sx1", m1.sd, VInSx1); addRow(o, "sx2", m2.sd, VInSx2);
    if (pooled) addRow(o, "sp", r.sp, VInSp);
    addRow(o, "n1", m1.n, VInN1); addRow(o, "n2", m2.n, VInN2);
    return o;
}

// ---------------------------------------------------------------- ANOVA
namespace {
std::vector<double> levelsOf(const std::vector<double>& v) {
    std::vector<double> l = v;
    std::sort(l.begin(), l.end());
    l.erase(std::unique(l.begin(), l.end()), l.end());
    return l;
}
std::size_t levelIndex(const std::vector<double>& levels, double v) {
    return static_cast<std::size_t>(std::lower_bound(levels.begin(), levels.end(), v) - levels.begin());
}
} // namespace
AnovaTask::AnovaTask(std::vector<double> a, std::vector<double> b, std::vector<double> y) : a_(std::move(a)), b_(std::move(b)), y_(std::move(y)) {
    needDim(!y_.empty() && a_.size() == y_.size() && (b_.empty() || b_.size() == y_.size()),
            "Dimension ERROR: fattori e Dependent di lunghezza diversa o vuoti");
    finiteAll(a_); finiteAll(b_); finiteAll(y_);
    table_.twoWay = !b_.empty();
    table_.levelsA = levelsOf(a_);
    if (table_.twoWay) table_.levelsB = levelsOf(b_);
    needDim(table_.levelsA.size() >= 2 && (!table_.twoWay || table_.levelsB.size() >= 2), "Dimension ERROR: servono almeno 2 livelli per fattore");
    const std::size_t cells = table_.levelsA.size() * (table_.twoWay ? table_.levelsB.size() : 1);
    sum_.assign(cells, 0); cnt_.assign(cells, 0);
}
bool AnovaTask::step(step::Budget& budget) {
    const std::size_t nb = table_.twoWay ? table_.levelsB.size() : 1;
    while (next_ < y_.size()) {
        const std::size_t ia = levelIndex(table_.levelsA, a_[next_]);
        const std::size_t ib = table_.twoWay ? levelIndex(table_.levelsB, b_[next_]) : 0;
        sum_[ia * nb + ib] += y_[next_]; cnt_[ia * nb + ib] += 1;
        total_ += y_[next_];
        ++next_;
        if (!budget.consume()) return false;
    }
    finish();
    ++next_;
    return true;
}
void AnovaTask::finish() {
    AnovaTable& t = table_;
    const std::size_t na = t.levelsA.size(), nb = t.twoWay ? t.levelsB.size() : 1;
    const double N = static_cast<double>(y_.size()), grand = total_ / N;
    std::vector<double> cellMean(na * nb, 0);
    for (std::size_t k = 0; k < na * nb; ++k) cellMean[k] = cnt_[k] > 0 ? sum_[k] / cnt_[k] : 0;
    double sse = 0;
    for (std::size_t i = 0; i < y_.size(); ++i) {
        const std::size_t ia = levelIndex(t.levelsA, a_[i]), ib = t.twoWay ? levelIndex(t.levelsB, b_[i]) : 0;
        sse += sq(y_[i] - cellMean[ia * nb + ib]);
    }
    if (!t.twoWay) {
        double ssa = 0;
        for (std::size_t k = 0; k < na; ++k) ssa += cnt_[k] * sq(cellMean[k] - grand);
        t.dfA = static_cast<double>(na - 1); t.dfE = N - static_cast<double>(na);
        needDim(t.dfE > 0, "Dimension ERROR: dati insufficienti per l'errore");
        t.ssA = ssa; t.ssE = sse;
    } else {
        // Q6: disegno bilanciato (stesso numero di repliche r in ogni cella).
        const double r = cnt_[0];
        for (double c : cnt_) needDim(c == r && c > 0, "Dimension ERROR: disegno non bilanciato (ANOVA a due vie)");
        std::vector<double> ma(na, 0), mb(nb, 0);
        for (std::size_t i = 0; i < na; ++i) for (std::size_t j = 0; j < nb; ++j) { ma[i] += cellMean[i * nb + j] / static_cast<double>(nb); mb[j] += cellMean[i * nb + j] / static_cast<double>(na); }
        double ssa = 0, ssb = 0, ssab = 0;
        for (std::size_t i = 0; i < na; ++i) ssa += sq(ma[i] - grand);
        for (std::size_t j = 0; j < nb; ++j) ssb += sq(mb[j] - grand);
        for (std::size_t i = 0; i < na; ++i) for (std::size_t j = 0; j < nb; ++j) ssab += sq(cellMean[i * nb + j] - ma[i] - mb[j] + grand);
        t.ssA = ssa * static_cast<double>(nb) * r; t.ssB = ssb * static_cast<double>(na) * r; ssab *= r;
        t.dfA = static_cast<double>(na - 1); t.dfB = static_cast<double>(nb - 1);
        t.interaction = r >= 2;
        if (t.interaction) {
            t.dfAB = t.dfA * t.dfB; t.ssAB = ssab;
            t.dfE = static_cast<double>(na * nb) * (r - 1); t.ssE = sse;
        } else {
            t.dfE = t.dfA * t.dfB; t.ssE = ssab;              // una replica: l'interazione fa da errore
        }
        t.cellMeans = cellMean;
    }
    t.msE = t.ssE / t.dfE;
    need(t.msE > 0, "Math ERROR: MS dell'errore nullo");
    t.msA = t.ssA / t.dfA; t.fA = t.msA / t.msE; t.pA = fUpperTail(t.fA, t.dfA, t.dfE);
    if (t.twoWay) {
        t.msB = t.ssB / t.dfB; t.fB = t.msB / t.msE; t.pB = fUpperTail(t.fB, t.dfB, t.dfE);
        if (t.interaction) { t.msAB = t.ssAB / t.dfAB; t.fAB = t.msAB / t.msE; t.pAB = fUpperTail(t.fAB, t.dfAB, t.dfE); }
    }
}
Outcome AnovaTask::outcome() const {
    const AnovaTable& t = table_;
    Outcome o; o.title = t.twoWay ? "Two-Way ANOVA" : "One-Way ANOVA";
    addRow(o, "A  df", t.dfA, VAdf); addRow(o, "A  SS", t.ssA, VSSa); addRow(o, "A  MS", t.msA, VMSa); addRow(o, "A  F", t.fA, VFa); addRow(o, "A  p", t.pA, VPa);
    if (t.twoWay) {
        addRow(o, "B  df", t.dfB, VBdf); addRow(o, "B  SS", t.ssB, VSSb); addRow(o, "B  MS", t.msB, VMSb); addRow(o, "B  F", t.fB, VFb); addRow(o, "B  p", t.pB, VPb);
        if (t.interaction) { addRow(o, "AB df", t.dfAB, VABdf); addRow(o, "AB SS", t.ssAB, VSSab); addRow(o, "AB MS", t.msAB, VMSab); addRow(o, "AB F", t.fAB, VFab); addRow(o, "AB p", t.pAB, VPab); }
    }
    addRow(o, "ERR df", t.dfE, VEdf); addRow(o, "ERR SS", t.ssE, VSSe); addRow(o, "ERR MS", t.msE, VMSe);
    o.curve = Curve::F; o.df1 = t.dfA; o.df2 = t.dfE; o.stat = t.fA; o.tail = 1;
    return o;
}
Outcome anova(const std::vector<double>& a, const std::vector<double>& b, const std::vector<double>& y) {
    AnovaTask t(a, b, y);
    step::Budget unlimited;
    while (!t.step(unlimited)) {}
    return t.outcome();
}

// ---------------------------------------------------------------- grafici
const char* graphTypeName(GraphType t) noexcept {
    static const char* const n[] = {"Scatter", "xyLine", "NPPlot", "Pie", "Hist", "MedBox", "N-Dist", "Broken", "X", "Med", "X\xC2\xB2", "X\xC2\xB3",
                                    "X\xE2\x81\xB4", "Log", "Exp(a\xC2\xB7" "e^bx)", "Exp(a\xC2\xB7" "b^x)", "Power", "Sin", "Logistic", "Bar"};
    const auto i = static_cast<std::size_t>(t);
    return i < static_cast<std::size_t>(GraphType::Count) ? n[i] : "?";
}
bool graphIsRegression(GraphType t) noexcept { return t >= GraphType::Linear && t <= GraphType::Logistic; }
bool graphNeedsY(GraphType t) noexcept { return t == GraphType::Scatter || t == GraphType::XYLine || graphIsRegression(t); }
Reg regressionOf(GraphType t) noexcept {
    switch (t) {
    case GraphType::MedMed: return Reg::MedMed;
    case GraphType::Quad: return Reg::Quad;
    case GraphType::Cubic: return Reg::Cubic;
    case GraphType::Quart: return Reg::Quart;
    case GraphType::Log: return Reg::Log;
    case GraphType::ExpE: return Reg::ExpE;
    case GraphType::ExpAb: return Reg::ExpAb;
    case GraphType::Power: return Reg::Power;
    case GraphType::Sin: return Reg::Sin;
    case GraphType::Logistic: return Reg::Logistic;
    default: return Reg::LinearAx;
    }
}
Histogram histogram(const std::vector<double>& x, const std::vector<double>* freq, double start, double width) {
    need(std::isfinite(start) && std::isfinite(width) && width > 0, "Argument ERROR: Width > 0");
    const Weighted w = weighted(x, freq);
    Histogram h; h.start = start; h.width = width;
    const double top = w.d.back().first;
    need(top >= start, "Argument ERROR: Start oltre il dato massimo");
    const double bins = std::floor((top - start) / width) + 1;
    need(bins <= 200, "Argument ERROR: troppe classi (Width troppo piccola)");
    h.counts.assign(static_cast<std::size_t>(bins), 0);
    for (const auto& [v, f] : w.d) {
        if (v < start) continue;
        std::size_t k = static_cast<std::size_t>(std::floor((v - start) / width));
        if (k >= h.counts.size()) k = h.counts.size() - 1;
        h.counts[k] += f;
    }
    return h;
}
BoxModel boxModel(const std::vector<double>& x, const std::vector<double>* freq, QuartileRule rule, bool outliers) {
    const OneVar o = oneVarEx(x, freq, rule);
    BoxModel b; b.min = o.min; b.q1 = o.q1; b.med = o.median; b.q3 = o.q3; b.max = o.max;
    b.lowWhisker = o.min; b.highWhisker = o.max;
    if (outliers) {
        const double iqr = o.q3 - o.q1, lo = o.q1 - 1.5 * iqr, hi = o.q3 + 1.5 * iqr;
        const Weighted w = weighted(x, freq);
        b.lowWhisker = kInf; b.highWhisker = -kInf;
        for (const auto& [v, f] : w.d) {
            (void)f;
            if (v < lo || v > hi) { if (b.outliers.empty() || b.outliers.back() != v) b.outliers.push_back(v); }
            else { b.lowWhisker = std::min(b.lowWhisker, v); b.highWhisker = std::max(b.highWhisker, v); }
        }
        if (!std::isfinite(b.lowWhisker)) { b.lowWhisker = b.q1; b.highWhisker = b.q3; }
    }
    return b;
}
void normalProbability(const std::vector<double>& x, const std::vector<double>* freq, std::vector<double>& xs, std::vector<double>& zs) {
    const Weighted w = weighted(x, freq);
    need(w.n <= 20000, "Argument ERROR: NPPlot con al piu' 20000 dati (frequenze comprese)");
    xs.clear(); zs.clear();
    const double n = w.n;
    double i = 0;
    for (const auto& [v, f] : w.d)
        for (double k = 0; k < f; ++k) { ++i; xs.push_back(v); zs.push_back(calclib::invNormCD(-1, (i - 0.5) / n, 1, 0)); }
}
std::vector<double> piePercent(const std::vector<double>& data) {
    needDim(!data.empty(), "Dimension ERROR: lista vuota");
    finiteAll(data);
    double s = 0;
    for (double d : data) { need(d >= 0, "Math ERROR: Pie con dati negativi"); s += d; }
    need(s > 0, "Math ERROR: Pie con somma nulla");
    std::vector<double> out;
    for (double d : data) out.push_back(100 * d / s);
    return out;
}

// ---------------------------------------------------------------- variabili
namespace {
const char* const kVarNames[ResultVarCount] = {
    "stat_n", "stat_meanx", "stat_sumx", "stat_sumx2", "stat_popsdx", "stat_sdx", "stat_minx", "stat_maxx",
    "stat_meany", "stat_sumy", "stat_sumy2", "stat_sumxy", "stat_popsdy", "stat_sdy", "stat_miny", "stat_maxy",
    "reg_a", "reg_b", "reg_c", "reg_d", "reg_e", "reg_r", "reg_r2", "reg_mse", "stat_q1", "stat_q3", "stat_med", "stat_mod", "stat_hstart", "stat_hpitch",
    "stat_x1", "stat_y1", "stat_x2", "stat_y2", "stat_x3", "stat_y3",
    "stat_in_n", "stat_in_mean", "stat_in_sx", "stat_in_n1", "stat_in_n2", "stat_in_mean1", "stat_in_mean2", "stat_in_sx1", "stat_in_sx2", "stat_in_sp",
    "stat_res_p", "stat_res_z", "stat_res_t", "stat_res_chi", "stat_res_f", "stat_res_phat", "stat_res_phat1", "stat_res_phat2", "stat_res_df",
    "stat_res_se", "stat_res_r", "stat_res_r2", "stat_res_pa", "stat_res_fa", "stat_res_adf", "stat_res_ssa", "stat_res_msa", "stat_res_pb",
    "stat_res_fb", "stat_res_bdf", "stat_res_ssb", "stat_res_msb", "stat_res_pab", "stat_res_fab", "stat_res_abdf", "stat_res_ssab", "stat_res_msab",
    "stat_res_edf", "stat_res_sse", "stat_res_mse",
    "stat_int_left", "stat_int_right", "stat_int_phat", "stat_int_phat1", "stat_int_phat2", "stat_int_df",
    "stat_dist_p", "stat_dist_xinv", "stat_dist_x1inv", "stat_dist_x2inv", "stat_dist_zlow", "stat_dist_zup", "stat_dist_tlow", "stat_dist_tup"};
} // namespace
const char* resultVarName(std::uint8_t v) noexcept { return v < ResultVarCount ? kVarNames[v] : "?"; }
int resultVarByName(const std::string& n) noexcept {
    for (std::size_t i = 0; i < ResultVarCount; ++i) if (n == kVarNames[i]) return static_cast<int>(i);
    return -1;
}
void oneVarVars(const OneVar& o, std::vector<std::pair<std::uint8_t, double>>& out) {
    const std::pair<std::uint8_t, double> v[] = {{VN, o.n}, {VMeanX, o.mean}, {VSumX, o.sum}, {VSumX2, o.sumSq}, {VPopSdX, o.popSd}, {VSdX, o.sampleSd},
        {VMinX, o.min}, {VMaxX, o.max}, {VQ1, o.q1}, {VQ3, o.q3}, {VMed, o.median}, {VMod, o.modes.empty() ? o.mean : o.modes.front()}};
    out.insert(out.end(), std::begin(v), std::end(v));
}
void twoVarVars(const TwoVar& t, std::vector<std::pair<std::uint8_t, double>>& out) {
    const std::pair<std::uint8_t, double> v[] = {{VN, t.n}, {VMeanX, t.meanX}, {VSumX, t.sumX}, {VSumX2, t.sumX2}, {VPopSdX, t.popSdX}, {VSdX, t.sampleSdX},
        {VMinX, t.minX}, {VMaxX, t.maxX}, {VMeanY, t.meanY}, {VSumY, t.sumY}, {VSumY2, t.sumY2}, {VSumXY, t.sumXY}, {VPopSdY, t.popSdY},
        {VSdY, t.sampleSdY}, {VMinY, t.minY}, {VMaxY, t.maxY}};
    out.insert(out.end(), std::begin(v), std::end(v));
}
void fitVars(const Fit& f, std::vector<std::pair<std::uint8_t, double>>& out) {
    for (std::size_t i = 0; i < 5; ++i) out.emplace_back(static_cast<std::uint8_t>(VRegA + i), i < f.ncoef ? f.coef[i] : 0.0);
    out.emplace_back(VRegR, f.hasR ? f.r : 0.0);
    out.emplace_back(VRegR2, f.hasR2 ? f.r2 : 0.0);
    out.emplace_back(VRegMse, f.mse);
    if (f.hasPts) for (std::size_t i = 0; i < 6; ++i) out.emplace_back(static_cast<std::uint8_t>(VX1 + i), f.pts[i]);
}

// ---------------------------------------------------------------- stato
bool StatGraph::operator==(const StatGraph& o) const noexcept {
    return type == o.type && xList == o.xList && yList == o.yList && freq == o.freq && mark == o.mark && color == o.color && colorLink == o.colorLink &&
           on == o.on && outliers == o.outliers && pieData == o.pieData && pieDisplay == o.pieDisplay && pieStore == o.pieStore && bar == o.bar &&
           stick == o.stick && hStart == o.hStart && hWidth == o.hWidth && hSet == o.hSet;
}
StatGraph defaultGraph(std::size_t index) noexcept {
    StatGraph g;
    g.on = index == 0;
    static const std::uint8_t colors[] = {0, 1, 3};             // Blue, Red, Green
    g.color = colors[index < 3 ? index : 0];
    return g;
}
const char* paletteName(std::uint8_t c) noexcept {
    static const char* const n[] = {"Blue", "Red", "Magenta", "Green", "Cyan", "Yellow", "Black"};
    return c < kPaletteColors ? n[c] : "?";
}
void PlusState::store(const std::vector<std::pair<std::uint8_t, double>>& vars) {
    for (const auto& [id, v] : vars) if (id < ResultVarCount) { results[id] = v; valid[id] = 1; }
}
bool PlusState::value(std::uint8_t v, double& out) const noexcept {
    if (v >= ResultVarCount || !valid[v]) return false;
    out = results[v];
    return true;
}
bool PlusState::operator==(const PlusState& o) const noexcept {
    return graphs == o.graphs && var1X == o.var1X && var1Freq == o.var1Freq && var2Freq == o.var2Freq && statWindManual == o.statWindManual &&
           residList == o.residList && quartiles == o.quartiles && listColor == o.listColor && listFormula == o.listFormula && results == o.results && valid == o.valid;
}
void reset(PlusState& s) { s = PlusState{}; }
void encode(const PlusState& s, std::vector<std::uint8_t>& out) {
    codec::Writer w{out};
    w.u8(1);
    for (const StatGraph& g : s.graphs) {
        w.u8(static_cast<std::uint8_t>(g.type)); w.u8(g.xList); w.u8(g.yList); w.u8(g.freq); w.u8(g.mark); w.u8(g.color); w.u8(g.colorLink);
        w.u8(static_cast<std::uint8_t>((g.on ? 1 : 0) | (g.outliers ? 2 : 0) | (g.hSet ? 4 : 0)));
        w.u8(g.pieData); w.u8(g.pieDisplay); w.u8(g.pieStore); for (std::uint8_t b : g.bar) w.u8(b); w.u8(g.stick);
        w.f64(g.hStart); w.f64(g.hWidth);
    }
    w.u8(s.var1X); w.u8(s.var1Freq); w.u8(s.var2Freq); w.u8(s.statWindManual ? 1 : 0); w.u8(s.residList); w.u8(static_cast<std::uint8_t>(s.quartiles));
    for (std::uint8_t c : s.listColor) w.u8(c);
    std::uint8_t formulas = 0; for (const auto& f : s.listFormula) if (!f.empty()) ++formulas;
    w.u8(formulas);
    for (std::size_t i = 0; i < s.listFormula.size(); ++i) if (!s.listFormula[i].empty()) { w.u8(static_cast<std::uint8_t>(i)); w.str(s.listFormula[i]); }
    std::uint16_t count = 0; for (std::uint8_t v : s.valid) if (v) ++count;
    w.u16(count);
    for (std::size_t i = 0; i < ResultVarCount; ++i) if (s.valid[i]) { w.u8(static_cast<std::uint8_t>(i)); w.f64(s.results[i]); }
}
bool decode(PlusState& out, const std::uint8_t* data, std::size_t size) {
    codec::Reader r{data, size};
    if (r.u8() != 1) return false;
    PlusState s;
    auto list = [](std::uint8_t v) { return v < 26; };
    auto optList = [](std::uint8_t v) { return v <= 26; };
    for (StatGraph& g : s.graphs) {
        const std::uint8_t type = r.u8();
        g.xList = r.u8(); g.yList = r.u8(); g.freq = r.u8(); g.mark = r.u8(); g.color = r.u8(); g.colorLink = r.u8();
        const std::uint8_t flags = r.u8();
        g.pieData = r.u8(); g.pieDisplay = r.u8(); g.pieStore = r.u8(); for (std::uint8_t& b : g.bar) b = r.u8(); g.stick = r.u8();
        g.hStart = r.f64(); g.hWidth = r.f64();
        if (!r.ok || type >= static_cast<std::uint8_t>(GraphType::Count) || !list(g.xList) || !list(g.yList) || !optList(g.freq) || g.mark >= 4 ||
            g.color >= kPaletteColors || g.colorLink >= 3 || (flags & ~7u) != 0 || !list(g.pieData) || g.pieDisplay >= 3 || !optList(g.pieStore) ||
            !optList(g.bar[0]) || !optList(g.bar[1]) || !optList(g.bar[2]) || g.stick >= 2 || !std::isfinite(g.hStart) || !std::isfinite(g.hWidth) || !(g.hWidth > 0))
            return false;
        g.type = static_cast<GraphType>(type);
        g.on = flags & 1; g.outliers = flags & 2; g.hSet = flags & 4;
    }
    s.var1X = r.u8(); s.var1Freq = r.u8(); s.var2Freq = r.u8();
    const std::uint8_t wind = r.u8();
    s.residList = r.u8();
    const std::uint8_t q = r.u8();
    if (!r.ok || !list(s.var1X) || !optList(s.var1Freq) || !optList(s.var2Freq) || wind > 1 || !optList(s.residList) || q > 1) return false;
    s.statWindManual = wind == 1; s.quartiles = static_cast<QuartileRule>(q);
    for (std::uint8_t& c : s.listColor) { c = r.u8(); if (c >= kPaletteColors) return false; }
    const std::uint8_t formulas = r.u8();
    for (std::uint8_t k = 0; k < formulas && r.ok; ++k) {
        const std::uint8_t i = r.u8();
        std::string f = r.str(kFormulaMax);
        if (!r.ok || i >= 26 || f.empty() || !s.listFormula[i].empty()) return false;
        s.listFormula[i] = std::move(f);
    }
    const std::uint16_t count = r.u16();
    for (std::uint16_t k = 0; k < count && r.ok; ++k) {
        const std::uint8_t id = r.u8();
        const double v = r.f64();
        if (!r.ok || id >= ResultVarCount || s.valid[id] || !std::isfinite(v)) return false;
        s.results[id] = v; s.valid[id] = 1;
    }
    if (!r.end()) return false;
    out = std::move(s);
    return true;
}
} // namespace fx::stats
